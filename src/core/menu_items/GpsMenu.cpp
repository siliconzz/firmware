#include "GpsMenu.h"
#include "core/display.h"
#include "core/settings.h"
#include "core/utils.h"
#include "globals.h"
#include "modules/gps/gps_time_sync.h"
#include "modules/gps/gps_tracker.h"
#include "modules/gps/wardriving.h"
#include <math.h>

void GpsMenu::optionsMenu() {
    options = {
        {"Wardriving",    [this]() { wardrivingMenu(); }     },
#if !defined(LITE_VERSION)
        {"GPS Tracker",   [=]() { GPSTracker(); }            },
#endif
        {"GPS Time Sync", [this]() { syncTimeFromGpsMenu(); }},
        {"Config",        [this]() { configMenu(); }         },
    };
    addOptionToMainMenu();

    String txt = "GPS (" + String(bruceConfigPins.gpsBaudrate) + " bps)";
    loopOptions(options, MENU_TYPE_SUBMENU, txt.c_str());
}

void GpsMenu::syncTimeFromGpsMenu() {
    if (bruceConfigPins.gps_bus.rx == GPIO_NUM_NC || bruceConfigPins.gps_bus.tx == GPIO_NUM_NC) {
        displayError("GPS pins not set!", true);
        return;
    }

    drawMainBorderWithTitle("GPS Time Sync");
    padprintln("");
    padprintln("Enabling GPS...");
    padprintln("");

    ioExpander.turnPinOnOff(IO_EXP_GPS, HIGH);
#ifdef USE_BOOST
    PPM.enableOTG();
#endif

    padprintln("Waiting 3s for startup...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    padprintln("Listening... (needs sky view)");

    TinyGPSPlus tempGps;
    HardwareSerial tempSerial(1);

    bool rxPinReleased = false;

    if (bruceConfigPins.CC1101_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
        bruceConfigPins.NRF24_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
#if !defined(LITE_VERSION)
        bruceConfigPins.W5500_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
        bruceConfigPins.LoRa_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
#endif
        bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.gps_bus.rx)) {
        pinMode(bruceConfigPins.gps_bus.rx, INPUT);
        rxPinReleased = true;
    }

    tempSerial.begin(
        bruceConfigPins.gpsBaudrate, SERIAL_8N1, bruceConfigPins.gps_bus.rx, bruceConfigPins.gps_bus.tx
    );

    unsigned long start = millis();
    bool gotValidTime = false;

    while (millis() - start < 60000) {
        if (tempSerial.available()) { tempGps.encode(tempSerial.read()); }

        if (tempGps.date.isValid() && tempGps.time.isValid() && tempGps.date.year() >= 2020 &&
            tempGps.date.year() <= 2035) {

            padprintf(
                2,
                "GPS time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                tempGps.date.year(),
                tempGps.date.month(),
                tempGps.date.day(),
                tempGps.time.hour(),
                tempGps.time.minute(),
                tempGps.time.second()
            );

            if (sync_esp32_time_from_gps(tempGps, false)) {
                displaySuccess("Time synced!", true);

                time_t now = time(nullptr);
                struct tm *lt = localtime(&now);
                char buf[32];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
                padprintln("Local: " + String(buf));
            } else {
                padprintln("Already synced or skipped");
            }
            gotValidTime = true;
            break;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ioExpander.turnPinOnOff(IO_EXP_GPS, LOW);
#ifdef USE_BOOST
    PPM.disableOTG();
#endif

    if (!gotValidTime) { displayError("No valid time in 60s", true); }

    if (rxPinReleased) {
        if (bruceConfigPins.CC1101_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
            bruceConfigPins.NRF24_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
#if !defined(LITE_VERSION)
            bruceConfigPins.W5500_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
            bruceConfigPins.LoRa_bus.checkConflict(bruceConfigPins.gps_bus.rx) ||
#endif
            bruceConfigPins.SDCARD_bus.checkConflict(bruceConfigPins.gps_bus.rx)) {
            pinMode(bruceConfigPins.gps_bus.rx, OUTPUT);
            if (bruceConfigPins.gps_bus.rx == bruceConfigPins.CC1101_bus.cs ||
                bruceConfigPins.gps_bus.rx == bruceConfigPins.NRF24_bus.cs ||
#if !defined(LITE_VERSION)
                bruceConfigPins.gps_bus.rx == bruceConfigPins.W5500_bus.cs ||
                bruceConfigPins.gps_bus.rx == bruceConfigPins.W5500_bus.cs ||
#endif
                bruceConfigPins.gps_bus.rx == bruceConfigPins.SDCARD_bus.cs) {
                digitalWrite(bruceConfigPins.gps_bus.rx, HIGH);
            } else {
                digitalWrite(bruceConfigPins.gps_bus.rx, LOW);
            }
        }
    }

    optionsMenu();
}

void GpsMenu::wardrivingMenu() {
    options = {
        {"Scan WiFi Networks", []() { Wardriving(true, false); }},
        {"Scan BLE Devices",   []() { Wardriving(false, true); }},
        {"Scan Both",          []() { Wardriving(true, true); } },
        {"Back",               [this]() { optionsMenu(); }      },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "Wardriving");
}
void GpsMenu::configMenu() {
    options = {
        {"Baudrate", setGpsBaudrateMenu                                 },
        {"GPS Pins", [=]() { setUARTPinsMenu(bruceConfigPins.gps_bus); }},
        {"Back",     [this]() { optionsMenu(); }                        },
    };

    loopOptions(options, MENU_TYPE_SUBMENU, "GPS Config");
}

void GpsMenu::drawIcon(float scale) {
    clearIconArea();
    int radius = scale * 18;
    if (radius % 2 != 0) radius++;

    int tangentX = sqrt(radius * radius - (radius / 2 * radius / 2));
    int32_t tangentY = radius / 2;

    tft.fillCircle(iconCenterX, iconCenterY - radius / 2, radius, bruceConfig.priColor);
    tft.fillTriangle(
        iconCenterX - tangentX,
        iconCenterY - radius / 2 + tangentY,
        iconCenterX + tangentX,
        iconCenterY - radius / 2 + tangentY,
        iconCenterX,
        iconCenterY + 1.5 * radius,
        bruceConfig.priColor
    );
    tft.fillCircle(iconCenterX, iconCenterY - radius / 2, radius / 2, bruceConfig.bgColor);

    tft.drawEllipse(iconCenterX, iconCenterY + 1.5 * radius, 1.5 * radius, radius / 2, bruceConfig.priColor);
}
