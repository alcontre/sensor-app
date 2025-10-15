#pragma once
#include <wx/timer.h>
#include <wx/wx.h>

#include <functional>
#include <vector>

class Node;
class SensorTreeModel;

struct PlotSeries
{
   const Node *node;
   wxColour colour;
};

class PlotFrame : public wxFrame
{
 public:
   PlotFrame(wxWindow *parent, const wxString &title, SensorTreeModel *model);

   bool AddSensors(const std::vector<Node *> &nodes);
   const wxString &GetPlotName() const { return m_title; }
   const std::vector<PlotSeries> &GetSeries() const { return m_series; }
   void SetOnClosed(std::function<void()> callback);

 private:
   class PlotCanvas;

   void OnTimer(wxTimerEvent &event);
   void OnClose(wxCloseEvent &event);
   bool AppendSeries(Node *node);
   wxColour PickColour();

   wxString m_title;
   SensorTreeModel *m_model;
   PlotCanvas *m_canvas;
   wxTimer m_timer;
   std::vector<PlotSeries> m_series;
   std::function<void()> m_onClosed;
   size_t m_nextColourIndex;
};
