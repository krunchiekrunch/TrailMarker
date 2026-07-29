// Tested on Heltec Wireless Tracker V2.3

#include "Arduino.h"
#include "HT_st7735.h"
#include "HT_TinyGPS++.h"

TinyGPSPlus GPS;
HT_st7735 st7735;

#define VGNSS_CTRL 3
#define BUTTON_PIN 0

#define BATTERY_PIN 1 
#define ADC_ATTENUATION ADC_2_5db 
#define ADC_MULTIPLIER (4.9 * 1.045)
#define ADC_CTRL 2     

// Backlight off pin
#ifndef ST7735_LED_K_Pin
#define ST7735_LED_K_Pin 21
#endif

// Screen sleep config
bool displayIsOn = true;
unsigned long lastActivityTime = 0;
const unsigned long screenTimeout = 180000; // 5 seconds timeout

struct Target {
    double lat;
    double lng;
};

// Coordinates array
Target targets[] = {
    {0, 0},
    {0, 180),
};

const int totalTargets = sizeof(targets) / sizeof(targets[0]);
int currentTargetIndex = 0;

// Tab managment
int currentScreen = 0; 

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

// Long press config
unsigned long pressStartTime = 0;
bool buttonIsPressed = false;
const unsigned long longPressTime = 500; 

// Double click config
unsigned long lastClickTime = 0;
const unsigned long doubleClickWindow = 400; 
bool waitingForDoubleClick = false;

unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 150; 

unsigned long lastBatteryCheck = 0;
const unsigned long batteryInterval = 10000; 
int cachedBatteryPct = 0;
float cachedVoltage = 0.0;

String lastDate = "";
String lastTime = "";
String lastInfo = "";
String lastSpeed = "";
String lastAltitude = "";
String lastTargetID = "";
int lastSatellites = -1;
String lastBatteryStr = "";

// Track previous values for tab 1 to prevent flashing
String lastLatStr = "";
String lastLonStr = "";
String lastScreenSpeed = "";
String lastScreenAltitude = "";

bool forceNavigationUpdate = true;
bool forceSpeedUpdate = true;
bool forceAltitudeUpdate = true;
bool forceIDUpdate = true;
bool forceCoordUpdate = true;

void wakeUpScreen() {
    displayIsOn = true;
    lastActivityTime = millis();
    
    // Turn backlight back ON
    digitalWrite(ST7735_LED_K_Pin, HIGH);
    
    // Force a full refresh of the current screen to restore contents immediately
    st7735.st7735_fill_screen(ST7735_BLACK);
    lastDate = "";
    lastTime = "";
    lastBatteryStr = "";
    lastSatellites = -1;
    lastLatStr = "";
    lastLonStr = "";
    lastScreenSpeed = "";
    lastScreenAltitude = "";
    lastTargetID = "";
    lastInfo = "";
    lastBatteryCheck = 0;

    if (currentScreen == 0) {
        forceNavigationUpdate = true;
        forceSpeedUpdate = true;
        forceAltitudeUpdate = true;
        forceIDUpdate = true;
    } else {
        forceCoordUpdate = true;
    }
}

void sleepScreen() {
    displayIsOn = false;
    st7735.st7735_fill_screen(ST7735_BLACK); // Clear buffer content
    // Turn off backlight
    digitalWrite(ST7735_LED_K_Pin, LOW);
}

