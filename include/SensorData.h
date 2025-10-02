#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// Simple value holder that can contain either a number or string
class DataValue
{
public:
    enum Type { INTEGER, DOUBLE, STRING };
    
    // Constructors
    DataValue(std::int64_t value);
    DataValue(int value);
    DataValue(double value);
    DataValue(const std::string& value);
    DataValue(const char* value);
    
    // Type checking
    Type GetType() const { return m_type; }
    bool IsInteger() const { return m_type == INTEGER; }
    bool IsDouble() const { return m_type == DOUBLE; }
    bool IsString() const { return m_type == STRING; }
    bool IsNumeric() const { return m_type == INTEGER || m_type == DOUBLE; }
    
    // Value access
    std::int64_t GetInteger() const;
    double GetDouble() const;
    double GetNumeric() const;
    const std::string& GetString() const;
    std::string GetDisplayString() const;

private:
    Type m_type;
    std::int64_t m_integerValue;
    double m_doubleValue;
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