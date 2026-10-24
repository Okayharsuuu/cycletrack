/*
 * CycleTrack - ESP32 + NEO-6M GPS tracker
 *
 * Reads the NEO-6M over UART2, connects to Wi-Fi and pushes the
 * cycle's location to Firebase Realtime Database every few seconds.
 * The website (index.html) polls the same path and shows it on a map.
 *
 * Data written to FIREBASE_PATH:
 *   lat, lng      degrees            (only sent when the GPS has a fix,
 *   alt           metres              so the last known spot is kept)
 *   speed         km/h
 *   satellites    count
 *   accuracy      metres (estimated from HDOP)
 *   gpsFixed      true / false
 *   wifi          true
 *   timestamp     Unix time in milliseconds (NTP, GPS time as fallback)
 *
 * Board:     ESP32 Dev Module
 * Libraries: TinyGPSPlus by Mikal Hart (Library Manager)
 *
 * Setup: copy config.example.h to config.h and fill in your values.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>
#include <time.h>

#include "config.h"

// ESP32 dev boards have a blue LED on GPIO2
#define STATUS_LED 2

// A NEO-6M position error is roughly HDOP x 2.5 m in open sky
#define UERE_METRES 2.5

// If no valid sentence arrives for this long, treat the fix as lost
#define GPS_STALE_MS 5000

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

unsigned long lastUpload = 0;
unsigned long lastWifiAttempt = 0;
bool ntpStarted = false;

void feedGPS();

// ------------------------------------------------------------
//  Wi-Fi
// ------------------------------------------------------------
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  // Don't hammer the router; retry at most every 10 s
  if (lastWifiAttempt != 0 && millis() - lastWifiAttempt < 10000) return;
  lastWifiAttempt = millis();

  Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    feedGPS();               // keep reading GPS while we wait
    delay(50);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected, IP %s\n", WiFi.localIP().toString().c_str());
    if (!ntpStarted) {
      configTime(0, 0, "pool.ntp.org", "time.google.com");
      ntpStarted = true;
    }
  } else {
    Serial.println("[WiFi] Not connected, will retry");
  }
}

// ------------------------------------------------------------
//  GPS
// ------------------------------------------------------------
void feedGPS() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
}

bool hasFix() {
  return gps.location.isValid() && gps.location.age() < GPS_STALE_MS;
}

// Days since 1970-01-01 for a Gregorian date
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

// Unix time in ms. Prefers NTP, falls back to the GPS clock, 0 if neither.
uint64_t currentTimestampMs() {
  time_t now = time(nullptr);
  if (now > 1600000000) {               // NTP has synced (after Sep 2020)
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
  }

  if (gps.date.isValid() && gps.time.isValid() && gps.date.year() > 2020) {
    long days = daysFromCivil(gps.date.year(), gps.date.month(), gps.date.day());
    uint64_t secs = (uint64_t)days * 86400ULL
                  + gps.time.hour() * 3600UL
                  + gps.time.minute() * 60UL
                  + gps.time.second();
    return secs * 1000ULL + gps.time.centisecond() * 10;
  }

  return 0;
}

// ------------------------------------------------------------
//  Firebase upload
// ------------------------------------------------------------
String buildPayload() {
  bool fix = hasFix();
  String json = "{";

  if (fix) {
    json += "\"lat\":"   + String(gps.location.lat(), 6) + ",";
    json += "\"lng\":"   + String(gps.location.lng(), 6) + ",";
    if (gps.altitude.isValid()) json += "\"alt\":" + String(gps.altitude.meters(), 1) + ",";
    if (gps.speed.isValid())    json += "\"speed\":" + String(gps.speed.kmph(), 1) + ",";
    if (gps.hdop.isValid())     json += "\"accuracy\":" + String(gps.hdop.hdop() * UERE_METRES, 1) + ",";
  }

  json += "\"satellites\":" + String(gps.satellites.isValid() ? gps.satellites.value() : 0) + ",";
  json += "\"gpsFixed\":" + String(fix ? "true" : "false") + ",";
  json += "\"wifi\":true";

  uint64_t ts = currentTimestampMs();
  if (ts > 0) {
    char buf[21];
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long)ts);
    json += ",\"timestamp\":";
    json += buf;
  }

  json += "}";
  return json;
}

bool uploadLocation() {
  WiFiClientSecure client;
  client.setInsecure();              // skip certificate check (fine for a hobby tracker)

  String url = String("https://") + FIREBASE_HOST + FIREBASE_PATH + ".json";
  if (strlen(FIREBASE_AUTH) > 0) url += String("?auth=") + FIREBASE_AUTH;

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[Firebase] Could not start request");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  String body = buildPayload();
  // PATCH only touches the fields we send, so lat/lng from the last fix
  // stay in the database while the GPS is searching.
  int code = http.sendRequest("PATCH", body);

  if (code == 200) {
    Serial.printf("[Firebase] OK  %s\n", body.c_str());
  } else {
    Serial.printf("[Firebase] HTTP %d  %s\n", code, http.getString().c_str());
  }
  http.end();
  return code == 200;
}

// ------------------------------------------------------------
//  Arduino entry points
// ------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nCycleTrack ESP32 starting");

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  connectWiFi();
}

void loop() {
  feedGPS();

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(STATUS_LED, LOW);
    connectWiFi();
    return;
  }

  // LED: solid when GPS has a fix, blinking while searching
  digitalWrite(STATUS_LED, hasFix() ? HIGH : (millis() / 500) % 2);

  if (millis() - lastUpload >= UPLOAD_INTERVAL_MS) {
    lastUpload = millis();

    if (gps.charsProcessed() < 10) {
      Serial.println("[GPS] No data from NEO-6M, check wiring (TX->16, RX->17)");
    }
    uploadLocation();
  }
}
