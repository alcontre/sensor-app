#include "SensorData.h"
#include <sstream>
#include <stdexcept>

// DataValue implementation
DataValue::DataValue(double value)
    : m_type(NUMERIC)
    , m_numericValue(value)
{
}

DataValue::DataValue(const std::string& value)
    : m_type(STRING)
    , m_numericValue(0.0)
    , m_stringValue(value)
{
}

DataValue::DataValue(const char* value)
    : m_type(STRING)
    , m_numericValue(0.0)
    , m_stringValue(value)
{
}

double DataValue::GetNumeric() const
{
    if (m_type != NUMERIC)
        throw std::runtime_error("DataValue is not numeric");
    return m_numericValue;
}

const std::string& DataValue::GetString() const
{
    if (m_type != STRING)
        throw std::runtime_error("DataValue is not string");
    return m_stringValue;
}

std::string DataValue::GetDisplayString() const
{
    if (m_type == NUMERIC)
    {
        std::ostringstream oss;
        oss << m_numericValue;
        return oss.str();
    }
    else
    {
        return m_stringValue;
    }
}

// SensorData implementation
SensorData::SensorData(const std::vector<std::string>& path, const DataValue& value)
    : m_path(path)
    , m_value(value)
{
}

std::string SensorData::GetFullPath(const std::string& separator) const
{
    if (m_path.empty())
        return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < m_path.size(); ++i)
    {
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