void checkButton()
{
    int buttonState = digitalRead(BUTTON_PIN);
    unsigned long now = millis();

    if (buttonState != lastButtonState) {
        lastDebounceTime = now;
    }

    if ((now - lastDebounceTime) > debounceDelay) {
        if (buttonState == LOW && !buttonIsPressed) {
            buttonIsPressed = true;
            pressStartTime = now;
        }
        else if (buttonState == HIGH && buttonIsPressed) {
            buttonIsPressed = false;
            unsigned long pressDuration = now - pressStartTime;

            // Check if screen was off, this button press only wakes it up and resets sleep timer
            if (!displayIsOn) {
                wakeUpScreen();
                lastButtonState = buttonState;
                return;
            }

            // Screen is on, register activity
            lastActivityTime = now;

            if (pressDuration >= longPressTime) {
                currentScreen = (currentScreen == 0) ? 1 : 0;
                st7735.st7735_fill_screen(ST7735_BLACK);
                
                Serial.print("Switched to tab: ");
                Serial.println(currentScreen);
                
                lastDate = "";
                lastTime = "";
                lastBatteryStr = "";
                lastSatellites = -1;
                lastLatStr = "";
                lastLonStr = "";
                lastScreenSpeed = "";
                lastScreenAltitude = "";
                lastBatteryCheck = 0; 

                if (currentScreen == 0) {
                    forceNavigationUpdate = true;
                    forceSpeedUpdate = true;
                    forceAltitudeUpdate = true;
                    forceIDUpdate = true;
                    lastTargetID = ""; 
                } else {
                    forceCoordUpdate = true;
                }
            } 
            else {
                if (currentScreen == 0) {
                    if (waitingForDoubleClick && (now - lastClickTime <= doubleClickWindow)) {
                        currentTargetIndex--;
                        if (currentTargetIndex < 0) {
                            currentTargetIndex = totalTargets - 1;
                        }

                        Serial.print("Double click detected, switched back to coordinate: ");
                        Serial.println(currentTargetIndex + 1);

                        waitingForDoubleClick = false;
                        
                        forceNavigationUpdate = true;
                        forceSpeedUpdate = true;
                        forceAltitudeUpdate = true;
                        forceIDUpdate = true;
                    } else {
                        lastClickTime = now;
                        waitingForDoubleClick = true;
                    }
                }
            }
        }
    }
    lastButtonState = buttonState;

    if (displayIsOn && currentScreen == 0 && waitingForDoubleClick && (millis() - lastClickTime > doubleClickWindow)) {
        currentTargetIndex++;
        if (currentTargetIndex >= totalTargets) {
            currentTargetIndex = 0;
        }

        Serial.print("Single click detected, switched to coordinate: ");
        Serial.println(currentTargetIndex + 1);

        waitingForDoubleClick = false;

        forceNavigationUpdate = true;
        forceSpeedUpdate = true;
        forceAltitudeUpdate = true;
        forceIDUpdate = true;
    }
}

void getBatteryReading(int &percentage, float &voltage)
{
#ifdef ADC_CTRL
    pinMode(ADC_CTRL, OUTPUT);
    digitalWrite(ADC_CTRL, HIGH); 
    delay(10);
#endif

    int rawValue = analogRead(BATTERY_PIN);

#ifdef ADC_CTRL
    digitalWrite(ADC_CTRL, LOW); 
#endif

    voltage = (rawValue / 4095.0) * 3.3 * ADC_MULTIPLIER;
    percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
    if (percentage > 100) percentage = 100;
    if (percentage < 0) percentage = 0;
}

