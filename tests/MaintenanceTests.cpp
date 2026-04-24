#include "PathUtils.h"
#include "SensorData.h"
#include "SensorDataJsonReader.h"
#include "SensorDataJsonWriter.h"
#include "SensorTreeModel.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Expect(bool condition, const std::string &message)
{
   if (!condition)
      throw std::runtime_error(message);
}

std::filesystem::path MakeTempPath(const std::string &suffix)
{
   const auto uniqueId = std::chrono::steady_clock::now().time_since_epoch().count();
   return std::filesystem::temp_directory_path() /
          ("sensor_app_maintenance_" + std::to_string(uniqueId) + suffix);
}

struct TempFile
{
   explicit TempFile(std::filesystem::path filePath) :
       path(std::move(filePath))
   {
   }

   ~TempFile()
   {
      std::error_code error;
      std::filesystem::remove(path, error);
   }

   std::filesystem::path path;
};

std::string ReadAll(const std::filesystem::path &filePath)
{
   std::ifstream input(filePath);
   if (!input.is_open())
      throw std::runtime_error("Unable to open file: " + filePath.string());

   return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

void TestPathRoundTrip()
{
   const std::vector<std::string> path = {"chassis", "power", "voltage"};
   const std::string joined            = PathUtils::JoinPath(path);

   Expect(joined == "chassis/power/voltage", "JoinPath should build a canonical path key");
   Expect(PathUtils::SplitPath(joined) == path, "SplitPath should invert JoinPath");
}

void TestAlarmStateStringMapping()
{
   SensorAlarmState parsedState = SensorAlarmState::Ok;
   Expect(std::string(ToString(SensorAlarmState::Warn)) == "warn", "Warn state should serialize to warn");
   Expect(TryParseSensorAlarmState("failed", parsedState), "failed should parse successfully");
   Expect(parsedState == SensorAlarmState::Failed, "failed should map to SensorAlarmState::Failed");
   Expect(!TryParseSensorAlarmState("unknown", parsedState), "Unknown alarm states should be rejected");
}

void TestUnsignedRangeCheck()
{
   const auto maxValue = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
   const DataValue inRange(maxValue);
   Expect(inRange.GetInteger() == std::numeric_limits<std::int64_t>::max(), "Maximum supported unsigned value should round-trip");

   bool didThrow = false;
   try {
      const auto overflow = maxValue + 1;
      static_cast<void>(DataValue(overflow));
   } catch (const std::out_of_range &) {
      didThrow = true;
   }

   Expect(didThrow, "Out-of-range unsigned values should throw");
}

void TestWriterOmitsFailedFieldAndPreservesWarnState()
{
   TempFile tempFile(MakeTempPath("_writer.json"));
   {
      SensorDataJsonWriter writer(tempFile.path.string());
      Expect(writer.IsOpen(), "Writer should open the output file");
      writer.RecordSample({"rack", "fan", "speed"}, DataValue(std::int64_t{4200}), {}, SensorAlarmState::Warn);
   }

   const std::string contents = ReadAll(tempFile.path);
   Expect(contents.find("\"status\": \"warn\"") != std::string::npos, "Writer should emit the status field");
   Expect(contents.find("\"failed\":") == std::string::npos, "Writer should no longer emit the failed field");

   SensorDataJsonReader::LoadResult result;
   std::string errorMessage;
   Expect(SensorDataJsonReader::LoadFromFile(tempFile.path.string(), result, errorMessage), "Reader should load files written by the writer");
   Expect(result.samples.size() == 1, "Writer round-trip should yield one sample");
   Expect(result.samples.front().alarmState == SensorAlarmState::Warn, "Warn state should survive writer/reader round-trip");
}

void TestReaderAcceptsLegacyFailedField()
{
   TempFile tempFile(MakeTempPath("_legacy.json"));
   std::ofstream output(tempFile.path);
   Expect(output.is_open(), "Legacy test file should open for writing");
   output << R"({"data":[{"path":["rack","psu"],"value":1,"failed":true}]})";
   output.close();

   SensorDataJsonReader::LoadResult result;
   std::string errorMessage;
   Expect(SensorDataJsonReader::LoadFromFile(tempFile.path.string(), result, errorMessage), "Reader should accept legacy failed-only files");
   Expect(result.samples.size() == 1, "Legacy file should yield one sample");
   Expect(result.samples.front().alarmState == SensorAlarmState::Failed, "Legacy failed flag should map to SensorAlarmState::Failed");
}

void TestModelVisibilityAndAlarmSummary()
{
   SensorTreeModel model;
   model.AddDataSample({"rack", "psu", "voltage"}, DataValue(std::int64_t{12}), {}, SensorAlarmState::Failed);
   model.AddDataSample({"rack", "fan", "speed"}, DataValue(std::int64_t{3000}), {}, SensorAlarmState::Ok);

   Node *rackNode = model.FindNodeByPath({"rack"});
   Node *psuNode  = model.FindNodeByPath({"rack", "psu"});
   Node *fanNode  = model.FindNodeByPath({"rack", "fan"});

   Expect(rackNode != nullptr, "Rack node should exist after adding samples");
   Expect(psuNode != nullptr, "PSU node should exist after adding samples");
   Expect(fanNode != nullptr, "Fan node should exist after adding samples");

   Expect(model.IsNodeVisible(rackNode), "Rack node should be visible without filters");
   Expect(model.IsNodeVisible(psuNode), "Alarmed subtree should be visible without filters");
   Expect(model.IsNodeVisible(fanNode), "Non-alarmed subtree should be visible without filters");

   wxVariant summaryValue;
   model.GetValue(summaryValue, wxDataViewItem(static_cast<void *>(rackNode)), SensorTreeModel::COL_VALUE);
   Expect(summaryValue.GetString() == "1 fail", "Collapsed container nodes should report visible descendant alarm summaries");

   model.SetShowAlarmedOnly(true);
   Expect(model.IsNodeVisible(rackNode), "Rack node should remain visible as the path to an alarmed descendant");
   Expect(model.IsNodeVisible(psuNode), "Alarmed subtree should remain visible when filtering alarmed nodes");
   Expect(!model.IsNodeVisible(fanNode), "Non-alarmed subtrees should be hidden when filtering alarmed nodes");

   wxDataViewItemArray visibleChildren;
   const unsigned int visibleChildCount = model.GetChildren(wxDataViewItem(static_cast<void *>(rackNode)), visibleChildren);
   Expect(visibleChildCount == 1, "Only the alarmed branch should remain visible under the rack node");
   Expect(static_cast<Node *>(visibleChildren[0].GetID()) == psuNode, "The remaining visible child should be the alarmed branch");

   model.SetFilter("voltage");
   Expect(model.IsNodeVisible(rackNode), "Rack node should remain visible as the path to a filtered descendant");
   Expect(model.IsNodeVisible(psuNode), "Filtered alarmed branch should remain visible");
   Expect(!model.IsNodeVisible(fanNode), "Non-matching branch should remain hidden under combined filters");
}

} // namespace

int main()
{
   try {
      TestPathRoundTrip();
      TestAlarmStateStringMapping();
      TestUnsignedRangeCheck();
      TestWriterOmitsFailedFieldAndPreservesWarnState();
      TestReaderAcceptsLegacyFailedField();
      TestModelVisibilityAndAlarmSummary();
   } catch (const std::exception &error) {
      std::cerr << error.what() << std::endl;
      return 1;
   }

   std::cout << "All maintenance tests passed." << std::endl;
   return 0;
}