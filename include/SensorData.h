#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
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

   // Templated helper: deduce numeric types (integers and floating point)
   template <typename T,
       typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value, int>::type = 0>
   static DataValue From(T value)
   {
      return DataValue(value);
   }

   template <typename T,
       typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
   static DataValue From(T value)
   {
      return DataValue(static_cast<double>(value));
   }

   // Overload for bool specifically
   static DataValue From(bool value) { return DataValue(value); }

   // Overloads for strings
   static DataValue From(const std::string &value) { return DataValue(value); }
   static DataValue From(const char *value) { return DataValue(value); }

   // Value access
   std::int64_t GetInteger() const;
   bool GetBoolean() const;
   double GetDouble() const;
   double GetNumeric() const;
   const std::string &GetString() const;
   std::string GetDisplayString() const;

 private:
   e_Type m_type;
   std::variant<std::int64_t, bool, double, std::string> m_value;
};

enum class SensorAlarmState
{
   Ok,
   Warn,
   Failed
};

inline const char *ToString(SensorAlarmState state)
{
   switch (state) {
      case SensorAlarmState::Ok:
         return "ok";
      case SensorAlarmState::Warn:
         return "warn";
      case SensorAlarmState::Failed:
         return "failed";
   }

   return "ok";
}

inline bool TryParseSensorAlarmState(std::string_view text, SensorAlarmState &state)
{
   if (text == "ok") {
      state = SensorAlarmState::Ok;
      return true;
   }

   if (text == "warn") {
      state = SensorAlarmState::Warn;
      return true;
   }

   if (text == "failed") {
      state = SensorAlarmState::Failed;
      return true;
   }

   return false;
}

struct SensorThresholds
{
   std::optional<DataValue> lowerCritical;
   std::optional<DataValue> lowerNonCritical;
   std::optional<DataValue> upperNonCritical;
   std::optional<DataValue> upperCritical;

   bool HasAny() const
   {
      return lowerCritical.has_value() || lowerNonCritical.has_value() ||
             upperNonCritical.has_value() || upperCritical.has_value();
   }
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