// Tab 1
void updateNavigationScreen()
{
    if (GPS.date.isValid()) {
        char date_buf[16];
        sprintf(date_buf, "%02d/%02d/%04d", GPS.date.day(), GPS.date.month(), GPS.date.year());
        String currentDate = String(date_buf);

        if (currentDate != lastDate) {
            lastDate = currentDate;
            st7735.st7735_write_str(0, 0, currentDate, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }
    }

    if (GPS.time.isValid()) {
        char time_buf[20];
        sprintf(time_buf, "%02d:%02d:%02dZ", GPS.time.hour(), GPS.time.minute(), GPS.time.second());
        String currentTime = String(time_buf);

        if (currentTime != lastTime) {
            lastTime = currentTime;
            st7735.st7735_write_str(91, 0, currentTime, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }
    }

    if (millis() - lastBatteryCheck >= batteryInterval || lastBatteryCheck == 0) {
        lastBatteryCheck = millis();
        getBatteryReading(cachedBatteryPct, cachedVoltage);

        char bat_buf[16];
        sprintf(bat_buf, "%d%% %.1fV", cachedBatteryPct, cachedVoltage);
        String currentBatteryStr = String(bat_buf);

        if (currentBatteryStr != lastBatteryStr) {
            lastBatteryStr = currentBatteryStr;
            st7735.st7735_write_str(0, 12, currentBatteryStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }
    }

    int currentSatellites = GPS.satellites.value();
    if (currentSatellites != lastSatellites || GPS.satellites.isUpdated()) {
        lastSatellites = currentSatellites;
        char sat_buf[12];
        sprintf(sat_buf, "%02d Sats", currentSatellites);
        String satStr = String(sat_buf);
        
        st7735.st7735_write_str(105, 12, satStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    if (GPS.location.isValid()) {
        double targetLat = targets[currentTargetIndex].lat;
        double targetLng = targets[currentTargetIndex].lng;

        double distanceMeters = TinyGPSPlus::distanceBetween(
            GPS.location.lat(), GPS.location.lng(), targetLat, targetLng
        );

        String distance;
        if (distanceMeters < 1000) {
            distance = String(distanceMeters, 1) + "m";
        } else {
            distance = String(distanceMeters / 1609.344, 2) + "mi";
        }

        double bearing = TinyGPSPlus::courseTo(
            GPS.location.lat(), GPS.location.lng(), targetLat, targetLng
        );

        String currentInfo = distance + " " + String((int)bearing) + "d";

        if (currentInfo != lastInfo || forceNavigationUpdate) {
            lastInfo = currentInfo;
            forceNavigationUpdate = false;

            while (currentInfo.length() < 16) {
                currentInfo += " ";
            }

            st7735.st7735_write_str(0, 28, currentInfo, Font_11x18, ST7735_GREEN, ST7735_BLACK);
        }

        float speedMph = GPS.speed.mph();
        float speedKmph = GPS.speed.kmph();
        String speedMphStr = String(speedMph, 1);
        String speedKmphStr = String(speedKmph, 1);
        
        String currentSpeed = speedKmphStr + "kph / " + speedMphStr + "mph";

        if (currentSpeed != lastSpeed || forceSpeedUpdate) {
            lastSpeed = currentSpeed;
            forceSpeedUpdate = false;

            while (currentSpeed.length() < 15) {
                currentSpeed += " ";
            }

            st7735.st7735_write_str(0, 52, currentSpeed, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }

        double altMeters = GPS.altitude.meters();
        double altFeet = altMeters * 3.28084;
        String currentAltitude = String((int)altMeters) + "m / " + String((int)altFeet) + "ft";

        if (currentAltitude != lastAltitude || forceAltitudeUpdate) {
            lastAltitude = currentAltitude;
            forceAltitudeUpdate = false;

            while (currentAltitude.length() < 18) {
                currentAltitude += " ";
            }

            st7735.st7735_write_str(0, 64, currentAltitude, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }

        String currentID = String(currentTargetIndex + 1);

        if (currentID != lastTargetID || forceIDUpdate) {
            lastTargetID = currentID;
            forceIDUpdate = false;

            while (currentID.length() < 3) {
                currentID += " ";
            }

            st7735.st7735_write_str(135, 52, currentID, Font_11x18, ST7735_YELLOW, ST7735_BLACK);
        }

    } else {
        String acquiringStr = "Acquiring Fix   "; 
        if (lastInfo != acquiringStr || forceNavigationUpdate) {
            lastInfo = acquiringStr;
            forceNavigationUpdate = false;

            st7735.st7735_write_str(0, 28, acquiringStr, Font_11x18, ST7735_CYAN, ST7735_BLACK);
        }
    }
}

// Tab 2
void updateCoordinatesScreen()
{
    if (GPS.date.isValid()) {
        char date_buf[16];
        sprintf(date_buf, "%02d/%02d/%04d", GPS.date.day(), GPS.date.month(), GPS.date.year());
        String currentDate = String(date_buf);
        if (currentDate != lastDate || forceCoordUpdate) {
            lastDate = currentDate;
            st7735.st7735_write_str(0, 0, currentDate, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }
    }

    if (GPS.time.isValid()) {
        char time_buf[20];
        sprintf(time_buf, "%02d:%02d:%02dZ", GPS.time.hour(), GPS.time.minute(), GPS.time.second());
        String currentTime = String(time_buf);
        if (currentTime != lastTime || forceCoordUpdate) {
            lastTime = currentTime;
            st7735.st7735_write_str(91, 0, currentTime, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        }
    }

    if (millis() - lastBatteryCheck >= batteryInterval || lastBatteryCheck == 0 || forceCoordUpdate) {
        lastBatteryCheck = millis();
        getBatteryReading(cachedBatteryPct, cachedVoltage);
        char bat_buf[16];
        sprintf(bat_buf, "%d%% %.1fV", cachedBatteryPct, cachedVoltage);
        lastBatteryStr = String(bat_buf);
        st7735.st7735_write_str(0, 12, lastBatteryStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    int currentSatellites = GPS.satellites.value();
    if (currentSatellites != lastSatellites || GPS.satellites.isUpdated() || forceCoordUpdate) {
        lastSatellites = currentSatellites;
        char sat_buf[12];
        sprintf(sat_buf, "%02d Sats", currentSatellites);
        st7735.st7735_write_str(105, 12, String(sat_buf), Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    String currentLatStr, currentLonStr;
    if (GPS.location.isValid()) {
        currentLatStr = "LAT: " + String(GPS.location.lat(), 5);
        currentLonStr = "LON: " + String(GPS.location.lng(), 5);
    } else {
        currentLatStr = "LAT: Acquiring...";
        currentLonStr = "LON: Acquiring...";
    }

    if (currentLatStr != lastLatStr || forceCoordUpdate || GPS.location.isUpdated()) {
        lastLatStr = currentLatStr;
        while (currentLatStr.length() < 22) currentLatStr += " ";
        st7735.st7735_write_str(0, 28, currentLatStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    if (currentLonStr != lastLonStr || forceCoordUpdate || GPS.location.isUpdated()) {
        lastLonStr = currentLonStr;
        while (currentLonStr.length() < 22) currentLonStr += " ";
        st7735.st7735_write_str(0, 40, currentLonStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    float speedMph = GPS.speed.mph();
    float speedKmph = GPS.speed.kmph();
    String currentSpeed = String(speedKmph, 1) + "kph / " + String(speedMph, 1) + "mph";

    if (currentSpeed != lastScreenSpeed || forceCoordUpdate || GPS.speed.isUpdated()) {
        lastScreenSpeed = currentSpeed;
        while (currentSpeed.length() < 18) currentSpeed += " ";
        st7735.st7735_write_str(0, 52, currentSpeed, Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    double altMeters = GPS.altitude.meters();
    double altFeet = altMeters * 3.28084;
    String currentAltitude = String((int)altMeters) + "m / " + String((int)altFeet) + "ft";

    if (currentAltitude != lastScreenAltitude || forceCoordUpdate || GPS.altitude.isUpdated()) {
        lastScreenAltitude = currentAltitude;
        while (currentAltitude.length() < 18) currentAltitude += " ";
        st7735.st7735_write_str(0, 64, currentAltitude, Font_7x10, ST7735_WHITE, ST7735_BLACK);
        forceCoordUpdate = false; 
    }
}

void updateDisplay()
{
    // Check for display timeout
    if (displayIsOn && (millis() - lastActivityTime >= screenTimeout)) {
        sleepScreen();
        return;
    }

    // Do not update pixels if screen is off
    if (!displayIsOn) {
        return;
    }

    if (currentScreen == 0) {
        updateNavigationScreen();
    } else {
        updateCoordinatesScreen();
    }
}

void GPS_test()
{
    pinMode(VGNSS_CTRL, OUTPUT);
    digitalWrite(VGNSS_CTRL, HIGH);

    pinMode(BUTTON_PIN, INPUT);

    // Init display backlight control pin as output and turn it on
    pinMode(ST7735_LED_K_Pin, OUTPUT);
    digitalWrite(ST7735_LED_K_Pin, HIGH);

    pinMode(BATTERY_PIN, INPUT);
    analogSetPinAttenuation(BATTERY_PIN, ADC_ATTENUATION);

    Serial1.begin(115200, SERIAL_8N1, 33, 34);
    Serial.println("Started");

    st7735.st7735_init();
    st7735.st7735_fill_screen(ST7735_BLACK);

    st7735.st7735_write_str(0, 0, "GPS Init", Font_16x26, ST7735_WHITE, ST7735_BLACK);
    delay(500);
    st7735.st7735_fill_screen(ST7735_BLACK);

    lastActivityTime = millis(); // Init timeout tracker

    while (1)
    {
        int maxBytes = 32; 
        while (Serial1.available() && maxBytes > 0) {
            GPS.encode(Serial1.read());
            maxBytes--;
        }

        checkButton();

        if (millis() - lastDisplayUpdate >= displayInterval) {
            lastDisplayUpdate = millis();
            updateDisplay();
        }
    }
}

void setup()
{
    delay(100);
    Serial.begin(115200);
    st7735.st7735_init();
    GPS_test();
}

void loop()
{
}
