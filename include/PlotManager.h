#pragma once
#include <wx/string.h>

#include <string>
#include <unordered_map>
#include <vector>

class wxWindow;
class Node;
class PlotFrame;
class SensorTreeModel;

class PlotManager
{
 public:
   PlotManager(wxWindow *parent, SensorTreeModel *model);
   ~PlotManager();

   bool HasPlot(const wxString &name) const;
   PlotFrame *CreatePlot(const wxString &name, const std::vector<Node *> &nodes);
   bool AddSensorsToPlot(const wxString &name, const std::vector<Node *> &nodes);
   std::vector<wxString> GetPlotNames() const;
   struct PlotConfiguration
   {
      wxString name;
      std::vector<std::string> sensorPaths;
   };
   std::vector<PlotConfiguration> GetPlotConfigurations() const;
   size_t RestorePlotConfigurations(const std::vector<PlotConfiguration> &configs, std::vector<wxString> &warnings);
   void CloseAllPlots();

 private:
   static std::string NormalizeName(const wxString &name);
   void HandlePlotClosed(const std::string &name);

   struct PlotEntry
   {
      wxString displayName;
      PlotFrame *frame;
   };

   wxWindow *m_parent;
   SensorTreeModel *m_model;
   std::unordered_map<std::string, PlotEntry> m_plots;
};
