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
      dc.SetBackground(*wxWHITE_BRUSH);
      dc.Clear();

      const auto &series = m_owner->GetSeries();
      if (series.empty()) {
         dc.SetTextForeground(*wxBLACK);
         dc.DrawText("No sensors selected for plotting.", wxPoint(10, 10));
         return;
      }

      using TimedSample = Node::TimedSample;

      bool hasData    = false;
      auto earliest   = std::chrono::steady_clock::time_point::max();
      auto latest     = std::chrono::steady_clock::time_point::min();
      double minValue = std::numeric_limits<double>::infinity();
      double maxValue = -std::numeric_limits<double>::infinity();

      for (const auto &entry : series) {
         const auto &history = entry.node->GetHistory();
         if (history.empty())
            continue;

         hasData = true;
         for (const TimedSample &sample : history) {
            earliest = std::min(earliest, sample.timestamp);
            latest   = std::max(latest, sample.timestamp);
            minValue = std::min(minValue, sample.value);
            maxValue = std::max(maxValue, sample.value);
         }
      }

      if (!hasData) {
         dc.SetTextForeground(*wxBLACK);
         dc.DrawText("Waiting for numeric samples...", wxPoint(10, 10));
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
      const int rightMargin  = 20;
      const int topMargin    = 20;
      const int bottomMargin = 50;

      const wxSize size    = GetClientSize();
      const int plotWidth  = std::max(1, size.GetWidth() - leftMargin - rightMargin);
      const int plotHeight = std::max(1, size.GetHeight() - topMargin - bottomMargin);

      const wxPoint origin(leftMargin, size.GetHeight() - bottomMargin);
      const wxPoint xAxisEnd(leftMargin + plotWidth, origin.y);
      const wxPoint yAxisTop(leftMargin, topMargin);

      dc.SetPen(*wxBLACK_PEN);
      dc.DrawLine(origin, xAxisEnd);
      dc.DrawLine(origin, yAxisTop);

      const double timeRange  = std::max(1e-9, std::chrono::duration<double>(latest - earliest).count());
      const double valueRange = std::max(1e-9, maxValue - minValue);

      auto toPoint = [&](const TimedSample &sample) {
         const double tSeconds = std::chrono::duration<double>(sample.timestamp - earliest).count();
         const double xNorm    = tSeconds / timeRange;
         const double yNorm    = (sample.value - minValue) / valueRange;
         const int x           = origin.x + static_cast<int>(xNorm * plotWidth);
         const int y           = origin.y - static_cast<int>(yNorm * plotHeight);
         return wxPoint(x, y);
      };

      dc.SetPen(wxPen(wxColour(220, 220, 220), 1, wxPENSTYLE_DOT));
      for (int i = 1; i < 5; ++i) {
         const int y = origin.y - (plotHeight * i) / 5;
         dc.DrawLine(leftMargin, y, leftMargin + plotWidth, y);
      }

      wxFont baseFont = GetFont();
      wxFont boldFont = baseFont;
      boldFont.MakeBold();
      dc.SetFont(boldFont);
      dc.SetTextForeground(*wxBLACK);
      dc.DrawText("Time (s)", wxPoint(origin.x + plotWidth / 2 - 25, origin.y + 20));
      dc.DrawRotatedText("Value", wxPoint(leftMargin - 40, topMargin + plotHeight / 2 + 20), 90.0);

      dc.SetFont(baseFont);
      int legendX = leftMargin + 10;
      int legendY = topMargin + 5;

      for (const auto &entry : series) {
         const auto &history = entry.node->GetHistory();
         if (history.size() < 2)
            continue;

         dc.SetPen(wxPen(entry.colour, 2));

         bool firstPoint = true;
         wxPoint previous;
         for (const TimedSample &sample : history) {
            const wxPoint point = toPoint(sample);
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
      for (const auto &entry : series) {
         const auto &history = entry.node->GetHistory();
         if (history.empty())
            continue;
         dc.SetPen(wxPen(entry.colour, 2));
         dc.SetBrush(wxBrush(entry.colour));
         for (const TimedSample &sample : history) {
            const wxPoint point = toPoint(sample);
            dc.DrawCircle(point, 2);
         }
      }

      // Legend
      for (const auto &entry : series) {
         const wxString label = wxString::FromUTF8(entry.node->GetFullPath().c_str());
         dc.SetBrush(wxBrush(entry.colour));
         dc.SetPen(*wxTRANSPARENT_PEN);
         dc.DrawRectangle(legendX, legendY, 10, 10);
         dc.SetTextForeground(*wxBLACK);
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
    m_nextColourIndex(0)
{
   wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
   sizer->Add(m_canvas, 1, wxEXPAND);
   SetSizer(sizer);

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
