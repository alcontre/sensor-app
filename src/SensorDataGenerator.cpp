#include "SensorDataGenerator.h"
#include "SensorDataEvent.h"

#include <wx/event.h>

#include <chrono>
#include <cstdint>

SensorDataGenerator::SensorDataGenerator(std::atomic<bool> &activeFlag, wxEvtHandler *target) :
    wxThread(wxTHREAD_DETACHED),
    m_activeFlag(activeFlag),
    m_target(target),
    m_rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
{
}

wxThread::ExitCode SensorDataGenerator::Entry()
{
   while (true) {
      if (m_activeFlag.load()) {
         QueueRandomDataSample();
         wxThread::Sleep(1000);
      } else {
         wxThread::Sleep(100);
      }
   }
   return static_cast<ExitCode>(0);
}

void SensorDataGenerator::QueueRandomDataSample()
{
   if (!m_target)
      return;

   struct SampleDefinition
   {
      std::vector<std::string> path;
      enum class ValueType
      {
         Double,
         Integer,
         String
      } type;
      double minDouble;
      double maxDouble;
      std::int64_t minInteger;
      std::int64_t maxInteger;
      std::vector<std::string> stringOptions;
   };

   static const std::vector<SampleDefinition> definitions = {
       {{"Server01", "CPU", "Core0", "Temperature"}, SampleDefinition::ValueType::Double, 35.0, 65.0, 0, 0, {}},
       {{"Server01", "CPU", "Core0", "Voltage"}, SampleDefinition::ValueType::Double, 1.0, 1.2, 0, 0, {}},
       {{"Server01", "CPU", "Core0", "FanRPM"}, SampleDefinition::ValueType::Integer, 0.0, 0.0, 1200, 2400, {}},
       {{"Server01", "CPU", "Core1", "Temperature"}, SampleDefinition::ValueType::Double, 35.0, 65.0, 0, 0, {}},
       {{"Server01", "GPU", "Temperature"}, SampleDefinition::ValueType::Double, 45.0, 80.0, 0, 0, {}},
       {{"Server01", "GPU", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0, {"Running", "Idle", "Throttled"}},
       {{"Server02", "CPU", "Temperature"}, SampleDefinition::ValueType::Double, 32.0, 60.0, 0, 0, {}},
       {{"Server02", "CPU", "LoadPercent"}, SampleDefinition::ValueType::Integer, 0.0, 0.0, 0, 100, {}},
       {{"Server02", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0, {"Online", "Maintenance", "Offline"}},
       {{"Network", "Router01", "Port1", "Throughput"}, SampleDefinition::ValueType::Integer, 0.0, 0.0, 1000, 10000, {}},
       {{"Network", "Router01", "Port1", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0, {"Up", "Down", "Flapping"}},
       {{"Network", "Router01", "Port2", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0, {"Up", "Down"}}};

   if (definitions.empty())
      return;

   std::uniform_int_distribution<size_t> defDist(0, definitions.size() - 1);
   const auto &def = definitions[defDist(m_rng)];

   DataValue value = DataValue::FromInt64(0);
   switch (def.type) {
      case SampleDefinition::ValueType::Double: {
         std::uniform_real_distribution<double> valDist(def.minDouble, def.maxDouble);
         value = DataValue::FromDouble(valDist(m_rng));
         break;
      }
      case SampleDefinition::ValueType::Integer: {
         std::uniform_int_distribution<std::int64_t> valDist(def.minInteger, def.maxInteger);
         value = DataValue::FromInt64(valDist(m_rng));
         break;
      }
      case SampleDefinition::ValueType::String: {
         if (def.stringOptions.empty())
            return;
         std::uniform_int_distribution<size_t> strDist(0, def.stringOptions.size() - 1);
         value = DataValue::FromString(def.stringOptions[strDist(m_rng)]);
         break;
      }
   }

   auto *evt = new SensorDataEvent(def.path, value);
   wxQueueEvent(m_target, evt);
}
