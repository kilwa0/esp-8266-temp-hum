# ESP8266 Temperature & Humidity Sensor

A room sensor based on the **Wemos D1 mini (ESP8266)** that reads temperature and humidity from a **DHT11**, displays live data on an **OLED SSD1306** screen, and publishes readings to **Home Assistant** via **MQTT Discovery** — no `configuration.yaml` editing required.

---

## Features

- Temperature (°C) and humidity (%) readings every **30 seconds**
- MQTT publish with **retain** flag on dedicated topics
- **Auto-discovery** for Home Assistant: entities appear automatically
- OLED display showing live values, IP address, and WiFi/MQTT status
- Automatic reconnection to both WiFi and MQTT broker on network drops
- Stable client ID derived from the ESP8266 Chip ID

---

## Hardware

| Component | Model | Pin |
|---|---|---|
| Microcontroller | Wemos D1 mini (ESP8266) | — |
| Temperature/humidity sensor | DHT11 | `D5` |
| OLED display | SSD1306 128×64 I2C | SDA=`D2`, SCL=`D1` |

---

## Dependencies (Arduino IDE / PlatformIO)

Install via the Library Manager:

- `ESP8266WiFi` (bundled with the ESP8266 core)
- `PubSubClient` — Nick O'Leary
- `DHT sensor library` — Adafruit
- `Adafruit GFX Library`
- `Adafruit SSD1306`

---

## Configuration

Credentials are managed through a **`secrets.h`** file (excluded from the repository via `.gitignore`). Create it in the same folder as `espSensor.ino`:

```cpp
// secrets.h
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
#define MQTT_BROKER     "homeassistant.local"
#define MQTT_PORT       1883
#define MQTT_USER       "mq"
#define MQTT_PASSWORD   "melon"
```

> ⚠️ **Never commit `secrets.h` to the repository.** It is already listed in `.gitignore`.

---

## MQTT Topics

### State topics (published every 30 s with `retain=true`)

| Data | Topic |
|---|---|
| Temperature | `home/Despacho_1/temperatura` |
| Humidity | `home/Despacho_1/humedad` |

### Discovery topics (published on connect with `retain=true`)

| Sensor | Config topic |
|---|---|
| Temperature | `homeassistant/sensor/despacho_1_temp/config` |
| Humidity | `homeassistant/sensor/despacho_1_hum/config` |

The discovery payloads register the device as **Despacho 1** (`ESP8266 DHT11 / DIY`) with proper `device_class` and `state_class` so Home Assistant computes long-term statistics automatically.

---

## Home Assistant Integration

1. Install and start the **Mosquitto broker** add-on in Home Assistant OS.
2. Enable the **MQTT integration** under *Settings → Devices & Services → MQTT*.
3. Confirm the discovery prefix is `homeassistant` (default value).
4. Flash the firmware and power on the device.
5. Within seconds the **Despacho 1** device with its two entities will appear under *Settings → Devices & Services → MQTT → Devices*.

To debug, subscribe to `homeassistant/#` from the HA MQTT panel and verify that retained config messages arrive for both sensors.

---

## Firmware Logic

```
setup()
  ├─ Initialize DHT11
  ├─ Initialize OLED  (Wire I2C: D2/D1, addr 0x3C)
  ├─ Connect to WiFi  (15 s timeout, auto-reconnect enabled)
  ├─ Configure PubSubClient  (buffer 1024 bytes)
  └─ Connect to MQTT + send discovery payloads

loop()  [~10 ms per iteration]
  ├─ WiFi down? → retry every 10 s
  ├─ MQTT down? → retry every 5 s
  ├─ client.loop()  ← keeps MQTT connection alive
  └─ Every 30 s → publishSensorData()
        ├─ Read DHT11
        ├─ Publish temperature & humidity  (retain)
        └─ Refresh OLED
```

---

## OLED Display

The screen shows up to four lines of live status:

```
Temp: 23.5 C
Hum : 48.0 %
192.168.1.42
MQTT OK
```

During boot it shows WiFi and MQTT connection progress.

---

## Repository Structure

```
esp-8266-temp-hum/
├── espSensor.ino   # Main firmware
├── secrets.h       # ⚠️ Create manually (see Configuration)
└── .gitignore
```

---

## License

DIY project, free to use. No warranties.
