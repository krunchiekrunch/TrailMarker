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

struct Target {
    double lat;
    double lng;
};

// Coordinates array
Target targets[] = {
    {0.0, 0.0},
    {0.0, 180.0},
};

const int totalTargets = sizeof(targets) / sizeof(targets[0]);
int currentTargetIndex = 0;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

// Double click config
unsigned long lastClickTime = 0;
const unsigned long doubleClickWindow = 400; // Max time in ms between clicks for a double click
bool waitingForDoubleClick = false;

unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 150; // Display update interval

unsigned long lastBatteryCheck = 0;
const unsigned long batteryInterval = 10000; // Battery update interval
int cachedBatteryPct = 0;
float cachedVoltage = 0.0;

String lastDate = "";
String lastTime = "";
String lastInfo = "";
String lastSpeed = "";
String lastTargetID = "";
int lastSatellites = -1;
String lastBatteryStr = "";

bool forceNavigationUpdate = true;
bool forceSpeedUpdate = true;
bool forceIDUpdate = true;

void checkButton()
{
    static int lastReading = HIGH;
    int buttonState = digitalRead(BUTTON_PIN);

    if (buttonState != lastReading) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (buttonState == LOW && lastButtonState == HIGH) {
            unsigned long now = millis();
            
            if (waitingForDoubleClick && (now - lastClickTime <= doubleClickWindow)) {
                // Double-click detected: go back 1 ID
                currentTargetIndex--;
                if (currentTargetIndex < 0) {
                    currentTargetIndex = totalTargets - 1;
                }

                Serial.print("Double-click detected, switched back to coordinate: ");
                Serial.println(currentTargetIndex + 1);

                waitingForDoubleClick = false;
                
                forceNavigationUpdate = true;
                forceSpeedUpdate = true;
                forceIDUpdate = true;
            } else {
                // First click registered, start timer to check for double click
                lastClickTime = now;
                waitingForDoubleClick = true;
            }
        }
        lastButtonState = buttonState;
    }
    lastReading = buttonState;

    // If the double click window expires without a second click, treat it as a single click
    if (waitingForDoubleClick && (millis() - lastClickTime > doubleClickWindow)) {
        currentTargetIndex++;
        if (currentTargetIndex >= totalTargets) {
            currentTargetIndex = 0;
        }

        Serial.print("Single-click processed, switched to coordinate: ");
        Serial.println(currentTargetIndex + 1);

        waitingForDoubleClick = false;

        forceNavigationUpdate = true;
        forceSpeedUpdate = true;
        forceIDUpdate = true;
    }
}

void getBatteryReading(int &percentage, float &voltage)
{
#ifdef ADC_CTRL
    pinMode(ADC_CTRL, OUTPUT);
    digitalWrite(ADC_CTRL, HIGH); // Enable voltage divider
    delay(10);
#endif

    int rawValue = analogRead(BATTERY_PIN);

#ifdef ADC_CTRL
    digitalWrite(ADC_CTRL, LOW); // Disable voltage divider to save power
#endif

    // Convert ADC to voltage
    voltage = (rawValue / 4095.0) * 3.3 * ADC_MULTIPLIER;

    // Estimate percentage
    percentage = (int)((voltage - 3.3) / (4.2 - 3.3) * 100.0);
    if (percentage > 100) percentage = 100;
    if (percentage < 0) percentage = 0;
}

void updateDisplay()
{
    // Date
    if (GPS.date.isValid()) {
        char date_buf[16];
        sprintf(date_buf, "%02d/%02d/%04d", GPS.date.day(), GPS.date.month(), GPS.date.year());
        String currentDate = String(date_buf);

        if (currentDate != lastDate) {
            lastDate = currentDate;
            st7735.st7735_write_str(0, 0, currentDate, Font_7x10, ST7735_YELLOW, ST7735_BLACK);
        }
    }

    // Battery
    if (millis() - lastBatteryCheck >= batteryInterval || lastBatteryCheck == 0) {
        lastBatteryCheck = millis();
        getBatteryReading(cachedBatteryPct, cachedVoltage);

        char bat_buf[16];
        sprintf(bat_buf, "%d%% %.1fV", cachedBatteryPct, cachedVoltage);
        String currentBatteryStr = String(bat_buf);

        if (currentBatteryStr != lastBatteryStr) {
            lastBatteryStr = currentBatteryStr;
            st7735.st7735_write_str(97, 0, currentBatteryStr, Font_7x10, ST7735_MAGENTA, ST7735_BLACK);
        }
    }

    // Satellites
    int currentSatellites = GPS.satellites.value();
    if (currentSatellites != lastSatellites || GPS.satellites.isUpdated()) {
        lastSatellites = currentSatellites;
        char sat_buf[12];
        sprintf(sat_buf, "%02d Sats", currentSatellites);
        String satStr = String(sat_buf);
        
        st7735.st7735_write_str(105, 12, satStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);
    }

    // Time
    if (GPS.time.isValid()) {
        char time_buf[20];
        sprintf(time_buf, "%02d:%02d:%02d UTC", GPS.time.hour(), GPS.time.minute(), GPS.time.second());
        String currentTime = String(time_buf);

        if (currentTime != lastTime) {
            lastTime = currentTime;
            st7735.st7735_write_str(0, 12, currentTime, Font_7x10, ST7735_CYAN, ST7735_BLACK);
        }
    }

    // Distance and bearings
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

        // Speed
        String currentSpeed = String(GPS.speed.mph(), 1) + " mph";

        if (currentSpeed != lastSpeed || forceSpeedUpdate) {
            lastSpeed = currentSpeed;
            forceSpeedUpdate = false;

            while (currentSpeed.length() < 10) {
                currentSpeed += " ";
            }

            st7735.st7735_write_str(0, 52, currentSpeed, Font_11x18, ST7735_WHITE, ST7735_BLACK);
        }

        // Coordinates ID
        String currentID = String(currentTargetIndex + 1);

        if (currentID != lastTargetID || forceIDUpdate) {
            lastTargetID = currentID;
            forceIDUpdate = false;

            while (currentID.length() < 2) {
                currentID += " ";
            }

            st7735.st7735_write_str(135, 52, currentID, Font_11x18, ST7735_RED, ST7735_BLACK);
        }

    } else {
        String acquiringStr = "Acquiring Fix   "; 
        if (lastInfo != acquiringStr || forceNavigationUpdate) {
            lastInfo = acquiringStr;
            forceNavigationUpdate = false;

            st7735.st7735_write_str(0, 28, acquiringStr, Font_11x18, ST7735_MAGENTA, ST7735_BLACK);
        }
    }
}

void GPS_test()
{
    pinMode(VGNSS_CTRL, OUTPUT);
    digitalWrite(VGNSS_CTRL, HIGH);

    pinMode(BUTTON_PIN, INPUT);

    pinMode(BATTERY_PIN, INPUT);
    analogSetPinAttenuation(BATTERY_PIN, ADC_ATTENUATION);

    Serial1.begin(115200, SERIAL_8N1, 33, 34);
    Serial.println("Started");

    st7735.st7735_fill_screen(ST7735_BLACK);

    st7735.st7735_write_str(0, 0, "GPS Init", Font_11x18, ST7735_WHITE, ST7735_BLACK);
    delay(500);
    st7735.st7735_fill_screen(ST7735_BLACK);

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
