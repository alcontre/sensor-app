#pragma once

#include "SensorData.h"

#include <string>
#include <vector>

struct RecordedSensorSample
{
   std::vector<std::string> path;
   DataValue value;
   SensorThresholds thresholds;
   SensorAlarmState alarmState = SensorAlarmState::Ok;
   double elapsedSeconds       = 0.0;
};

class SensorDataJsonReader
{
 public:
   struct LoadResult
   {
      std::vector<RecordedSensorSample> samples;
      std::vector<std::string> warnings;
   };

   static bool LoadFromFile(const std::string &filePath, LoadResult &result, std::string &errorMessage);
};