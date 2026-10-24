// ------------------------------------------------------------
//  CycleTrack ESP32 configuration
//
//  Copy this file to config.h in the same folder and fill in
//  your own values. config.h is git-ignored so your Wi-Fi
//  password and Firebase key never end up on GitHub.
// ------------------------------------------------------------
#pragma once

// ---------- Wi-Fi ----------
#define WIFI_SSID       "your-wifi-name"
#define WIFI_PASSWORD   "your-wifi-password"

// ---------- Firebase Realtime Database ----------
// Host only: no https:// and no trailing slash.
#define FIREBASE_HOST   "cycletrack-gps-299ba-default-rtdb.asia-southeast1.firebasedatabase.app"

// Path the website reads from (must match FIREBASE_URL in index.html).
#define FIREBASE_PATH   "/devices/cycle001/location"

// Leave empty if your database rules allow public writes to the path.
// Otherwise put a database secret or ID token here.
#define FIREBASE_AUTH   ""

// ---------- GPS (NEO-6M) wiring ----------
// NEO-6M TX -> ESP32 GPIO16 (RX2)
// NEO-6M RX -> ESP32 GPIO17 (TX2)
#define GPS_RX_PIN      16
#define GPS_TX_PIN      17
#define GPS_BAUD        9600

// ---------- Timing ----------
// The website marks the device as off after 2 minutes of silence,
// so keep this well under 120 000 ms.
#define UPLOAD_INTERVAL_MS   10000
