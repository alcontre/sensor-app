#include "SensorDataGenerator.h"
#include "SensorDataEvent.h"

#include <wx/event.h>

#include <chrono>

SensorDataGenerator::SensorDataGenerator(std::atomic<bool>& activeFlag, wxEvtHandler* target)
    : wxThread(wxTHREAD_DETACHED)
    , m_activeFlag(activeFlag)
    , m_target(target)
    , m_rng(static_cast<unsigned int>(
          std::chrono::steady_clock::now().time_since_epoch().count()))
{
}

wxThread::ExitCode SensorDataGenerator::Entry()
{
    while (true)
    {
        if (m_activeFlag.load())
        {
            QueueRandomDataSample();
            wxThread::Sleep(1000);
        }
        else
        {
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
        enum class ValueType { Numeric, String } type;
        double minValue;
        double maxValue;
        std::vector<std::string> stringOptions;
    };

    static const std::vector<SampleDefinition> definitions = {
        { {"Server01", "CPU", "Core0", "Temperature"}, SampleDefinition::ValueType::Numeric, 35.0, 65.0, {} },
        { {"Server01", "CPU", "Core0", "Voltage"}, SampleDefinition::ValueType::Numeric, 1.0, 1.2, {} },
        { {"Server01", "CPU", "Core1", "Temperature"}, SampleDefinition::ValueType::Numeric, 35.0, 65.0, {} },
        { {"Server01", "GPU", "Temperature"}, SampleDefinition::ValueType::Numeric, 45.0, 80.0, {} },
        { {"Server01", "GPU", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Running", "Idle", "Throttled"} },
        { {"Server02", "CPU", "Temperature"}, SampleDefinition::ValueType::Numeric, 32.0, 60.0, {} },
        { {"Server02", "Status"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Online", "Maintenance", "Offline"} },
        { {"Network", "Router01", "Port1", "Throughput"}, SampleDefinition::ValueType::Numeric, 1000.0, 10000.0, {} },
        { {"Network", "Router01", "Port1", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Up", "Down", "Flapping"} },
        { {"Network", "Router01", "Port2", "LinkStatus"}, SampleDefinition::ValueType::String, 0.0, 0.0, {"Up", "Down"} }
    };

    if (definitions.empty())
        return;

    std::uniform_int_distribution<size_t> defDist(0, definitions.size() - 1);
    const auto& def = definitions[defDist(m_rng)];

    DataValue value(0.0);
    if (def.type == SampleDefinition::ValueType::Numeric)
    {
        std::uniform_real_distribution<double> valDist(def.minValue, def.maxValue);
        value = DataValue(valDist(m_rng));
    }
    else
    {
        if (def.stringOptions.empty())
            return;
        std::uniform_int_distribution<size_t> strDist(0, def.stringOptions.size() - 1);
        value = DataValue(def.stringOptions[strDist(m_rng)]);
    }

    auto* evt = new SensorDataEvent(def.path, value);
    wxQueueEvent(m_target, evt);
}
