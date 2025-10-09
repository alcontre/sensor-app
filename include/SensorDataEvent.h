#pragma once
#include "SensorData.h"

#include <wx/event.h>

#include <optional>
#include <vector>

// Custom event carrying sensor data samples queued into the UI thread
wxDECLARE_EVENT(wxEVT_SENSOR_DATA_SAMPLE, wxCommandEvent);

class SensorDataEvent : public wxCommandEvent
{
 public:
   SensorDataEvent();
   SensorDataEvent(const std::vector<std::string> &path, const DataValue &value,
       std::optional<DataValue> lowerThreshold = std::nullopt,
       std::optional<DataValue> upperThreshold = std::nullopt,
       bool failed                             = false);
   SensorDataEvent(const SensorDataEvent &other);

   // Required for wxWidgets event cloning
   virtual wxEvent *Clone() const override;

   const std::vector<std::string> &GetPath() const { return m_path; }
   const DataValue &GetValue() const { return m_value; }
   const std::optional<DataValue> &GetLowerThreshold() const { return m_lowerThreshold; }
   const std::optional<DataValue> &GetUpperThreshold() const { return m_upperThreshold; }
   bool IsFailed() const { return m_failed; }

 private:
   std::vector<std::string> m_path;
   DataValue m_value;
   std::optional<DataValue> m_lowerThreshold;
   std::optional<DataValue> m_upperThreshold;
   bool m_failed;
};
