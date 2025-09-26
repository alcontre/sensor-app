#include "SensorDataEvent.h"

wxDEFINE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

SensorDataEvent::SensorDataEvent() :
    wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE),
    m_value(0.0)
{
}

SensorDataEvent::SensorDataEvent(const std::vector<std::string> &path, const DataValue &value) :
    wxCommandEvent(wxEVT_SENSOR_DATA_SAMPLE),
    m_path(path),
    m_value(value)
{
}

SensorDataEvent::SensorDataEvent(const SensorDataEvent &other) :
    wxCommandEvent(other),
    m_path(other.m_path),
    m_value(other.m_value)
{
}

wxEvent *SensorDataEvent::Clone() const
{
   return new SensorDataEvent(*this);
}
