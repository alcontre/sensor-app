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
      const wxColour background(18, 22, 30);
      const wxColour textColour(235, 238, 245);
      const wxColour gridColour(70, 78, 92);
      const wxColour missingTextColour(160, 165, 180);

      dc.SetBackground(wxBrush(background));
      dc.Clear();

      std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
      if (gc) {
         gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
         gc->SetInterpolationQuality(wxINTERPOLATION_DEFAULT);
      }

      const auto &series        = m_owner->GetSeries();
      const auto windowDuration = m_owner->GetTimeRangeDuration();
      const auto now            = std::chrono::steady_clock::now();

      if (series.empty()) {
         dc.SetTextForeground(textColour);
         dc.DrawText("No sensors selected for plotting.", wxPoint(10, 10));
         return;
      }

      SensorTreeModel *model = m_owner->GetModel();

      const int leftMargin   = 55;
      const int rightMargin  = 22;
      const int topMargin    = 10;
      const int bottomMargin = 30;

      const wxSize size    = GetClientSize();
      const int plotWidth  = std::max(1, size.GetWidth() - leftMargin - rightMargin);
      const int plotHeight = std::max(1, size.GetHeight() - topMargin - bottomMargin);

      const wxPoint origin(leftMargin, size.GetHeight() - bottomMargin);
      const int plotTop = origin.y - plotHeight;

      std::vector<const Node *> resolvedNodes(series.size(), nullptr);
      bool anyResolved = false;

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

      auto windowStart = std::chrono::steady_clock::time_point::min();
      if (windowDuration)
         windowStart = now - *windowDuration;

      bool hasData    = false;
      auto earliest   = std::chrono::steady_clock::time_point::max();
      auto latest     = std::chrono::steady_clock::time_point::min();
      double minValue = std::numeric_limits<double>::infinity();
      double maxValue = -std::numeric_limits<double>::infinity();

      std::vector<std::vector<const TimedSample *>> filtered(series.size());

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

      auto plotEnd = windowDuration ? now : latest;
      if (windowDuration && latest > plotEnd)
         plotEnd = latest;

      auto plotStart = windowDuration ? (plotEnd - *windowDuration) : earliest;
      if (!windowDuration && plotStart > earliest)
         plotStart = earliest;
      if (plotStart > plotEnd)
         plotStart = plotEnd - std::chrono::milliseconds(1);

      const double timeRange  = std::max(1e-9, std::chrono::duration<double>(plotEnd - plotStart).count());
      const double valueRange = std::max(1e-9, maxValue - minValue);

      auto toPoint = [&](const TimedSample &sample) {
         const double tSeconds = std::chrono::duration<double>(sample.timestamp - plotStart).count();
         const double xNorm    = tSeconds / timeRange;
         const double yNorm    = (sample.value - minValue) / valueRange;
         const double x        = origin.x + xNorm * plotWidth;
         const double y        = origin.y - yNorm * plotHeight;
         return wxPoint2DDouble(x, y);
      };

      std::vector<std::vector<wxPoint2DDouble>> projected(series.size());
      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &filteredHistory = filtered[idx];
         auto &points                = projected[idx];
         points.reserve(filteredHistory.size());
         for (const TimedSample *sample : filteredHistory) {
            points.push_back(toPoint(*sample));
         }
      }

      dc.SetPen(wxPen(gridColour, 1, wxPENSTYLE_DOT));
      for (int i = 0; i <= 5; ++i) {
         const int y = origin.y - (plotHeight * i) / 5;
         dc.DrawLine(leftMargin, y, leftMargin + plotWidth, y);
      }
      for (int i = 0; i <= 5; ++i) {
         const int x = leftMargin + (plotWidth * i) / 5;
         dc.DrawLine(x, origin.y, x, plotTop);
      }

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

      for (int i = 0; i <= 5; ++i) {
         const double fraction = static_cast<double>(i) / 5.0;
         const double value    = minValue + fraction * (maxValue - minValue);
         const wxString label  = formatValue(value);
         const int y           = origin.y - static_cast<int>(fraction * plotHeight);
         const wxSize textSz   = dc.GetTextExtent(label);
         dc.DrawText(label, wxPoint(leftMargin - textSz.GetWidth() - 6, y - textSz.GetHeight() / 2));
      }

      for (int i = 0; i <= 5; ++i) {
         const double fraction = static_cast<double>(i) / 5.0;
         const double seconds  = fraction * timeRange;
         const wxString label  = formatSeconds(seconds);
         const int x           = leftMargin + static_cast<int>(fraction * plotWidth);
         const wxSize textSz   = dc.GetTextExtent(label);
         dc.DrawText(label, wxPoint(x - textSz.GetWidth() / 2, origin.y + 4));
      }

      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &entry           = series[idx];
         const auto &filteredHistory = filtered[idx];
         const auto &points          = projected[idx];
         if (filteredHistory.size() < 2)
            continue;

         if (gc) {
            gc->SetPen(wxPen(entry.colour, 2));
            bool firstPoint = true;
            double prevX    = 0.0;
            double prevY    = 0.0;
            for (const wxPoint2DDouble &point : points) {
               if (firstPoint) {
                  prevX      = point.m_x;
                  prevY      = point.m_y;
                  firstPoint = false;
                  continue;
               }
               gc->StrokeLine(prevX, prevY, point.m_x, point.m_y);
               prevX = point.m_x;
               prevY = point.m_y;
            }
         } else {
            dc.SetPen(wxPen(entry.colour, 2));
            bool firstPoint = true;
            wxPoint previous;
            for (const wxPoint2DDouble &point : points) {
               const wxPoint rounded(static_cast<int>(std::lround(point.m_x)), static_cast<int>(std::lround(point.m_y)));
               if (firstPoint) {
                  previous   = rounded;
                  firstPoint = false;
                  continue;
               }
               dc.DrawLine(previous, rounded);
               previous = rounded;
            }
         }
      }

      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &entry           = series[idx];
         const auto &filteredHistory = filtered[idx];
         const auto &points          = projected[idx];
         if (filteredHistory.empty())
            continue;
         if (gc) {
            gc->SetPen(wxPen(entry.colour, 2));
            gc->SetBrush(wxBrush(entry.colour));
            for (const wxPoint2DDouble &point : points) {
               gc->DrawEllipse(point.m_x - 2.0, point.m_y - 2.0, 4.0, 4.0);
            }
         } else {
            dc.SetPen(wxPen(entry.colour, 2));
            dc.SetBrush(wxBrush(entry.colour));
            for (const wxPoint2DDouble &point : points) {
               const wxPoint rounded(static_cast<int>(std::lround(point.m_x)), static_cast<int>(std::lround(point.m_y)));
               dc.DrawCircle(rounded, 2);
            }
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

   if (appended)
      m_canvas->Refresh();

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

   if (!node->HasValue() || !node->GetValue().IsNumeric())
      return false;

   std::vector<std::string> pathSegments = node->GetPath();
   if (pathSegments.empty())
      return false;

   auto alreadyTracked = std::find_if(m_series.begin(), m_series.end(),
       [&pathSegments](const PlotSeries &series) {
          return series.pathSegments == pathSegments;
       });

   if (alreadyTracked != m_series.end())
      return false;

   PlotSeries series;
   series.pathSegments = std::move(pathSegments);
   series.displayPath  = node->GetFullPath();
   series.colour       = PickColour();
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
      if (m_canvas)
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
