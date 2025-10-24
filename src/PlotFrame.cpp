#include "PlotFrame.h"

#include "Node.h"
#include "SensorTreeModel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/geometry.h>
#include <wx/graphics.h>

class PlotFrame::PlotCanvas : public wxPanel
{
 public:
   explicit PlotCanvas(PlotFrame *owner) :
       wxPanel(owner, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
       m_owner(owner)
   {
      SetBackgroundStyle(wxBG_STYLE_PAINT);
      Bind(wxEVT_PAINT, &PlotCanvas::OnPaint, this);
      Bind(wxEVT_SIZE, &PlotCanvas::OnSize, this);
   }

 private:
   void OnSize(wxSizeEvent &event)
   {
      Refresh();
      event.Skip();
   }

   void OnPaint(wxPaintEvent &WXUNUSED(event))
   {
      wxAutoBufferedPaintDC dc(this);

      // Configure drawing surface and colours for dark-themed chart.
      const wxColour background(18, 22, 30);
      const wxColour textColour(235, 238, 245);
      const wxColour gridColour(70, 78, 92);
      const wxColour missingTextColour(160, 165, 180);

      dc.SetBackground(wxBrush(background));
      dc.Clear();

      std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
      if (!gc)
         return;

      gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
      gc->SetInterpolationQuality(wxINTERPOLATION_DEFAULT);

      const auto &series = m_owner->GetSeries();
      if (series.empty()) {
         dc.SetTextForeground(textColour);
         dc.DrawText("No sensors selected for plotting.", wxPoint(10, 10));
         return;
      }

      const int leftMargin   = 55;
      const int rightMargin  = 22;
      const int topMargin    = 10;
      const int bottomMargin = 30;

      const wxSize size    = GetClientSize();
      const int plotWidth  = std::max(1, size.GetWidth() - leftMargin - rightMargin);
      const int plotHeight = std::max(1, size.GetHeight() - topMargin - bottomMargin);

      const wxPoint origin(leftMargin, size.GetHeight() - bottomMargin);
      const int plotTop = origin.y - plotHeight;

      // Resolve each configured path to a live node in the model.
      std::vector<const Node *> resolvedNodes(series.size(), nullptr);
      bool anyResolved = false;

      const SensorTreeModel *model = m_owner->GetModel();
      for (size_t idx = 0; idx < series.size(); ++idx) {
         const PlotSeries &entry = series[idx];
         Node *node              = model->FindNodeByPath(entry.pathSegments);
         resolvedNodes[idx]      = node;
         if (node)
            anyResolved = true;
      }

      auto drawLegend = [&](int startX, int startY) {
         int legendX = startX;
         int legendY = startY;
         for (size_t idx = 0; idx < series.size(); ++idx) {
            const auto &entry         = series[idx];
            const bool missing        = !resolvedNodes[idx];
            const wxColour legendText = missing ? missingTextColour : textColour;
            const wxString baseLabel  = wxString::FromUTF8(entry.displayPath.c_str());
            wxString legendLabel      = baseLabel;
            if (missing)
               legendLabel += " (no data)";
            dc.SetBrush(wxBrush(entry.colour));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(legendX, legendY, 10, 10);
            dc.SetTextForeground(legendText);
            dc.DrawText(legendLabel, legendX + 15, legendY - 2);
            legendY += 16;
         }
      };

      if (!anyResolved) {
         dc.SetTextForeground(textColour);
         dc.DrawText("Assigned sensors are not available in the tree.", wxPoint(10, 10));
         drawLegend(10, 40);
         return;
      }

      using TimedSample = Node::TimedSample;

      // Track newest sample across all series to detect available data.
      auto latestOverall = std::chrono::steady_clock::time_point::min();
      for (size_t idx = 0; idx < series.size(); ++idx) {
         const Node *node = resolvedNodes[idx];
         if (!node)
            continue;
         const auto &history = node->GetHistory();
         if (history.empty())
            continue;
         latestOverall = std::max(latestOverall, history.back().timestamp);
      }

      if (latestOverall == std::chrono::steady_clock::time_point::min()) {
         dc.SetTextForeground(textColour);
         dc.DrawText("Waiting for numeric samples...", wxPoint(10, 10));
         drawLegend(leftMargin + 8, plotTop + 24);
         return;
      }

      const auto windowDuration = m_owner->GetTimeRangeDuration();
      const auto now            = std::chrono::steady_clock::now();

      auto windowStart = std::chrono::steady_clock::time_point::min();
      if (windowDuration)
         windowStart = now - *windowDuration;

      bool hasData    = false;
      auto earliest   = std::chrono::steady_clock::time_point::max();
      auto latest     = std::chrono::steady_clock::time_point::min();
      double minValue = std::numeric_limits<double>::infinity();
      double maxValue = -std::numeric_limits<double>::infinity();

      std::vector<std::vector<const TimedSample *>> filtered(series.size());

      // Filter samples into the chosen window while computing overall extents.
      for (size_t idx = 0; idx < series.size(); ++idx) {
         const Node *node = resolvedNodes[idx];
         if (!node)
            continue;
         const auto &history = node->GetHistory();
         if (history.empty())
            continue;

         auto &bucket = filtered[idx];
         for (const TimedSample &sample : history) {
            if (windowDuration && sample.timestamp < windowStart)
               continue;

            hasData = true;
            bucket.push_back(&sample);
            earliest = std::min(earliest, sample.timestamp);
            latest   = std::max(latest, sample.timestamp);
            minValue = std::min(minValue, sample.value);
            maxValue = std::max(maxValue, sample.value);
         }
      }

      if (!hasData) {
         dc.SetTextForeground(textColour);
         dc.DrawText("No samples in selected timescale.", wxPoint(10, 10));
         drawLegend(leftMargin + 8, plotTop + 24);
         return;
      }

      if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
         minValue = -1.0;
         maxValue = 1.0;
      }

      if (std::chrono::duration<double>(latest - earliest).count() <= 0.0) {
         latest = earliest + std::chrono::milliseconds(1);
      }

      if (minValue == maxValue) {
         minValue -= 1.0;
         maxValue += 1.0;
      }

      // Nominally the plot end is "now", but cover the corner case where a sample
      // timestamp is just beyond now.
      auto plotEnd = std::max(now, latest);

      // Compute the plot start time based on the window duration (or the earliest sample
      // when no window is set).
      auto plotStart = windowDuration ? (plotEnd - *windowDuration) : earliest;
      if (plotStart > plotEnd)
         plotStart = plotEnd - std::chrono::milliseconds(1);

      const double timeRange  = std::max(1e-9, std::chrono::duration<double>(plotEnd - plotStart).count());
      const double valueRange = std::max(1e-9, maxValue - minValue);

      // Translate a sample to device coordinates inside the plot rectangle.
      auto toPoint = [&](const TimedSample &sample) {
         const double tSeconds = std::chrono::duration<double>(sample.timestamp - plotStart).count();
         const double xNorm    = tSeconds / timeRange;
         const double yNorm    = (sample.value - minValue) / valueRange;
         const double x        = origin.x + xNorm * plotWidth;
         const double y        = origin.y - yNorm * plotHeight;
         return wxPoint2DDouble(x, y);
      };

      wxGraphicsPath gridPath = gc->CreatePath();
      const double leftX      = static_cast<double>(leftMargin);
      const double rightX     = static_cast<double>(leftMargin + plotWidth);
      const double topY       = static_cast<double>(plotTop);
      const double bottomY    = static_cast<double>(origin.y);
      // Lay out a simple grid for reference.
      for (int i = 0; i <= 5; ++i) {
         const double y = bottomY - (static_cast<double>(plotHeight) * i) / 5.0;
         gridPath.MoveToPoint(leftX, y);
         gridPath.AddLineToPoint(rightX, y);
      }
      for (int i = 0; i <= 5; ++i) {
         const double x = leftX + (static_cast<double>(plotWidth) * i) / 5.0;
         gridPath.MoveToPoint(x, bottomY);
         gridPath.AddLineToPoint(x, topY);
      }
      gc->SetPen(wxPen(gridColour, 1, wxPENSTYLE_DOT));
      gc->StrokePath(gridPath);

      auto formatSeconds = [](double seconds) {
         if (seconds < 1.0)
            return wxString::Format("%.2fs", seconds);
         if (seconds < 60.0)
            return wxString::Format("%.1fs", seconds);
         const int totalSeconds = static_cast<int>(std::round(seconds));
         const int minutes      = totalSeconds / 60;
         const int remSeconds   = totalSeconds % 60;
         if (minutes < 60)
            return wxString::Format("%dm %02ds", minutes, remSeconds);
         const int hours  = minutes / 60;
         const int remMin = minutes % 60;
         return wxString::Format("%dh %02dm", hours, remMin);
      };

      auto formatValue = [](double value) {
         const double absVal = std::fabs(value);
         if (absVal >= 1000.0)
            return wxString::Format("%.0f", value);
         if (absVal >= 100.0)
            return wxString::Format("%.1f", value);
         return wxString::Format("%.2f", value);
      };

      wxFont baseFont = GetFont();
      dc.SetFont(baseFont);
      dc.SetTextForeground(textColour);

      // Annotate value axis on the left.
      for (int i = 0; i <= 5; ++i) {
         const double fraction = static_cast<double>(i) / 5.0;
         const double value    = minValue + fraction * (maxValue - minValue);
         const wxString label  = formatValue(value);
         const int y           = origin.y - static_cast<int>(fraction * plotHeight);
         const wxSize textSz   = dc.GetTextExtent(label);
         dc.DrawText(label, wxPoint(leftMargin - textSz.GetWidth() - 6, y - textSz.GetHeight() / 2));
      }

      // Annotate time axis at the bottom.
      for (int i = 0; i <= 5; ++i) {
         const double fraction = static_cast<double>(i) / 5.0;
         const double seconds  = fraction * timeRange;
         const wxString label  = formatSeconds(seconds);
         const int x           = leftMargin + static_cast<int>(fraction * plotWidth);
         const wxSize textSz   = dc.GetTextExtent(label);
         dc.DrawText(label, wxPoint(x - textSz.GetWidth() / 2, origin.y + 4));
      }

      // Reuse a scratch buffer so we only allocate when a series exceeds prior size.
      std::vector<wxPoint2DDouble> pointCache;
      pointCache.reserve(128);
      const double markerRadius   = 2.0;
      const double markerDiameter = markerRadius * 2.0;

      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &entry           = series[idx];
         const auto &filteredHistory = filtered[idx];
         if (filteredHistory.empty())
            continue;

         // Render polylines and markers for each active series.
         pointCache.clear();
         pointCache.reserve(filteredHistory.size());
         for (const TimedSample *sample : filteredHistory) {
            pointCache.push_back(toPoint(*sample));
         }

         gc->SetPen(entry.pen);
         if (pointCache.size() >= 2) {
            wxGraphicsPath seriesPath = gc->CreatePath();
            seriesPath.MoveToPoint(pointCache.front().m_x, pointCache.front().m_y);
            for (size_t i = 1; i < pointCache.size(); ++i)
               seriesPath.AddLineToPoint(pointCache[i].m_x, pointCache[i].m_y);
            gc->StrokePath(seriesPath);
         }

         gc->SetBrush(entry.brush);
         for (const wxPoint2DDouble &point : pointCache) {
            gc->DrawEllipse(point.m_x - markerRadius, point.m_y - markerRadius, markerDiameter, markerDiameter);
         }
      }

