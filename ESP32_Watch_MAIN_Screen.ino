#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SPIFFS.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <MPU6050.h>

TFT_eSPI tft = TFT_eSPI();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

const char* ssid = "Viettel";
const char* password = "00000000";
const char* apiKey = "86c9613f62cdb46eeec19f77111a1535";

int batteryLevel = 75;
float tempC = 0;
String weatherIconCode = "10d";

unsigned long lastClockUpdate = 0;
unsigned long lastWeatherUpdate = 0;
String lastTimeStr = "";

int currentScreen = 0;  // 0: Main, 1: HR-SpO2, 2: Speed-Step
bool screenOn = true;

#define BUTTON_PAGE 6
#define BUTTON_SCREEN 7
#define VIBRATION_PIN 4

MAX30105 maxSensor;
uint32_t irBuffer[100];
uint32_t redBuffer[100];
int bpm = 0;
int spo2 = 0;
int8_t validSpO2 = 0;
int8_t validHeartRate = 0;

MPU6050 mpu;
int steps = 0;
unsigned long lastStepTime = 0;
float accelOffset = 16384;

unsigned long lastButtonTime = 0;
const unsigned long debounceDelay = 250;

void vibrate() {
  digitalWrite(VIBRATION_PIN, HIGH);
  delay(50);
  digitalWrite(VIBRATION_PIN, LOW);
}

void drawHeartIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x, y, 4, color);
  tft.fillCircle(x + 5, y, 4, color);
  tft.fillTriangle(x - 3, y + 2, x + 8, y + 2, x + 2, y + 10, color);
}

void drawO2Icon(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 5, color);
  tft.fillCircle(x, y, 2, color);
  tft.setCursor(x + 7, y - 4);
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.print("O2");
}

void drawMainScreen() {
  unsigned long now = millis();
  if (now - lastClockUpdate >= 1000) {
    lastClockUpdate = now;
    timeClient.update();
    String currentTime = timeClient.getFormattedTime();
    if (currentTime != lastTimeStr) {
      lastTimeStr = currentTime;
      tft.fillRect(40, 80, 160, 30, TFT_WHITE);
      tft.setTextColor(TFT_BLACK);
      tft.setTextSize(3);
      tft.setCursor(40, 80);
      tft.println(currentTime);
    }
  }

  if (now - lastWeatherUpdate >= 10000) {
    lastWeatherUpdate = now;

    timeClient.update();
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=Vung%20Tau,vn&appid=" + String(apiKey) + "&units=metric";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      tempC = doc["main"]["temp"];
      weatherIconCode = doc["weather"][0]["icon"].as<String>();
    }
    http.end();

    time_t rawTime = timeClient.getEpochTime();
    struct tm* ti = localtime(&rawTime);
    char dateStr[20];
    sprintf(dateStr, "%02d/%02d/%04d", ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
    tft.fillRect(60, 120, 150, 20, TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(60, 120);
    tft.println(dateStr);

    tft.fillRect(40, 170, 80, 60, TFT_WHITE);
    drawWeatherIconBmp(weatherIconCode.c_str(), 40, 170);

    tft.fillRect(90, 170, 100, 20, TFT_WHITE);
    tft.setCursor(90, 180);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.printf("%.1f", tempC);
    tft.print("C");
  }
}

void drawWeatherIconBmp(const char* iconCode, int16_t x, int16_t y) {
  String path = "/" + String(iconCode) + ".bmp";
  File bmpFS = SPIFFS.open(path, "r");
  if (!bmpFS) return;

  if (bmpFS.read() != 'B' || bmpFS.read() != 'M') {
    bmpFS.close(); return;
  }
  bmpFS.seek(10);
  uint32_t offset = bmpFS.read() | (bmpFS.read() << 8) | (bmpFS.read() << 16) | (bmpFS.read() << 24);
  bmpFS.seek(18);
  int32_t w = bmpFS.read() | (bmpFS.read() << 8) | (bmpFS.read() << 16) | (bmpFS.read() << 24);
  int32_t h = bmpFS.read() | (bmpFS.read() << 8) | (bmpFS.read() << 16) | (bmpFS.read() << 24);
  bmpFS.seek(offset);

  uint8_t rowBuf[96];
  int rowSize = (w * 3 + 3) & ~3;
  for (int row = h - 1; row >= 0; row--) {
    bmpFS.seek(offset + row * rowSize);
    bmpFS.read(rowBuf, rowSize);
    uint8_t* p = rowBuf;
    for (int col = 0; col < w; col++) {
      uint8_t b = *p++, g = *p++, r = *p++;
      tft.drawPixel(x + col, y + (h - 1 - row), tft.color565(r, g, b));
    }
  }
  bmpFS.close();
}

void drawHrSpO2Screen() {
  for (int i = 0; i < 100; i++) {
    redBuffer[i] = maxSensor.getRed();
    irBuffer[i] = maxSensor.getIR();
    delay(20);
  }

  maxim_heart_rate_and_oxygen_saturation(
    irBuffer, redBuffer, 100, &spo2, &validSpO2, &bpm, &validHeartRate);

  tft.fillScreen(TFT_BLACK);
  drawHeartIcon(30, 83, TFT_RED);
  tft.setCursor(50, 75);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("HR");
  tft.setCursor(120, 75);
  tft.printf("%d bpm", bpm);

  drawO2Icon(30, 157, TFT_CYAN);
  tft.setCursor(50, 150);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.print("SpO2");
  tft.setCursor(120, 150);
  tft.printf("%d%%", spo2);
}

void drawSpeedStepScreen() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float aMag = sqrt(ax * ax + ay * ay + az * az);
  float rawDiff = abs(aMag - accelOffset);
  float speed = (rawDiff > 250) ? (rawDiff / 16384.0 * 2.0) : 0.0;
  unsigned long now = millis();
  if (speed > 0.2 && abs(az - accelOffset) > 3000 && (now - lastStepTime) > 400) {
    steps++;
    lastStepTime = now;
  }

  tft.fillScreen(TFT_DARKGREY);
  tft.setTextColor(tft.color565(0, 255, 255));
  tft.setTextSize(3);
  tft.setCursor(80, 105);
  tft.printf("%.2f m/s", speed);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(70, 175);
  tft.printf("Steps: %d", steps);
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_WHITE);
  SPIFFS.begin(true);
  timeClient.begin();
  timeClient.update();
  pinMode(BUTTON_PAGE, INPUT_PULLUP);
  pinMode(BUTTON_SCREEN, INPUT_PULLUP);
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);
  Wire.begin(2, 3);
  maxSensor.begin(Wire);
  maxSensor.setup();
  mpu.initialize();
  for (int i = 0; i < 50; i++) {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    float mag = sqrt(ax * ax + ay * ay + az * az);
    accelOffset += mag;
    delay(10);
  }
  accelOffset /= 50.0;
}

void loop() {
  unsigned long now = millis();

  if (millis() - lastButtonTime > debounceDelay) {
    if (digitalRead(BUTTON_PAGE) == LOW) {
      vibrate();
      currentScreen = (currentScreen + 1) % 3;
      screenOn = true;
      lastButtonTime = millis();
    }
    if (digitalRead(BUTTON_SCREEN) == LOW) {
      vibrate();
      screenOn = !screenOn;
      lastButtonTime = millis();
      if (!screenOn) {
        tft.fillScreen(TFT_BLACK);
        return;
      }
    }
  }

  if (!screenOn) return;

  if (currentScreen == 0) drawMainScreen();
  else if (currentScreen == 1) drawHrSpO2Screen();
  else if (currentScreen == 2) drawSpeedStepScreen();
}