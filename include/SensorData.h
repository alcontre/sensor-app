#pragma once
#include <string>
#include <vector>
#include <memory>

// Simple value holder that can contain either a number or string
class DataValue
{
public:
    enum Type { NUMERIC, STRING };
    
    // Constructors
    DataValue(double value);
    DataValue(const std::string& value);
    DataValue(const char* value);
    
    // Type checking
    Type GetType() const { return m_type; }
    bool IsNumeric() const { return m_type == NUMERIC; }
    bool IsString() const { return m_type == STRING; }
    
    // Value access
    double GetNumeric() const;
    const std::string& GetString() const;
    std::string GetDisplayString() const;

private:
    Type m_type;
    double m_numericValue;
    std::string m_stringValue;
};

// Individual data sample with hierarchical path
class SensorData
{
public:
    SensorData(const std::vector<std::string> &path, const DataValue &value);

    const std::vector<std::string> &GetPath() const { return m_path; }

    const DataValue &GetValue() const { return m_value; }

    // Path utilities
    std::string GetFullPath(const std::string& separator = "/") const;
    std::string GetLeafName() const;
    std::vector<std::string> GetParentPath() const;
    size_t GetDepth() const { return m_path.size(); }

private:
    std::vector<std::string> m_path;
    DataValue m_value;
};