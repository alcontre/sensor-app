#include "SensorDataTestGenerator.h"

#include "MainFrame.h"
#include "SensorDataEvent.h"

#include <wx/event.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <random>

SensorDataTestGenerator::SensorDataTestGenerator(std::atomic<bool> &activeFlag, wxEvtHandler *target) :
    wxThread(wxTHREAD_DETACHED),
    m_activeFlag(activeFlag),
    m_target(target),
    m_rng(static_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count()))
{
}

wxThread::ExitCode SensorDataTestGenerator::Entry()
{
   QueueConnectionEvent(m_activeFlag.load());

   bool lastActive = m_activeFlag.load();
   while (!TestDestroy()) {
      bool isActive = m_activeFlag.load();
      if (isActive != lastActive) {
         QueueConnectionEvent(isActive);
         lastActive = isActive;
      }

      if (isActive) {
         QueueRandomDataSample();
         wxThread::Sleep(100);
      } else {
         wxThread::Sleep(100);
      }
   }

   QueueConnectionEvent(false);
   return static_cast<ExitCode>(0);
}

void SensorDataTestGenerator::QueueRandomDataSample()
{
   struct SampleDefinition
   {
      std::vector<std::string> path;
      enum class ValueType
      {
         Double,
         Integer,
         String,
         Boolean
      } type;
      double minDouble;
      double maxDouble;
      std::int64_t minInteger;
      std::int64_t maxInteger;
      std::vector<std::string> stringOptions;
      std::optional<DataValue> lowerThreshold;
      std::optional<DataValue> upperThreshold;
      double failureProbability;
      std::vector<std::string> failureStringValues;
      double booleanTrueProbability = 0.5;
      std::vector<bool> failureBooleanValues;
   };

   static const std::vector<SampleDefinition> definitions = {
       {{"Server01", "CPU", "Core0", "Temperature"}, SampleDefinition::ValueType::Double, 35.0, 65.0, 0, 0, {},
           DataValue::FromDouble(32.0), DataValue::FromDouble(72.0), 0.1, {}},
       {{"Server01", "CPU", "Core0", "Voltage"}, SampleDefinition::ValueType::Double, 1.0, 1.2, 0, 0, {},
           DataValue::FromDouble(0.9), DataValue::FromDouble(1.25), 0.08, {}},
       {{"Server01", "CPU", "Core0", "FanRPM"}, SampleDefinition::ValueType::Integer, 0.0, 0.0, 1200, 2400, {},
           DataValue::FromInt64(1000), DataValue::FromInt64(2600), 0.12, {}},
       {{"Server01", "CPU", "Core1", "Temperature"}, SampleDefinition::ValueType::Double, 35.0, 65.0, 0, 0, {},
           DataValue::FromDouble(32.0), DataValue::FromDouble(72.0), 0.1, {}},
       {{"Server01", "GPU", "Temperature"}, SampleDefinition::ValueType::Double, 45.0, 80.0, 0, 0, {},
           DataValue::FromDouble(40.0), DataValue::FromDouble(85.0), 0.12, {}},
       {{"Server01", "GPU", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0,
           {"Running", "Idle", "Throttled"}, std::nullopt, std::nullopt, 0.15, {"Throttled"}},
       {{"Server02", "CPU", "Temperature"}, SampleDefinition::ValueType::Double, 32.0, 60.0, 0, 0, {},
           DataValue::FromDouble(28.0), DataValue::FromDouble(68.0), 0.1, {}},
       {{"Server02", "CPU", "LoadPercent"}, SampleDefinition::ValueType::Integer, 0.0, 0.0, 0, 100, {},
           DataValue::FromInt64(0), DataValue::FromInt64(95), 0.1, {}},
       {{"Server02", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0,
           {"Online", "Maintenance", "Offline"}, std::nullopt, std::nullopt, 0.2, {"Offline"}},
       {{"Network", "Router01", "Port1", "Throughput"}, SampleDefinition::ValueType::Integer, 0.0, 0.0, 1000, 10000, {},
           DataValue::FromInt64(1500), DataValue::FromInt64(9000), 0.1, {}},
       {{"Network", "Router01", "Port1", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0,
           {"Up", "Down", "Flapping"}, std::nullopt, std::nullopt, 0.25, {"Down", "Flapping"}},
      {{"Network", "Router01", "Port2", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, 0, 0,
         {"Up", "Down"}, std::nullopt, std::nullopt, 0.2, {"Down"}},
      {{"Server01", "Power", "IsRedundant"}, SampleDefinition::ValueType::Boolean, 0.0, 0.0, 0, 0, {}, std::nullopt,
         std::nullopt, 0.05, {}, 0.85, {false}},
      {{"Network", "Firewall", "FailoverActive"}, SampleDefinition::ValueType::Boolean, 0.0, 0.0, 0, 0, {}, std::nullopt,
         std::nullopt, 0.1, {}, 0.1, {true}}};

   if (definitions.empty())
      return;

   std::uniform_int_distribution<size_t> defDist(0, definitions.size() - 1);
   const auto &def = definitions[defDist(m_rng)];

   DataValue value     = DataValue::FromInt64(0);
   bool failed         = false;
   auto lowerThreshold = def.lowerThreshold;
   auto upperThreshold = def.upperThreshold;

   switch (def.type) {
      case SampleDefinition::ValueType::Double: {
         std::uniform_real_distribution<double> valDist(def.minDouble, def.maxDouble);
         double generated = valDist(m_rng);

         bool induceFailure = def.failureProbability > 0.0 && (lowerThreshold || upperThreshold) && std::bernoulli_distribution(def.failureProbability)(m_rng);
         if (induceFailure) {
            const double range = std::max(1.0, def.maxDouble - def.minDouble);
            if (lowerThreshold && (!upperThreshold || std::bernoulli_distribution(0.5)(m_rng))) {
               generated = lowerThreshold->GetNumeric() - range * 0.2;
            } else if (upperThreshold) {
               generated = upperThreshold->GetNumeric() + range * 0.2;
            }
         }

         value          = DataValue::FromDouble(generated);
         double numeric = generated;
         if (lowerThreshold && numeric < lowerThreshold->GetNumeric())
            failed = true;
         if (upperThreshold && numeric > upperThreshold->GetNumeric())
            failed = true;
         break;
      }
      case SampleDefinition::ValueType::Integer: {
         std::uniform_int_distribution<std::int64_t> valDist(def.minInteger, def.maxInteger);
         std::int64_t generated = valDist(m_rng);

         bool induceFailure = def.failureProbability > 0.0 && (lowerThreshold || upperThreshold) && std::bernoulli_distribution(def.failureProbability)(m_rng);
         if (induceFailure) {
            const std::int64_t range = std::max<std::int64_t>(1, def.maxInteger - def.minInteger);
            if (lowerThreshold && (!upperThreshold || std::bernoulli_distribution(0.5)(m_rng))) {
               generated = static_cast<std::int64_t>(lowerThreshold->GetNumeric()) - std::max<std::int64_t>(1, range / 5);
            } else if (upperThreshold) {
               generated = static_cast<std::int64_t>(upperThreshold->GetNumeric()) + std::max<std::int64_t>(1, range / 5);
            }
         }

         value          = DataValue::FromInt64(generated);
         double numeric = static_cast<double>(generated);
         if (lowerThreshold && numeric < lowerThreshold->GetNumeric())
            failed = true;
         if (upperThreshold && numeric > upperThreshold->GetNumeric())
            failed = true;
         break;
      }
      case SampleDefinition::ValueType::String: {
         if (def.stringOptions.empty())
            return;

         bool induceFailure = def.failureProbability > 0.0 && !def.failureStringValues.empty() && std::bernoulli_distribution(def.failureProbability)(m_rng);
         std::string chosen;
         if (induceFailure) {
            std::uniform_int_distribution<size_t> failureDist(0, def.failureStringValues.size() - 1);
            chosen = def.failureStringValues[failureDist(m_rng)];
         } else {
            std::uniform_int_distribution<size_t> strDist(0, def.stringOptions.size() - 1);
            chosen = def.stringOptions[strDist(m_rng)];
         }

         value  = DataValue::FromString(chosen);
         failed = std::find(def.failureStringValues.begin(), def.failureStringValues.end(), chosen) != def.failureStringValues.end();
         break;
      }
      case SampleDefinition::ValueType::Boolean: {
         bool generated = std::bernoulli_distribution(def.booleanTrueProbability)(m_rng);

         bool induceFailure = def.failureProbability > 0.0 && !def.failureBooleanValues.empty() &&
                              std::bernoulli_distribution(def.failureProbability)(m_rng);
         if (induceFailure) {
            std::uniform_int_distribution<size_t> failureDist(0, def.failureBooleanValues.size() - 1);
            generated = def.failureBooleanValues[failureDist(m_rng)];
         }

         value  = DataValue::FromBool(generated);
         failed = std::any_of(def.failureBooleanValues.begin(), def.failureBooleanValues.end(),
             [generated](bool failureVal) { return failureVal == generated; });
         break;
      }
   }

   auto *evt = new SensorDataEvent(def.path, value, lowerThreshold, upperThreshold, failed);
   wxQueueEvent(m_target, evt);
   QueueNewMessageEvent();
}

void SensorDataTestGenerator::QueueConnectionEvent(bool connected)
{
   auto *evt = new wxThreadEvent(wxEVT_THREAD, connected ? ID_ConnectYes : ID_ConnectNo);
   wxQueueEvent(m_target, evt);
}

void SensorDataTestGenerator::QueueNewMessageEvent()
{
   auto *evt = new wxThreadEvent(wxEVT_THREAD, ID_NewMessage);
   wxQueueEvent(m_target, evt);
}
