#include "SensorDataJsonReader.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;

std::string DescribeEntry(size_t entryIndex)
{
   std::ostringstream oss;
   oss << "Entry " << (entryIndex + 1);
   return oss.str();
}

bool ParseScalarValue(const json &value, DataValue &parsedValue, std::string &errorMessage)
{
   if (value.is_boolean()) {
      parsedValue = DataValue(value.get<bool>());
      return true;
   }

   if (value.is_number_integer()) {
      parsedValue = DataValue(value.get<std::int64_t>());
      return true;
   }

   if (value.is_number_unsigned()) {
      const auto unsignedValue = value.get<std::uint64_t>();
      if (unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
         errorMessage = "unsigned integer exceeds supported range";
         return false;
      }

      parsedValue = DataValue(static_cast<std::int64_t>(unsignedValue));
      return true;
   }

   if (value.is_number_float()) {
      parsedValue = DataValue(value.get<double>());
      return true;
   }

   if (value.is_string()) {
      parsedValue = DataValue(value.get<std::string>());
      return true;
   }

   errorMessage = "expected a scalar JSON value";
   return false;
}

bool ParseThresholdField(const json &entry, const char *fieldName, std::optional<DataValue> &target, std::string &errorMessage)
{
   const auto thresholdIt = entry.find(fieldName);
   if (thresholdIt == entry.end()) {
      target.reset();
      return true;
   }

   DataValue parsedValue(0);
   if (!ParseScalarValue(*thresholdIt, parsedValue, errorMessage)) {
      errorMessage = std::string("field '") + fieldName + "' " + errorMessage;
      return false;
   }

   target = std::move(parsedValue);
   return true;
}

bool ParsePath(const json &entry, size_t entryIndex, std::vector<std::string> &path, std::string &errorMessage)
{
   const auto pathIt = entry.find("path");
   if (pathIt == entry.end() || !pathIt->is_array()) {
      errorMessage = DescribeEntry(entryIndex) + ": missing 'path' array";
      return false;
   }

   if (pathIt->empty()) {
      errorMessage = DescribeEntry(entryIndex) + ": 'path' array is empty";
      return false;
   }

   path.clear();
   path.reserve(pathIt->size());
   for (size_t pathIndex = 0; pathIndex < pathIt->size(); ++pathIndex) {
      const auto &segment = (*pathIt)[pathIndex];
      if (!segment.is_string()) {
         std::ostringstream oss;
         oss << DescribeEntry(entryIndex) << ": path segment " << (pathIndex + 1) << " is not a string";
         errorMessage = oss.str();
         return false;
      }

      const std::string value = segment.get<std::string>();
      if (value.empty()) {
         std::ostringstream oss;
         oss << DescribeEntry(entryIndex) << ": path segment " << (pathIndex + 1) << " is empty";
         errorMessage = oss.str();
         return false;
      }

      path.push_back(value);
   }

   return true;
}

bool ParseAlarmState(const json &entry, size_t entryIndex, SensorAlarmState &alarmState, std::string &errorMessage)
{
   const auto statusIt = entry.find("status");
   if (statusIt != entry.end()) {
      if (!statusIt->is_string()) {
         errorMessage = DescribeEntry(entryIndex) + ": field 'status' is not a string";
         return false;
      }

      const std::string status = statusIt->get<std::string>();
      if (status == "ok") {
         alarmState = SensorAlarmState::Ok;
         return true;
      }
      if (status == "warn") {
         alarmState = SensorAlarmState::Warn;
         return true;
      }
      if (status == "failed") {
         alarmState = SensorAlarmState::Failed;
         return true;
      }

      errorMessage = DescribeEntry(entryIndex) + ": field 'status' must be 'ok', 'warn', or 'failed'";
      return false;
   }

   const auto failedIt = entry.find("failed");
   if (failedIt != entry.end()) {
      if (!failedIt->is_boolean()) {
         errorMessage = DescribeEntry(entryIndex) + ": field 'failed' is not a boolean";
         return false;
      }

      alarmState = failedIt->get<bool>() ? SensorAlarmState::Failed : SensorAlarmState::Ok;
      return true;
   }

   alarmState = SensorAlarmState::Ok;
   return true;
}

bool ParseSample(const json &entry, size_t entryIndex, RecordedSensorSample &sample, std::string &errorMessage)
{
   if (!entry.is_object()) {
      errorMessage = DescribeEntry(entryIndex) + ": expected an object";
      return false;
   }

   if (!ParsePath(entry, entryIndex, sample.path, errorMessage))
      return false;

   const auto valueIt = entry.find("value");
   if (valueIt == entry.end()) {
      errorMessage = DescribeEntry(entryIndex) + ": missing 'value'";
      return false;
   }

   if (!ParseScalarValue(*valueIt, sample.value, errorMessage)) {
      errorMessage = DescribeEntry(entryIndex) + ": field 'value' " + errorMessage;
      return false;
   }

   if (!ParseThresholdField(entry, "lcr", sample.thresholds.lowerCritical, errorMessage) ||
       !ParseThresholdField(entry, "lnc", sample.thresholds.lowerNonCritical, errorMessage) ||
       !ParseThresholdField(entry, "unc", sample.thresholds.upperNonCritical, errorMessage) ||
       !ParseThresholdField(entry, "ucr", sample.thresholds.upperCritical, errorMessage)) {
      errorMessage = DescribeEntry(entryIndex) + ": " + errorMessage;
      return false;
   }

   if (!ParseAlarmState(entry, entryIndex, sample.alarmState, errorMessage))
      return false;

   return true;
}

} // namespace

bool SensorDataJsonReader::LoadFromFile(const std::string &filePath, LoadResult &result, std::string &errorMessage)
{
   result = {};
   errorMessage.clear();

   std::ifstream input(filePath);
   if (!input.is_open()) {
      errorMessage = "Unable to open recording file.";
      return false;
   }

   json document;
   try {
      input >> document;
   } catch (const json::parse_error &error) {
      errorMessage = std::string("Invalid JSON: ") + error.what();
      return false;
   }

   if (!document.is_object()) {
      errorMessage = "Recording root must be a JSON object.";
      return false;
   }

   const auto dataIt = document.find("data");
   if (dataIt == document.end() || !dataIt->is_array()) {
      errorMessage = "Recording must contain a top-level 'data' array.";
      return false;
   }

   for (size_t entryIndex = 0; entryIndex < dataIt->size(); ++entryIndex) {
      RecordedSensorSample sample{{}, DataValue(0), {}, SensorAlarmState::Ok};
      std::string warning;
      if (ParseSample((*dataIt)[entryIndex], entryIndex, sample, warning)) {
         result.samples.push_back(std::move(sample));
      } else {
         result.warnings.push_back(std::move(warning));
      }
   }

   if (result.samples.empty()) {
      if (result.warnings.empty())
         errorMessage = "Recording does not contain any samples.";
      else
         errorMessage = "Recording did not contain any valid samples.";
      return false;
   }

   return true;
}