#pragma once

#include <wx/thread.h>

#include <atomic>
#include <random>
#include <string>
#include <vector>

class wxEvtHandler;

class SensorDataTestGenerator : public wxThread
{
 public:
   SensorDataTestGenerator(std::atomic<bool> &activeFlag, wxEvtHandler *target);
   virtual ~SensorDataTestGenerator() = default;

 protected:
   virtual ExitCode Entry() override;

 private:
   void QueueRandomDataSample();
   void QueueConnectionEvent(bool connected);

   std::atomic<bool> &m_activeFlag;
   wxEvtHandler *m_target;
   std::mt19937 m_rng;
};
