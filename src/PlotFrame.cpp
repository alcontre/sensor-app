#include "PlotFrame.h"

#include "Node.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <wx/dcbuffer.h>

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

   void OnPaint(wxPaintEvent &event)
   {
      (void)event;
      wxAutoBufferedPaintDC dc(this);
      const wxColour background(18, 22, 30);
      const wxColour textColour(235, 238, 245);
      const wxColour gridColour(70, 78, 92);

      dc.SetBackground(wxBrush(background));
      dc.Clear();

      const auto &series        = m_owner->GetSeries();
      const auto windowDuration = m_owner->GetTimeRangeDuration();
      if (series.empty()) {
         dc.SetTextForeground(textColour);
         dc.DrawText("No sensors selected for plotting.", wxPoint(10, 10));
         return;
      }

      using TimedSample = Node::TimedSample;

      auto latestOverall = std::chrono::steady_clock::time_point::min();
      for (const auto &entry : series) {
         const auto &history = entry.node->GetHistory();
         if (history.empty())
            continue;
         latestOverall = std::max(latestOverall, history.back().timestamp);
      }

      if (latestOverall == std::chrono::steady_clock::time_point::min()) {
         dc.SetTextForeground(textColour);
         dc.DrawText("Waiting for numeric samples...", wxPoint(10, 10));
         return;
      }

      auto windowStart = std::chrono::steady_clock::time_point::min();
      if (windowDuration)
         windowStart = latestOverall - *windowDuration;

      bool hasData = false;
      auto earliest = std::chrono::steady_clock::time_point::max();
      auto latest   = std::chrono::steady_clock::time_point::min();
      double minValue = std::numeric_limits<double>::infinity();
      double maxValue = -std::numeric_limits<double>::infinity();

      std::vector<std::vector<const TimedSample *>> filtered(series.size());

      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &entry   = series[idx];
         const auto &history = entry.node->GetHistory();
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

      const int leftMargin   = 60;
      const int rightMargin  = 25;
      const int topMargin    = 12;
      const int bottomMargin = 65;

      const wxSize size    = GetClientSize();
      const int plotWidth  = std::max(1, size.GetWidth() - leftMargin - rightMargin);
      const int plotHeight = std::max(1, size.GetHeight() - topMargin - bottomMargin);

      const wxPoint origin(leftMargin, size.GetHeight() - bottomMargin);
      const int plotTop = origin.y - plotHeight;

      auto plotStart = windowDuration ? (latestOverall - *windowDuration) : earliest;
      if (plotStart > latest)
         plotStart = latest;
      if (plotStart > earliest && !windowDuration)
         plotStart = earliest;
      const double timeRange  = std::max(1e-9, std::chrono::duration<double>(latest - plotStart).count());
      const double valueRange = std::max(1e-9, maxValue - minValue);

      auto toPoint = [&](const TimedSample &sample) {
         const double tSeconds = std::chrono::duration<double>(sample.timestamp - plotStart).count();
         const double xNorm    = tSeconds / timeRange;
         const double yNorm    = (sample.value - minValue) / valueRange;
         const int x           = origin.x + static_cast<int>(xNorm * plotWidth);
         const int y           = origin.y - static_cast<int>(yNorm * plotHeight);
         return wxPoint(x, y);
      };

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
         const int hours = minutes / 60;
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
      wxFont boldFont = baseFont;
      boldFont.MakeBold();

      dc.SetFont(boldFont);
      dc.SetTextForeground(textColour);
      wxString rangeLabel;
      if (windowDuration) {
         const double seconds = std::chrono::duration<double>(*windowDuration).count();
         if (seconds >= 60.0) {
            const double minutes = seconds / 60.0;
            rangeLabel = wxString::Format("Last %.1f min", minutes);
         } else {
            rangeLabel = wxString::Format("Last %.0f s", seconds);
         }
      } else {
         const double seconds = std::chrono::duration<double>(latest - earliest).count();
         rangeLabel = wxString::Format("Entire history (%.1f s span)", seconds);
      }

      const wxSize labelSize = dc.GetTextExtent(rangeLabel);
      const int rangeLabelY  = std::max(2, plotTop - labelSize.GetHeight() - 4);
      dc.DrawText(rangeLabel, wxPoint(leftMargin, rangeLabelY));

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
         dc.DrawText(label, wxPoint(x - textSz.GetWidth() / 2, origin.y + 6));
      }

      int legendX = leftMargin + 8;
      int legendY = plotTop + 24;

      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &entry     = series[idx];
         const auto &filteredHistory = filtered[idx];
         if (filteredHistory.size() < 2)
            continue;

         dc.SetPen(wxPen(entry.colour, 2));

         bool firstPoint = true;
         wxPoint previous;
         for (const TimedSample *sample : filteredHistory) {
            const wxPoint point = toPoint(*sample);
            if (firstPoint) {
               previous   = point;
               firstPoint = false;
               continue;
            }
            dc.DrawLine(previous, point);
            previous = point;
         }
      }

      // Draw markers even for single samples to confirm presence
      for (size_t idx = 0; idx < series.size(); ++idx) {
         const auto &entry            = series[idx];
         const auto &filteredHistory = filtered[idx];
         if (filteredHistory.empty())
            continue;
         dc.SetPen(wxPen(entry.colour, 2));
         dc.SetBrush(wxBrush(entry.colour));
         for (const TimedSample *sample : filteredHistory) {
            const wxPoint point = toPoint(*sample);
            dc.DrawCircle(point, 2);
         }
      }

      // Legend
      for (const auto &entry : series) {
         const wxString label = wxString::FromUTF8(entry.node->GetFullPath().c_str());
         dc.SetBrush(wxBrush(entry.colour));
         dc.SetPen(*wxTRANSPARENT_PEN);
         dc.DrawRectangle(legendX, legendY, 10, 10);
         dc.SetTextForeground(textColour);
         dc.DrawText(label, legendX + 15, legendY - 2);
         legendY += 16;
      }
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
   wxPanel *controlPanel   = new wxPanel(this, wxID_ANY);
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

   m_timer.Start(250);
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

void PlotFrame::OnTimer(wxTimerEvent &event)
{
   (void)event;
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

   auto alreadyTracked = std::find_if(m_series.begin(), m_series.end(),
       [node](const PlotSeries &series) {
          return series.node == node;
       });

   if (alreadyTracked != m_series.end())
      return false;

   if (!node->HasValue() || !node->GetValue().IsNumeric())
      return false;

   PlotSeries series{node, PickColour()};
   m_series.push_back(series);
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
