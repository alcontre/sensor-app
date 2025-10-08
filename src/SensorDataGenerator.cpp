#include "SensorDataGenerator.h"

#include "MainFrame.h"
#include "SensorDataEvent.h"

#include <wx/event.h>

#include <chrono>
#include <cstdint>

SensorDataGenerator::SensorDataGenerator(wxEvtHandler *target) :
    wxThread(wxTHREAD_DETACHED),
    m_target(target)
{
}

void SensorDataGenerator::QueueNewMessageEvent()
{
   auto *evt = new wxThreadEvent(wxEVT_THREAD, MainFrame::ID_NewMessage);
   wxQueueEvent(m_target, evt);
}

void SensorDataGenerator::QueueConnectionEvent(bool connected)
{
   auto *evt = new wxThreadEvent(wxEVT_THREAD,
       connected ? MainFrame::ID_ConnectYes : MainFrame::ID_ConnectNo);
   wxQueueEvent(m_target, evt);
}

wxThread::ExitCode SensorDataGenerator::Entry()
{
   while (!TestDestroy()) {
      wxThread::Sleep(100);
   }
   QueueConnectionEvent(false);
   return static_cast<ExitCode>(0);
}
