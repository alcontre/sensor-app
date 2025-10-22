#include "SensorDataJsonWriter.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <vector>

SensorDataJsonWriter::SensorDataJsonWriter(const std::string &filePath) :
    m_stream(filePath, std::ios::out | std::ios::trunc),
    m_firstEntry(true),
    m_startTime(std::chrono::steady_clock::now())
{
   if (m_stream.is_open()) {
      m_stream << "{\"data\":[";
      m_stream.flush();
   }
}

std::string SensorDataJsonWriter::GenerateTimestampedFilename()
{
   const auto now   = std::chrono::system_clock::now();
   const auto timeT = std::chrono::system_clock::to_time_t(now);
#if defined(_WIN32)
   std::tm tmBuf;
   localtime_s(&tmBuf, &timeT);
   const std::tm *timeInfo = &tmBuf;
#else
   std::tm tmBuf;
   localtime_r(&timeT, &tmBuf);
   const std::tm *timeInfo = &tmBuf;
#endif

   std::ostringstream oss;
   oss << std::put_time(timeInfo, "%Y%m%d_%H%M%S") << "_sensor.json";
   return oss.str();
}

SensorDataJsonWriter::~SensorDataJsonWriter()
{
   Close();
}

bool SensorDataJsonWriter::IsOpen() const
{
   return m_stream.is_open();
}

void SensorDataJsonWriter::RecordSample(const std::vector<std::string> &path, const DataValue &value,
    const std::optional<DataValue> &lowerThreshold,
    const std::optional<DataValue> &upperThreshold,
    bool failed)
{
   if (!m_stream.is_open())
      return;

   const auto nowSteady = std::chrono::steady_clock::now();
   const auto elapsed   = std::chrono::duration_cast<std::chrono::duration<double>>(nowSteady - m_startTime).count();

   const auto nowSystem = std::chrono::system_clock::now();

   if (m_firstEntry) {
      m_stream << '\n';
      m_firstEntry = false;
   } else {
      m_stream << ",\n";
   }

   std::ostringstream entry;
   entry << "  {\n";

   {
      std::ostringstream elapsedStream;
      elapsedStream << std::fixed << std::setprecision(6) << elapsed;
      entry << "    \"elapsed_seconds\": " << elapsedStream.str() << ",\n";
   }

   entry << "    \"local_time\": \"" << EscapeString(FormatLocalTime(nowSystem)) << "\",\n";

   entry << "    \"path\": [";
   for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0)
         entry << ", ";
      entry << "\"" << EscapeString(path[i]) << "\"";
   }
   entry << "],\n";

   entry << "    \"value\": " << FormatValue(value);

   std::vector<std::string> extraFields;
   if (lowerThreshold) {
      extraFields.emplace_back("    \"lower_threshold\": " + FormatValue(*lowerThreshold));
   }
   if (upperThreshold) {
      extraFields.emplace_back("    \"upper_threshold\": " + FormatValue(*upperThreshold));
   }
   extraFields.emplace_back(std::string("    \"failed\": ") + (failed ? "true" : "false"));

   if (!extraFields.empty()) {
      entry << ",\n";
      for (size_t i = 0; i < extraFields.size(); ++i) {
         entry << extraFields[i];
         if (i + 1 < extraFields.size())
            entry << ",\n";
         else
            entry << '\n';
      }
   } else {
      entry << '\n';
   }
   entry << "  }";

   m_stream << entry.str();
   m_stream.flush();
}

void SensorDataJsonWriter::Close()
{
   if (!m_stream.is_open())
      return;

   if (!m_firstEntry)
      m_stream << '\n';

   m_stream << "]}\n";
   m_stream.flush();
   m_stream.close();
}

std::string SensorDataJsonWriter::EscapeString(const std::string &input)
{
   std::ostringstream escaped;
   for (const unsigned char ch : input) {
      switch (ch) {
         case '\\':
            escaped << "\\\\";
            break;
         case '"':
            escaped << "\\\"";
            break;
         case '\b':
            escaped << "\\b";
            break;
         case '\f':
            escaped << "\\f";
            break;
         case '\n':
            escaped << "\\n";
            break;
         case '\r':
            escaped << "\\r";
            break;
         case '\t':
            escaped << "\\t";
            break;
         default:
            if (ch < 0x20) {
               escaped << "\\u"
                       << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                       << static_cast<int>(ch);
               escaped << std::dec << std::nouppercase << std::setfill(' ');
            } else {
               escaped << static_cast<char>(ch);
            }
      }
   }
   return escaped.str();
}

std::string SensorDataJsonWriter::FormatValue(const DataValue &value)
{
   if (value.IsString()) {
      return "\"" + EscapeString(value.GetString()) + "\"";
   }

   if (value.IsInteger()) {
      return std::to_string(value.GetInteger());
   }

   if (value.IsBoolean()) {
      return value.GetBoolean() ? "true" : "false";
   }

   std::ostringstream oss;
   oss << std::setprecision(15) << value.GetDouble();
   return oss.str();
}

std::string SensorDataJsonWriter::FormatLocalTime(const std::chrono::system_clock::time_point &tp)
{
   const auto timeT = std::chrono::system_clock::to_time_t(tp);
#if defined(_WIN32)
   std::tm tmBuf;
   localtime_s(&tmBuf, &timeT);
   const std::tm *timeInfo = &tmBuf;
#else
   std::tm tmBuf;
   localtime_r(&timeT, &tmBuf);
   const std::tm *timeInfo = &tmBuf;
#endif

   const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;

   std::ostringstream formatted;
   formatted << std::put_time(timeInfo, "%Y-%m-%dT%H:%M:%S")
             << '.' << std::setw(3) << std::setfill('0') << millis.count();
   return formatted.str();
}
