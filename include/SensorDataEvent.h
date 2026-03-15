#pragma once
#include "SensorData.h"

#include <wx/event.h>

#include <vector>

// Custom event carrying sensor data samples queued into the UI thread
wxDECLARE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

class SensorDataEvent : public wxCommandEvent
{
 public:
   SensorDataEvent();
   SensorDataEvent(const std::vector<std::string> &path, const DataValue &value,
       SensorThresholds thresholds = {},
       SensorAlarmState alarmState = SensorAlarmState::Ok);
   SensorDataEvent(const SensorDataEvent &other);

   // Required for wxWidgets event cloning
   virtual wxEvent *Clone() const override;

   const std::vector<std::string> &GetPath() const { return m_path; }
   const DataValue &GetValue() const { return m_value; }
   const SensorThresholds &GetThresholds() const { return m_thresholds; }
   SensorAlarmState GetAlarmState() const { return m_alarmState; }
   bool IsWarn() const { return m_alarmState == SensorAlarmState::Warn; }
   bool IsFailed() const { return m_alarmState == SensorAlarmState::Failed; }

 private:
   std::vector<std::string> m_path;
   DataValue m_value;
   SensorThresholds m_thresholds;
   SensorAlarmState m_alarmState;
};
