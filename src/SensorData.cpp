#include "SensorData.h"

#include <limits>
#include <sstream>
#include <stdexcept>

// DataValue implementation
DataValue::DataValue(std::int64_t value) :
    m_type(INTEGER),
    m_value(static_cast<std::int64_t>(value))
{
}

DataValue::DataValue(std::uint64_t value) :
    m_type(INTEGER),
    m_value(static_cast<std::int64_t>(value))
{
   if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
      throw std::out_of_range("DataValue integer exceeds supported range");
}

DataValue::DataValue(std::int32_t value) :
    DataValue(static_cast<std::int64_t>(value))
{
}

DataValue::DataValue(std::uint32_t value) :
    DataValue(static_cast<std::uint64_t>(value))
{
}

DataValue::DataValue(bool value) :
    m_type(BOOLEAN),
    m_value(value)
{
}

DataValue::DataValue(double value) :
    m_type(DOUBLE),
    m_value(value)
{
}

DataValue::DataValue(const std::string &value) :
    m_type(STRING),
    m_value(value)
{
}

DataValue::DataValue(const char *value) :
    m_type(STRING),
    m_value(std::string(value))
{
}

std::int64_t DataValue::GetInteger() const
{
   if (m_type != INTEGER)
      throw std::runtime_error("DataValue is not integer");
   return std::get<std::int64_t>(m_value);
}

double DataValue::GetDouble() const
{
   if (m_type != DOUBLE)
      throw std::runtime_error("DataValue is not double");
   return std::get<double>(m_value);
}

double DataValue::GetNumeric() const
{
   if (m_type == DOUBLE)
      return std::get<double>(m_value);
   if (m_type == INTEGER)
      return static_cast<double>(std::get<std::int64_t>(m_value));

   throw std::runtime_error("DataValue is not numeric");
}

const std::string &DataValue::GetString() const
{
   if (m_type != STRING)
      throw std::runtime_error("DataValue is not string");
   return std::get<std::string>(m_value);
}

bool DataValue::GetBoolean() const
{
   if (m_type != BOOLEAN)
      throw std::runtime_error("DataValue is not boolean");
   return std::get<bool>(m_value);
}

std::string DataValue::GetDisplayString() const
{
   switch (m_type) {
      case INTEGER:
         return std::to_string(std::get<std::int64_t>(m_value));
      case DOUBLE: {
         std::ostringstream oss;
         oss << std::get<double>(m_value);
         return oss.str();
      }
      case BOOLEAN:
         return std::get<bool>(m_value) ? "true" : "false";
      case STRING:
         return std::get<std::string>(m_value);
      default:
         return {};
   }
}

// SensorData implementation
SensorData::SensorData(
    const std::vector<std::string> &path,
    const DataValue &value) :
    m_path(path),
    m_value(value)
{
}

std::string SensorData::GetFullPath(const std::string &separator) const
{
   if (m_path.empty())
      return "";

   std::ostringstream oss;
   for (size_t i = 0; i < m_path.size(); ++i) {
      if (i > 0)
         oss << separator;
      oss << m_path[i];
   }
   return oss.str();
}

std::string SensorData::GetLeafName() const
{
   return m_path.empty() ? "" : m_path.back();
}

std::vector<std::string> SensorData::GetParentPath() const
{
   if (m_path.empty())
      return {};

   std::vector<std::string> parent(m_path.begin(), m_path.end() - 1);
   return parent;
}