#ifndef GPS_TIME_SYNC_H
#define GPS_TIME_SYNC_H

#include <TinyGPS++.h>

bool sync_esp32_time_from_gps(TinyGPSPlus &gps, bool force = false);

#endif
