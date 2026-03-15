#include "SensorDataEvent.h"

#include <utility>

wxDEFINE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

SensorDataEvent::SensorDataEvent() :
    wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE),
    m_value(0.0),
    m_thresholds(),
    m_alarmState(SensorAlarmState::Ok)
{
}

SensorDataEvent::SensorDataEvent(const std::vector<std::string> &path, const DataValue &value,
    SensorThresholds thresholds,
    SensorAlarmState alarmState) :
    wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE),
    m_path(path),
    m_value(value),
    m_thresholds(std::move(thresholds)),
    m_alarmState(alarmState)
{
}

SensorDataEvent::SensorDataEvent(const SensorDataEvent &other) :
    wxCommandEvent(other),
    m_path(other.m_path),
    m_value(other.m_value),
    m_thresholds(other.m_thresholds),
    m_alarmState(other.m_alarmState)
{
}

wxEvent *SensorDataEvent::Clone() const
{
   return new SensorDataEvent(*this);
}
