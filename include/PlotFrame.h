#pragma once
#include <wx/tglbtn.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class Node;
class SensorTreeModel;

struct PlotSeries
{
   std::vector<std::string> pathSegments;
   std::string displayPath;
   wxColour colour;
   wxPen pen;
   wxBrush brush;
};

enum class TimeRange
{
   Last20Seconds,
   Last1Minute,
   Last5Minutes,
   Last10Minutes,
   All
};

struct PlotViewportState
{
   TimeRange presetRange = TimeRange::All;
   bool followLatest     = true;
   std::optional<std::chrono::steady_clock::time_point> xMin;
   std::optional<std::chrono::steady_clock::time_point> xMax;
};

class PlotFrame : public wxFrame
{
 public:
   PlotFrame(wxWindow *parent, const wxString &title, SensorTreeModel *model);

   bool AddSensors(const std::vector<Node *> &nodes);
   bool AddSensorPath(std::vector<std::string> pathSegments, std::string displayPath);
   const wxString &GetPlotName() const { return m_title; }
   const std::vector<PlotSeries> &GetSeries() const { return m_series; }
   SensorTreeModel *GetModel() const { return m_model; }
   const PlotViewportState &GetViewportState() const { return m_viewport; }
   void SetViewportState(const PlotViewportState &viewport);
   void SetSynchronizedViewportState(const PlotViewportState &viewport);
   void SetLockAllPlotsEnabled(bool enabled);
   bool IsLockAllPlotsEnabled() const { return m_lockAllPlots; }
   void SetOnViewportChanged(std::function<void(const PlotViewportState &)> callback);
   void SetOnLockAllPlotsChanged(std::function<void(bool)> callback);
   void SetOnClosed(std::function<void()> callback);
   std::optional<std::chrono::seconds> GetTimeRangeDuration() const;

 private:
   class PlotCanvas;

   void ApplyViewportState(const PlotViewportState &viewport, bool notify);
   void ApplyLockAllPlotsState(bool locked, bool notify);
   void OnTimer(wxTimerEvent &event);
   void OnClose(wxCloseEvent &event);
   bool AppendSeries(Node *node);
   wxColour PickColour();
   void OnLockAllPlotsButton(wxCommandEvent &event);
   void OnTimeRangeButton(wxCommandEvent &event);
   void SetTimeRange(TimeRange range);
   void UpdateTimeRangeButtons();

   wxString m_title;
   SensorTreeModel *m_model;
   PlotCanvas *m_canvas;
   wxTimer m_timer;
   std::vector<PlotSeries> m_series;
   std::function<void()> m_onClosed;
   size_t m_nextColourIndex;
   struct TimeButtonEntry
   {
      int id;
      TimeRange range;
      wxToggleButton *button;
   };
   std::vector<TimeButtonEntry> m_timeButtons;
   wxToggleButton *m_lockAllButton;
   bool m_lockAllPlots;
   PlotViewportState m_viewport;
   std::function<void(const PlotViewportState &)> m_onViewportChanged;
   std::function<void(bool)> m_onLockAllPlotsChanged;
};
