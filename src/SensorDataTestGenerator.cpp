#include "SensorDataTestGenerator.h"

#include "MainFrame.h"
#include "SensorDataEvent.h"

#include <wx/event.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <random>

namespace {
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
   double minDouble        = 0.0;
   double maxDouble        = 0.0;
   std::int64_t minInteger = 0;
   std::int64_t maxInteger = 0;
   std::vector<std::string> stringOptions;
   std::optional<DataValue> lowerThreshold;
   std::optional<DataValue> upperThreshold;
   double failureProbability = 0.0;
   std::vector<std::string> failureStringValues;
   double booleanTrueProbability = 0.5;
   std::vector<bool> failureBooleanValues;
};

std::vector<SampleDefinition> GenerateTestSensors()
{
   std::vector<SampleDefinition> defs;
   defs.reserve(10 * 10 * 10);

   for (int i = 0; i < 10; ++i) {
      std::string cat = "Category" + std::to_string(i + 1);
      for (int j = 0; j < 10; ++j) {
         std::string sub = "SubCategory" + std::to_string(j + 1);
         for (int k = 0; k < 10; ++k) {
            int type = k % 4;
            std::string typeSuffix;
            if (type == 0)
               typeSuffix = "Double";
            else if (type == 1)
               typeSuffix = "Int";
            else if (type == 2)
               typeSuffix = "String";
            else
               typeSuffix = "Bool";

            std::string sensor = "Sensor" + std::to_string(k + 1) + "_" + typeSuffix;

            SampleDefinition def;
            def.path = {cat, sub, sensor};

            // Distribute types
            if (type == 0) { // Double
               def.type               = SampleDefinition::ValueType::Double;
               def.minDouble          = 0.0;
               def.maxDouble          = 100.0;
               def.lowerThreshold     = DataValue::FromDouble(10.0);
               def.upperThreshold     = DataValue::FromDouble(90.0);
               def.failureProbability = 0.05;
            } else if (type == 1) { // Integer
               def.type               = SampleDefinition::ValueType::Integer;
               def.minInteger         = 0;
               def.maxInteger         = 1000;
               def.lowerThreshold     = DataValue::FromInt64(100);
               def.upperThreshold     = DataValue::FromInt64(900);
               def.failureProbability = 0.05;
            } else if (type == 2) { // String
               def.type                = SampleDefinition::ValueType::String;
               def.stringOptions       = {"OK", "Warning", "Error", "Unknown"};
               def.failureStringValues = {"Error", "Unknown"};
               def.failureProbability  = 0.05;
            } else { // Boolean
               def.type                   = SampleDefinition::ValueType::Boolean;
               def.booleanTrueProbability = 0.8;
               def.failureBooleanValues   = {false};
               def.failureProbability     = 0.05;
            }

            defs.push_back(def);
         }
      }
   }
   return defs;
}
} // namespace

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
         wxThread::Sleep(10);
      } else {
         wxThread::Sleep(100);
      }
   }

   QueueConnectionEvent(false);
   return static_cast<ExitCode>(0);
}

void SensorDataTestGenerator::QueueRandomDataSample()
{
   static const std::vector<SampleDefinition> definitions = GenerateTestSensors();

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
