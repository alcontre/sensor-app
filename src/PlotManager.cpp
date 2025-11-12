#include "PlotManager.h"

#include "Node.h"
#include "PlotFrame.h"
#include "SensorTreeModel.h"

#include <wx/window.h>

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
std::string ToUtf8(const wxString &value)
{
   const wxScopedCharBuffer buffer = value.ToUTF8();
   if (!buffer)
      return "";
   return std::string(buffer.data(), buffer.length());
}

std::vector<std::string> SplitPath(const std::string &path)
{
   std::vector<std::string> segments;
   size_t start = 0;
   while (start < path.size()) {
      size_t end = path.find('/', start);
      if (end == std::string::npos)
         end = path.size();
      if (end > start)
         segments.emplace_back(path.substr(start, end - start));
      start = end + 1;
   }
   return segments;
}

std::string JoinPath(const std::vector<std::string> &path)
{
   std::ostringstream oss;
   for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0)
         oss << '/';
      oss << path[i];
   }
   return oss.str();
}
} // namespace

PlotManager::PlotManager(wxWindow *parent, SensorTreeModel *model) :
    m_parent(parent),
    m_model(model),
    m_plots()
{
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

std::vector<PlotManager::PlotConfiguration> PlotManager::GetPlotConfigurations() const
{
   std::vector<PlotConfiguration> configs;
   configs.reserve(m_plots.size());
   for (const auto &entry : m_plots) {
      PlotConfiguration cfg;
      cfg.name               = entry.second.displayName;
      const PlotFrame *frame = entry.second.frame;
      const auto &series     = frame->GetSeries();
      cfg.sensorPaths.reserve(series.size());
      for (const auto &plotSeries : series) {
         cfg.sensorPaths.push_back(plotSeries.displayPath);
      }
      configs.push_back(std::move(cfg));
   }

   std::sort(configs.begin(), configs.end(),
       [](const PlotConfiguration &lhs, const PlotConfiguration &rhs) {
          return lhs.name.CmpNoCase(rhs.name) < 0;
       });
   return configs;
}

size_t PlotManager::RestorePlotConfigurations(const std::vector<PlotConfiguration> &configs, std::vector<wxString> &warnings)
{
   size_t plotsCreated = 0;

   for (const auto &cfg : configs) {
      if (cfg.sensorPaths.empty()) {
         warnings.push_back(wxString::Format("Plot '%s' skipped (no sensors listed).", cfg.name));
         continue;
      }

      struct SeriesRestoreEntry
      {
         std::vector<std::string> pathSegments;
         std::string displayPath;
      };

      std::vector<SeriesRestoreEntry> seriesToAdd;
      seriesToAdd.reserve(cfg.sensorPaths.size());

      std::unordered_set<std::string> seenPaths;

      for (const std::string &rawPath : cfg.sensorPaths) {
         if (rawPath.empty())
            continue;

         const std::vector<std::string> segments = SplitPath(rawPath);
         if (segments.empty())
            continue;

         const std::string normalizedPath = JoinPath(segments);
         if (!seenPaths.insert(normalizedPath).second)
            continue;

         Node *node = m_model->FindNodeByPath(segments);
         if (node) {
            if (!node->IsLeaf()) {
               warnings.push_back(wxString::Format("Plot '%s': path '%s' is not a sensor.", cfg.name, wxString::FromUTF8(rawPath.c_str())));
               continue;
            }

            seriesToAdd.push_back(SeriesRestoreEntry{node->GetPath(), node->GetFullPath()});
         } else {
            seriesToAdd.push_back(SeriesRestoreEntry{segments, rawPath});
            warnings.push_back(wxString::Format("Plot '%s': sensor '%s' not found (awaiting data).", cfg.name, wxString::FromUTF8(rawPath.c_str())));
         }
      }

      if (seriesToAdd.empty()) {
         warnings.push_back(wxString::Format("Plot '%s' skipped (no matching sensors).", cfg.name));
         continue;
      }

      const bool existed = HasPlot(cfg.name);
      std::vector<Node *> emptyNodes;
      PlotFrame *frame = CreatePlot(cfg.name, emptyNodes);
      if (!frame)
         continue;

      for (auto &entry : seriesToAdd) {
         frame->AddSensorPath(std::move(entry.pathSegments), std::move(entry.displayPath));
      }

      if (!existed && frame)
         ++plotsCreated;
   }

   return plotsCreated;
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

   for (auto *frame : frames) {
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
