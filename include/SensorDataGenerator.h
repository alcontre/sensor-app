#pragma once

#include <wx/thread.h>

#include <atomic>
#include <random>
#include <string>
#include <vector>

class wxEvtHandler;

class SensorDataGenerator : public wxThread
{
 public:
   SensorDataGenerator(wxEvtHandler *target);
   virtual ~SensorDataGenerator() = default;

 protected:
   virtual ExitCode Entry() override;

 private:
   void QueueConnectionEvent(bool connected);
   void QueueNewMessageEvent();

   wxEvtHandler *m_target;
};
