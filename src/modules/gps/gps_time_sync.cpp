/**
 * @file gps_time_sync.cpp
 * @author silicon (https://github.com/siliconzz)
 * @brief GPS Time Sync
 * @version 0.1
 * @date 2026-02-10
 */

#include "gps_time_sync.h"
#include "core/display.h"
#include <globals.h>
#include <sys/time.h>
#include <time.h>

static bool already_synced_this_boot = false;

// GPS time is 18 seconds ahead of UTC (2026 value)
const time_t GPS_LEAP_OFFSET = 18;

bool sync_esp32_time_from_gps(TinyGPSPlus &gps, bool force) {
    if (!force && already_synced_this_boot) {
        padprintln("Time already synced this boot — skipping");
        return false;
    }

    if (!gps.date.isValid() || !gps.time.isValid()) {
        padprintln("GPS date or time not valid");
        return false;
    }

    if (gps.date.year() < 2020 || gps.date.year() > 2035) {
        padprintln("GPS year out of range: %d", gps.date.year());
        return false;
    }

    struct tm tm_gps = {};
    tm_gps.tm_year = gps.date.year() - 1900;
    tm_gps.tm_mon = gps.date.month() - 1;
    tm_gps.tm_mday = gps.date.day();
    tm_gps.tm_hour = gps.time.hour();
    tm_gps.tm_min = gps.time.minute();
    tm_gps.tm_sec = gps.time.second();
    tm_gps.tm_isdst = -1;

    time_t gps_epoch = mktime(&tm_gps);
    if (gps_epoch == (time_t)-1) {
        padprintln("mktime failed - invalid GPS time?");
        return false;
    }

    // Debug: show raw epoch
    // padprintf(2, "Raw GPS epoch: %ld\n", gps_epoch);

    // Set system time to raw GPS time
    struct timeval tv = {.tv_sec = gps_epoch, .tv_usec = 0};
    settimeofday(&tv, nullptr);

    // Apply Bruce timezone + DST offset
    long tz_offset_seconds = static_cast<long>(bruceConfig.tmz * 3600.0f);
    if (bruceConfig.dst) { tz_offset_seconds += 3600; }

    struct timeval local_tv;
    gettimeofday(&local_tv, nullptr);
    local_tv.tv_sec += tz_offset_seconds;
    settimeofday(&local_tv, nullptr);

    // Final correction: Add back the GPS leap offset to match UTC/local time
    // (This makes the clock agree with phone/PC)
    local_tv.tv_sec += GPS_LEAP_OFFSET;
    settimeofday(&local_tv, nullptr);

    already_synced_this_boot = true;

    /*padprintf(
        "\n  GPS time synced!\n  (tmz=%.2f, dst=%s, tz offset=%+lds, leap correction "
        "+%lds)\n",
        bruceConfig.tmz,
        bruceConfig.dst ? "on" : "off",
        tz_offset_seconds,
        GPS_LEAP_OFFSET
    );*/

    return true;
}
