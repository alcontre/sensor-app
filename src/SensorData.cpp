#include "SensorData.h"

#include <sstream>
#include <stdexcept>

// DataValue implementation
DataValue::DataValue(std::int64_t value) : m_type(INTEGER), m_integerValue(value), m_doubleValue(0.0)
{
}

DataValue::DataValue(int value) : DataValue(static_cast<std::int64_t>(value))
{
}

DataValue::DataValue(double value) : m_type(DOUBLE), m_integerValue(0), m_doubleValue(value)
{
}

DataValue::DataValue(const std::string &value) : m_type(STRING), m_integerValue(0), m_doubleValue(0.0), m_stringValue(value)
{
}

DataValue::DataValue(const char *value) : m_type(STRING), m_integerValue(0), m_doubleValue(0.0), m_stringValue(value)
{
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

std::string DataValue::GetDisplayString() const
{
   if (m_type == INTEGER) {
      return std::to_string(m_integerValue);
   } else if (m_type == DOUBLE) {
      std::ostringstream oss;
      oss << m_doubleValue;
      return oss.str();
   } else {
      return m_stringValue;
   }
}

// SensorData implementation
SensorData::SensorData(const std::vector<std::string> &path, const DataValue &value) : m_path(path), m_value(value)
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