      drawLegend(leftMargin + 8, plotTop + 24);
   }

   PlotFrame *m_owner;
};

PlotFrame::PlotFrame(wxWindow *parent, const wxString &title, SensorTreeModel *model) :
    wxFrame(parent, wxID_ANY, title, wxDefaultPosition, wxSize(640, 480)),
    m_title(title),
    m_model(model),
    m_canvas(new PlotCanvas(this)),
    m_timer(this),
    m_series(),
    m_onClosed(),
    m_nextColourIndex(0),
    m_timeButtons(),
    m_timeRange(TimeRange::All)
{
   wxPanel *controlPanel    = new wxPanel(this, wxID_ANY);
   wxBoxSizer *controlSizer = new wxBoxSizer(wxHORIZONTAL);
   controlPanel->SetSizer(controlSizer);

   wxStaticText *label = new wxStaticText(controlPanel, wxID_ANY, "Timescale:");
   controlSizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

   struct TimeOption
   {
      const char *label;
      TimeRange range;
   };

   const TimeOption options[] = {
       {"20s", TimeRange::Last20Seconds},
       {"1m", TimeRange::Last1Minute},
       {"5m", TimeRange::Last5Minutes},
       {"10m", TimeRange::Last10Minutes},
       {"All", TimeRange::All}};

   for (const auto &option : options) {
      const int id           = wxWindow::NewControlId();
      wxToggleButton *button = new wxToggleButton(controlPanel, id, option.label);
      controlSizer->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
      m_timeButtons.push_back({id, option.range, button});
      Bind(wxEVT_TOGGLEBUTTON, &PlotFrame::OnTimeRangeButton, this, id);
   }

   controlSizer->AddStretchSpacer();

   wxBoxSizer *rootSizer = new wxBoxSizer(wxVERTICAL);
   rootSizer->Add(controlPanel, 0, wxEXPAND | wxALL, 5);
   rootSizer->Add(m_canvas, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
   SetSizer(rootSizer);

   SetTimeRange(TimeRange::Last1Minute);

   m_timer.Start(100);
   Bind(wxEVT_TIMER, &PlotFrame::OnTimer, this, m_timer.GetId());
   Bind(wxEVT_CLOSE_WINDOW, &PlotFrame::OnClose, this);
}

bool PlotFrame::AddSensors(const std::vector<Node *> &nodes)
{
   bool appended = false;
   for (Node *node : nodes) {
      appended |= AppendSeries(node);
   }

   return appended;
}

void PlotFrame::SetOnClosed(std::function<void()> callback)
{
   m_onClosed = std::move(callback);
}

void PlotFrame::OnTimer(wxTimerEvent &WXUNUSED(event))
{
   m_canvas->Refresh();
}

void PlotFrame::OnClose(wxCloseEvent &event)
{
   m_timer.Stop();
   if (m_onClosed)
      m_onClosed();
   event.Skip();
}

bool PlotFrame::AppendSeries(Node *node)
{
   if (!node)
      return false;

   if (!node->IsLeaf())
      return false;

   if (node->HasValue() && !node->GetValue().IsNumeric())
      return false;

   std::vector<std::string> pathSegments = node->GetPath();
   if (pathSegments.empty())
      return false;

   return AddSensorPath(std::move(pathSegments), node->GetFullPath());
}

bool PlotFrame::AddSensorPath(std::vector<std::string> pathSegments, std::string displayPath)
{
   if (pathSegments.empty())
      return false;

   auto alreadyTracked = std::find_if(m_series.begin(), m_series.end(),
       [&pathSegments](const PlotSeries &series) {
          return series.pathSegments == pathSegments;
       });

   if (alreadyTracked != m_series.end())
      return false;

   if (displayPath.empty()) {
      displayPath.reserve(pathSegments.size() * 8);
      for (size_t idx = 0; idx < pathSegments.size(); ++idx) {
         if (idx > 0)
            displayPath.push_back('/');
         displayPath.append(pathSegments[idx]);
      }
   }

   PlotSeries series;
   series.pathSegments = std::move(pathSegments);
   series.displayPath  = std::move(displayPath);
   series.colour       = PickColour();
   series.pen          = wxPen(series.colour, 2);
   series.pen.SetCap(wxCAP_ROUND);
   series.pen.SetJoin(wxJOIN_ROUND);
   series.brush = wxBrush(series.colour);
   m_series.push_back(std::move(series));

   return true;
}

wxColour PlotFrame::PickColour()
{
   static const std::array<wxColour, 8> kPalette = {
       wxColour(57, 106, 177),
       wxColour(218, 124, 48),
       wxColour(62, 150, 81),
       wxColour(204, 37, 41),
       wxColour(148, 103, 189),
       wxColour(255, 187, 120),
       wxColour(140, 86, 75),
       wxColour(31, 119, 180)};

   wxColour colour;
   if (m_nextColourIndex < kPalette.size()) {
      colour = kPalette[m_nextColourIndex];
   } else {
      const int base = static_cast<int>(m_nextColourIndex);
      colour         = wxColour((base * 47) % 200 + 30, (base * 67) % 200 + 30, (base * 89) % 200 + 30);
   }

   ++m_nextColourIndex;
   return colour;
}

void PlotFrame::OnTimeRangeButton(wxCommandEvent &event)
{
   for (const auto &entry : m_timeButtons) {
      if (entry.id == event.GetId()) {
         SetTimeRange(entry.range);
         break;
      }
   }
}

void PlotFrame::SetTimeRange(TimeRange range)
{
   if (m_timeRange == range) {
      UpdateTimeRangeButtons();
   } else {
      m_timeRange = range;
      UpdateTimeRangeButtons();
      m_canvas->Refresh();
   }
}

void PlotFrame::UpdateTimeRangeButtons()
{
   for (auto &entry : m_timeButtons) {
      if (entry.button)
         entry.button->SetValue(entry.range == m_timeRange);
   }
}

std::optional<std::chrono::seconds> PlotFrame::GetTimeRangeDuration() const
{
   switch (m_timeRange) {
      case TimeRange::Last20Seconds:
         return std::chrono::seconds(20);
      case TimeRange::Last1Minute:
         return std::chrono::minutes(1);
      case TimeRange::Last5Minutes:
         return std::chrono::minutes(5);
      case TimeRange::Last10Minutes:
         return std::chrono::minutes(10);
      case TimeRange::All:
      default:
         return std::nullopt;
   }
}
