#pragma once
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

// Simple value holder that can contain either a number or string
class DataValue
{
 public:
   enum e_Type
   {
      INTEGER,
      BOOLEAN,
      DOUBLE,
      STRING
   };

   // Constructors
   DataValue(std::int64_t value);
   DataValue(std::uint64_t value);
   DataValue(std::int32_t value);
   DataValue(std::uint32_t value);
   DataValue(bool value);
   DataValue(double value);
   DataValue(const std::string &value);
   DataValue(const char *value);

   // Type checking
   e_Type GetType() const { return m_type; }
   bool IsInteger() const { return m_type == INTEGER; }
   bool IsDouble() const { return m_type == DOUBLE; }
   bool IsString() const { return m_type == STRING; }
   bool IsBoolean() const { return m_type == BOOLEAN; }
   bool IsNumeric() const { return m_type == INTEGER || m_type == DOUBLE; }

   // Static factory helpers
   static DataValue FromInt64(std::int64_t value);
   static DataValue FromUInt64(std::uint64_t value);
   static DataValue FromInt32(std::int32_t value);
   static DataValue FromUInt32(std::uint32_t value);
   static DataValue FromBool(bool value);
   static DataValue FromDouble(double value);
   static DataValue FromString(const std::string &value);
   static DataValue FromString(const char *value);

   // Templated helper: deduce numeric types (integers and floating point)
   template <typename T,
       typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
   static DataValue From(T value)
   {
      // dispatch unsigned vs signed
      return std::is_unsigned<T>::value ? FromUInt64(static_cast<std::uint64_t>(value))
                                        : FromInt64(static_cast<std::int64_t>(value));
   }

   template <typename T,
       typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
   static DataValue From(T value)
   {
      return FromDouble(static_cast<double>(value));
   }

   // Overload for bool specifically
   static DataValue From(bool value) { return FromBool(value); }

   // Overloads for strings
   static DataValue From(const std::string &value) { return FromString(value); }
   static DataValue From(const char *value) { return FromString(value); }

   // Value access
   std::int64_t GetInteger() const;
   bool GetBoolean() const;
   double GetDouble() const;
   double GetNumeric() const;
   const std::string &GetString() const;
   std::string GetDisplayString() const;

 private:
   e_Type m_type;
   std::int64_t m_integerValue;
   double m_doubleValue;
   bool m_boolValue;
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
   std::string GetFullPath(const std::string &separator = "/") const;
   std::string GetLeafName() const;
   std::vector<std::string> GetParentPath() const;
   size_t GetDepth() const { return m_path.size(); }

 private:
   std::vector<std::string> m_path;
   DataValue m_value;
};