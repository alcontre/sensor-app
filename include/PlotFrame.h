#pragma once
#include <wx/tglbtn.h>
#include <wx/timer.h>
#include <wx/wx.h>

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

class Node;
class SensorTreeModel;

struct PlotSeries
{
   const Node *node;
   wxColour colour;
};

enum class TimeRange
{
  Last20Seconds,
  Last1Minute,
  Last5Minutes,
  Last10Minutes,
  All
};

class PlotFrame : public wxFrame
{
 public:
   PlotFrame(wxWindow *parent, const wxString &title, SensorTreeModel *model);

   bool AddSensors(const std::vector<Node *> &nodes);
   const wxString &GetPlotName() const { return m_title; }
   const std::vector<PlotSeries> &GetSeries() const { return m_series; }
   void SetOnClosed(std::function<void()> callback);
  std::optional<std::chrono::seconds> GetTimeRangeDuration() const;

 private:
   class PlotCanvas;

   void OnTimer(wxTimerEvent &event);
   void OnClose(wxCloseEvent &event);
   bool AppendSeries(Node *node);
   wxColour PickColour();
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
  TimeRange m_timeRange;
};
