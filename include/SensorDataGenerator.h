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
   SensorDataGenerator(std::atomic<bool> &activeFlag, wxEvtHandler *target);
   virtual ~SensorDataGenerator() = default;

 protected:
   virtual ExitCode Entry() override;

 private:
   void QueueRandomDataSample();

   std::atomic<bool> &m_activeFlag;
   wxEvtHandler *m_target;
   std::mt19937 m_rng;
};
