#include "PlotManager.h"

#include "PlotFrame.h"

#include <wx/window.h>

#include <algorithm>
#include <vector>

namespace {
std::string ToUtf8(const wxString &value)
{
   const wxScopedCharBuffer buffer = value.ToUTF8();
   if (!buffer)
      return std::string();
   return std::string(buffer.data(), buffer.length());
}
} // namespace

PlotManager::PlotManager(wxWindow *parent, SensorTreeModel *model) :
    m_parent(parent),
    m_model(model),
    m_plots()
{
   (void)m_model; // currently unused but kept for future enhancements
}

PlotManager::~PlotManager()
{
   CloseAllPlots();
}

bool PlotManager::HasPlot(const wxString &name) const
{
   const std::string key = NormalizeName(name);
   return m_plots.find(key) != m_plots.end();
}

PlotFrame *PlotManager::CreatePlot(const wxString &name, const std::vector<Node *> &nodes)
{
   const std::string key = NormalizeName(name);
   auto existing         = m_plots.find(key);
   if (existing != m_plots.end())
      return existing->second.frame;

   PlotFrame *frame = new PlotFrame(m_parent, name, m_model);
   frame->AddSensors(nodes);
   frame->SetOnClosed([this, key]() {
      HandlePlotClosed(key);
   });
   frame->Show(true);
   frame->Raise();

   m_plots.emplace(key, PlotEntry{wxString(name), frame});
   return frame;
}

bool PlotManager::AddSensorsToPlot(const wxString &name, const std::vector<Node *> &nodes)
{
   const std::string key = NormalizeName(name);
   auto it               = m_plots.find(key);
   if (it == m_plots.end())
      return false;

   PlotFrame *frame    = it->second.frame;
   const bool appended = frame->AddSensors(nodes);
   frame->Raise();
   return appended;
}

std::vector<wxString> PlotManager::GetPlotNames() const
{
   std::vector<wxString> names;
   names.reserve(m_plots.size());
   for (const auto &entry : m_plots) {
      names.push_back(entry.second.displayName);
   }
   std::sort(names.begin(), names.end());
   return names;
}

void PlotManager::CloseAllPlots()
{
   std::vector<PlotFrame *> frames;
   frames.reserve(m_plots.size());
   for (auto &entry : m_plots) {
      PlotFrame *frame = entry.second.frame;
      if (!frame->IsBeingDeleted()) {
         frame->SetOnClosed(nullptr);
         frames.push_back(frame);
      }
   }
   m_plots.clear();

   for (PlotFrame *frame : frames) {
      frame->Destroy();
   }
}

std::string PlotManager::NormalizeName(const wxString &name)
{
   wxString trimmed = name;
   trimmed.Trim(true);
   trimmed.Trim(false);
   return ToUtf8(trimmed.Lower());
}

void PlotManager::HandlePlotClosed(const std::string &name)
{
   auto it = m_plots.find(name);
   if (it != m_plots.end())
      m_plots.erase(it);
}
