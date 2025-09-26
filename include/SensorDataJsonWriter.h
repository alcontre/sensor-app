#pragma once

#include "SensorData.h"

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

class SensorDataJsonWriter
{
 public:
   // Default constructor uses an internal default filename.
   SensorDataJsonWriter();
   explicit SensorDataJsonWriter(const std::string &filePath);
   ~SensorDataJsonWriter();

   bool IsOpen() const;

   void RecordSample(const std::vector<std::string> &path, const DataValue &value);

 private:
   void Close();

   static std::string EscapeString(const std::string &input);
   static std::string FormatValue(const DataValue &value);
   static std::string FormatLocalTime(const std::chrono::system_clock::time_point &tp);

   std::ofstream m_stream;
   bool m_firstEntry;
   std::chrono::steady_clock::time_point m_startTime;
};
