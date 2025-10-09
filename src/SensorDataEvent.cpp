#include "SensorDataEvent.h"

#include <utility>

wxDEFINE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

SensorDataEvent::SensorDataEvent() :
    wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE),
    m_value(0.0),
    m_lowerThreshold(std::nullopt),
    m_upperThreshold(std::nullopt),
    m_failed(false)
{
}

SensorDataEvent::SensorDataEvent(const std::vector<std::string> &path, const DataValue &value,
    std::optional<DataValue> lowerThreshold,
    std::optional<DataValue> upperThreshold,
    bool failed) :
    wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE),
    m_path(path),
    m_value(value),
    m_lowerThreshold(std::move(lowerThreshold)),
    m_upperThreshold(std::move(upperThreshold)),
    m_failed(failed)
{
}

SensorDataEvent::SensorDataEvent(const SensorDataEvent &other) :
    wxCommandEvent(other),
    m_path(other.m_path),
    m_value(other.m_value),
    m_lowerThreshold(other.m_lowerThreshold),
    m_upperThreshold(other.m_upperThreshold),
    m_failed(other.m_failed)
{
}

wxEvent *SensorDataEvent::Clone() const
{
   return new SensorDataEvent(*this);
}
