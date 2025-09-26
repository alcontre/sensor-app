#include "SensorData.h"

#include <sstream>
#include <stdexcept>

// DataValue implementation
DataValue::DataValue(std::int64_t value) :
    m_type(INTEGER),
    m_integerValue(static_cast<std::int64_t>(value)),
    m_doubleValue(0.0)
{
}

DataValue::DataValue(std::uint64_t value) :
    m_type(INTEGER),
    m_integerValue(static_cast<std::int64_t>(value)),
    m_doubleValue(0.0)
{
}

DataValue::DataValue(std::int32_t value) :
    DataValue(static_cast<std::int64_t>(value))
{
}

DataValue::DataValue(std::uint32_t value) :
    DataValue(static_cast<std::int64_t>(value))
{
}

DataValue::DataValue(bool value) :
    m_type(BOOLEAN),
    m_integerValue(0),
    m_doubleValue(0.0),
    m_boolValue(value)
{
}

DataValue::DataValue(double value) :
    m_type(DOUBLE),
    m_integerValue(0),
    m_doubleValue(value)
{
}

DataValue::DataValue(const std::string &value) :
    m_type(STRING),
    m_integerValue(0),
    m_doubleValue(0.0),
    m_stringValue(value)
{
}

DataValue::DataValue(const char *value) :
    m_type(STRING),
    m_integerValue(0),
    m_doubleValue(0.0),
    m_stringValue(value)
{
}

// Static factories
DataValue DataValue::FromInt64(std::int64_t value)
{
   return DataValue(value);
}

DataValue DataValue::FromUInt64(std::uint64_t value)
{
   return DataValue(value);
}

DataValue DataValue::FromInt32(std::int32_t value)
{
   return DataValue(value);
}

DataValue DataValue::FromUInt32(std::uint32_t value)
{
   return DataValue(value);
}

DataValue DataValue::FromDouble(double value)
{
   return DataValue(value);
}

DataValue DataValue::FromString(const std::string &value)
{
   return DataValue(value);
}

DataValue DataValue::FromString(const char *value)
{
   return DataValue(value);
}

DataValue DataValue::FromBool(bool value)
{
   return DataValue(value);
}

std::int64_t DataValue::GetInteger() const
{
   if (m_type != INTEGER)
      throw std::runtime_error("DataValue is not integer");
   return m_integerValue;
}

double DataValue::GetDouble() const
{
   if (m_type != DOUBLE)
      throw std::runtime_error("DataValue is not double");
   return m_doubleValue;
}

double DataValue::GetNumeric() const
{
   if (m_type == DOUBLE)
      return m_doubleValue;
   if (m_type == INTEGER)
      return static_cast<double>(m_integerValue);

   throw std::runtime_error("DataValue is not numeric");
}

const std::string &DataValue::GetString() const
{
   if (m_type != STRING)
      throw std::runtime_error("DataValue is not string");
   return m_stringValue;
}

bool DataValue::GetBoolean() const
{
   if (m_type != BOOLEAN)
      throw std::runtime_error("DataValue is not boolean");
   return m_boolValue;
}

std::string DataValue::GetDisplayString() const
{
   if (m_type == INTEGER) {
      return std::to_string(m_integerValue);
   } else if (m_type == DOUBLE) {
      std::ostringstream oss;
      oss << m_doubleValue;
      return oss.str();
   } else if (m_type == BOOLEAN) {
      return m_boolValue ? "true" : "false";
   } else {
      return m_stringValue;
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