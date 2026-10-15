/*
  esp32-base: base firmware for the 7Semi ESP32-S3 board

  - Connects to WiFi at power-on and reconnects automatically
  - Wireless upload (OTA) from Arduino IDE after the first USB upload
  - Onboard RGB LED shows status:
      yellow blinking = connecting to WiFi
      green           = connected
      blue            = wireless upload in progress
      red             = WiFi not reachable for 30 s (still retrying)

  RULE: keep everything marked CORE in every sketch you build from this one,
  or wireless upload stops working. Put your own code in projectSetup() and
  projectLoop() at the bottom.
*/

#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

// ===== Settings =====
const char* HOSTNAME  = "esp32-desk";  // name shown in Arduino IDE under Tools > Port
const int   RGB_PIN   = 48;            // onboard RGB LED; if it stays dark, change to 38
const int   LED_LEVEL = 8;            // LED brightness, 0-255

// ===== CORE: status LED =====
void led(int r, int g, int b) {
  static int lastR = -1, lastG = -1, lastB = -1;
  if (r == lastR && g == lastG && b == lastB) return;  // skip if unchanged
  lastR = r; lastG = g; lastB = b;
  rgbLedWrite(RGB_PIN, r, g, b);
}

// ===== CORE: WiFi + OTA =====
bool wasConnected = false;
bool otaStarted = false;
unsigned long disconnectedSince = 0;
unsigned long lastRetry = 0;
unsigned long lastBlink = 0;
bool blinkOn = false;

void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA: upload starting");
      led(0, 0, LED_LEVEL);  // blue
    })
    .onProgress([](unsigned int done, unsigned int total) {
      Serial.printf("OTA: %u%%\r", done * 100 / total);
    })
    .onEnd([]() {
      Serial.println("\nOTA: finished, restarting");
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA error: %u\n", error);
      led(LED_LEVEL, 0, 0);  // red
    });
}

void coreSetup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nesp32-base starting");
  led(LED_LEVEL, LED_LEVEL, 0);  // yellow

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // quicker responses; fine on USB power
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  setupOTA();
  disconnectedSince = millis();
  lastRetry = millis();
}

void coreLoop() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  // Just connected (or reconnected): start wireless upload
  if (connected && !wasConnected) {
    Serial.print("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
    if (otaStarted) ArduinoOTA.end();
    ArduinoOTA.begin();
    otaStarted = true;
    led(0, LED_LEVEL, 0);  // green
  }

  if (!connected) {
    unsigned long now = millis();

    if (wasConnected) {  // WiFi just dropped
      Serial.println("WiFi lost, reconnecting...");
      disconnectedSince = now;
      lastRetry = now;
    }

    if (now - lastRetry > 15000) {  // retry every 15 s
      lastRetry = now;
      Serial.printf("Retrying WiFi (status %d)...\n", WiFi.status());
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    if (now - disconnectedSince > 30000) {
      led(LED_LEVEL, 0, 0);  // red: unreachable for over 30 s
    } else if (now - lastBlink > 400) {
      lastBlink = now;
      blinkOn = !blinkOn;
      if (blinkOn) led(LED_LEVEL, LED_LEVEL, 0);  // yellow blink
      else led(0, 0, 0);
    }
  }

  wasConnected = connected;

  if (connected && otaStarted) ArduinoOTA.handle();
}

void setup() {
  coreSetup();    // CORE: keep this line
  projectSetup();
}

void loop() {
  coreLoop();     // CORE: keep this line
  projectLoop();
}

// ===== YOUR PROJECT CODE =====
void projectSetup() {
}

void projectLoop() {
  // Keep this fast. Avoid long delay() calls, or wireless upload becomes unreliable.
}
