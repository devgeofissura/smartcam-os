# Build
- `arduino-cli compile --fqbn esp32:esp32:esp32s3`
- Core: esp32:esp32:esp32s3 v3.3.10
- Board: LilyGO T-SIMCAM V1.3 (ESP32-S3 rev0.2, 8MB PSRAM)
- No filesystem partition (SPIFFS absent)

# WiFi Config Flow
1. Boot → try saved credentials from Preferences (NVS)
2. If fail/no creds → AP mode + captive portal (SSID: `GeoFissura`, password: `Tenta&70`, IP: `192.168.4.1`)
3. User connects to AP, opens Settings page, scans networks, enters password, saves
4. Save writes to Preferences via `POST /api/wifi/config` → `ESP.restart()`
5. On reboot, tries saved STA credentials

# Known Issues
- WebServer must be restarted after WiFi mode switch (STA→AP) because socket bound
  to STA interface becomes orphaned. Handled in `SmartCamOS.ino` after
  `connectOrFallback()` — stops and restarts `apiServer` + re-registers routes.
