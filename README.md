# CycleTrack

A GPS tracker for a bicycle. An ESP32 reads its position from a NEO-6M GPS module, sends it over Wi-Fi to Firebase Realtime Database, and a web dashboard shows the cycle live on a map.

```
NEO-6M GPS ──UART──▶ ESP32 ──Wi-Fi / HTTPS──▶ Firebase RTDB ◀──polls every 30 s── index.html (Leaflet map)
```

## Repository layout

```
cycletrack/
├── index.html                          Web dashboard (login, live map, GPS and device status)
├── firmware/
│   └── cycletrack_esp32/
│       ├── cycletrack_esp32.ino        ESP32 sketch: GPS read, Wi-Fi, Firebase upload
│       └── config.example.h            Template for Wi-Fi and Firebase settings
├── firebase/
│   └── database.rules.json             Realtime Database rules for the location path
└── README.md
```

`index.html` stays at the root so GitHub Pages keeps serving the dashboard at the same link.

## Hardware

| Part | Notes |
|------|-------|
| ESP32 dev board | Any ESP32-WROOM board works |
| NEO-6M GPS module | With its ceramic patch antenna facing the sky |
| Power | USB power bank or 3.7 V Li-ion with a 5 V boost module |

### Wiring

| NEO-6M | ESP32 |
|--------|-------|
| VCC | 3.3 V (or 5 V if your module has a regulator) |
| GND | GND |
| TX | GPIO16 (RX2) |
| RX | GPIO17 (TX2) |

The onboard LED on GPIO2 blinks while the GPS is searching and stays on once it has a fix.

## Firmware setup

1. Install the ESP32 board package in Arduino IDE (Boards Manager, search "esp32" by Espressif).
2. Install **TinyGPSPlus** by Mikal Hart from the Library Manager.
3. In `firmware/cycletrack_esp32/`, copy `config.example.h` to `config.h` and fill in your Wi-Fi name, password and Firebase host. `config.h` is git-ignored.
4. Open `cycletrack_esp32.ino`, select **ESP32 Dev Module**, and upload.
5. Open Serial Monitor at 115200 baud to see Wi-Fi status and each upload.

The first GPS fix outdoors can take a few minutes from a cold start. Indoors the NEO-6M usually won't get one.

## Data format

The ESP32 updates `/devices/cycle001/location` every 10 seconds:

```json
{
  "lat": 26.471234,
  "lng": 73.113456,
  "alt": 231.4,
  "speed": 12.6,
  "satellites": 8,
  "accuracy": 3.0,
  "gpsFixed": true,
  "wifi": true,
  "timestamp": 1792811617897
}
```

It uses a PATCH request, so while the GPS is searching only `gpsFixed`, `satellites`, `wifi` and `timestamp` change, and the last known coordinates stay on the map. `accuracy` is estimated from HDOP. `timestamp` is Unix time in milliseconds from NTP, with the GPS clock as a fallback.

The dashboard treats the device as powered off when the timestamp is more than 2 minutes old.

## Firebase setup

1. Create a Firebase project and add a Realtime Database.
2. Paste `firebase/database.rules.json` into **Realtime Database → Rules** and publish.
3. Put the database host in `config.h` and the full `.../devices/cycle001/location.json` URL in `FIREBASE_URL` inside `index.html`.

These rules keep the database open to reads and writes on the location path only, and check that every field has the right type. For anything beyond a demo, add Firebase Authentication and lock writes to the device.

## Dashboard

Open `index.html` directly or through GitHub Pages. It shows the cycle's position, speed, altitude, satellites, accuracy, and when the last update came in (IST). Set `USE_MOCK_DATA = true` in the script to test it without the hardware.

The login on the page is a client-side gate only. Anyone who opens the page source can see the usernames and passwords, so treat it as a demo screen rather than real security.
