#pragma once

#include <wx/thread.h>

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
