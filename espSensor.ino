#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DHTPIN D5
#define DHTTYPE DHT11

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

const char* ssid = "Ranita";
const char* password = "k1lw4c3r0wl4n!";

const char* mqtt_server = "192.168.2.40";
const int mqtt_port = 1883;
const char* mqtt_user = "mq";
const char* mqtt_password = "melon";

const char* topic_temp = "home/Despacho_1/temperatura";
const char* topic_hum  = "home/Despacho_1/humedad";

// MQTT Discovery topics
const char* topic_temp_config = "homeassistant/sensor/despacho_1_temp/config";
const char* topic_hum_config  = "homeassistant/sensor/despacho_1_hum/config";

// Payloads Home Assistant MQTT Discovery en una sola línea y retain=true
const char* temp_config_payload = "{\"name\":\"Despacho_1 Temp\",\"unique_id\":\"despacho_1_temp\",\"state_topic\":\"home/Despacho_1/temperatura\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"device\":{\"identifiers\":[\"despacho_1_device\"],\"name\":\"Despacho 1\",\"model\":\"ESP8266 DHT11\",\"manufacturer\":\"DIY\"}}";
const char* hum_config_payload  = "{\"name\":\"Despacho_1 Hum\",\"unique_id\":\"despacho_1_hum\",\"state_topic\":\"home/Despacho_1/humedad\",\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"device\":{\"identifiers\":[\"despacho_1_device\"],\"name\":\"Despacho 1\",\"model\":\"ESP8266 DHT11\",\"manufacturer\":\"DIY\"}}";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastPublish = 0;
const unsigned long publishInterval = 30000;

unsigned long lastWiFiAttempt = 0;
const unsigned long wifiRetryInterval = 10000;

unsigned long lastMQTTAttempt = 0;
const unsigned long mqttRetryInterval = 5000;

unsigned long lastScreenUpdate = 0;
const unsigned long screenInterval = 2000;

float lastTemp = NAN;
float lastHum = NAN;

bool discoverySent = false;

void updateDisplay(const String& line1, const String& line2 = "", const String& line3 = "", const String& line4 = "") {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2.length()) display.println(line2);
  if (line3.length()) display.println(line3);
  if (line4.length()) display.println(line4);
  display.display();
}

String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED) {
    return "WiFi OK";
  }
  return "WiFi OFF";
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WiFi: iniciando conexion...");
  updateDisplay("Conectando WiFi...", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
    yield();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi: conectado");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    updateDisplay("WiFi conectado", WiFi.localIP().toString());
  } else {
    Serial.println("WiFi: no conectado");
    updateDisplay("WiFi fallo", "Reintentando...");
  }
}

// bool mqttConnect() {
//   if (WiFi.status() != WL_CONNECTED) return false;

//   String clientId = "despacho_1_";
//   clientId += String(ESP.getChipId(), HEX);

//   Serial.print("MQTT: conectando... ");
//   if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
//     Serial.println("OK");
//     updateDisplay("MQTT conectado", WiFi.localIP().toString());

//     client.publish(topic_temp_config, temp_config_payload, true);
//     client.publish(topic_hum_config, hum_config_payload, true);
//     discoverySent = true;

//     return true;
//   }

//   Serial.print("ERROR rc=");
//   Serial.println(client.state());
//   updateDisplay("MQTT fallo", "rc=" + String(client.state()));
//   return false;
// }

void ensureConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastWiFiAttempt >= wifiRetryInterval) {
      lastWiFiAttempt = now;
      Serial.println("WiFi perdida. Reintentando...");
      WiFi.disconnect();
      delay(200);
      connectWiFi();
    }
    return;
  }

  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastMQTTAttempt >= mqttRetryInterval) {
      lastMQTTAttempt = now;
      Serial.println("MQTT desconectado. Reintentando...");
      mqttConnect();
    }
    return;
  }
}

void publishSensorData() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Error: DHT11 no responde");
    updateDisplay("Error DHT11", wifiStatusText(), client.connected() ? "MQTT OK" : "MQTT OFF");
    return;
  }

  lastTemp = t;
  lastHum = h;

  char tempStr[16];
  char humStr[16];
  dtostrf(t, 4, 1, tempStr);
  dtostrf(h, 4, 1, humStr);

  bool ok1 = client.publish(topic_temp, tempStr, true);
  bool ok2 = client.publish(topic_hum, humStr, true);

  Serial.print("Publicando: Temp=");
  Serial.print(tempStr);
  Serial.print(" C Hum=");
  Serial.print(humStr);
  Serial.print(" % -> ");
  Serial.print(ok1 ? "TEMP OK " : "TEMP FAIL ");
  Serial.println(ok2 ? "HUM OK" : "HUM FAIL");

  updateDisplay(
    "Temp: " + String(tempStr) + " C",
    "Hum : " + String(humStr) + " %",
    wifiStatusText(),
    client.connected() ? "MQTT OK" : "MQTT OFF"
  );
}

void refreshDisplayIdle() {
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "sin IP";
  String mqtt = client.connected() ? "MQTT OK" : "MQTT OFF";

  String l1 = "Despacho 1";
  String l2;

  if (!isnan(lastTemp) && !isnan(lastHum)) {
    l2 = String(lastTemp, 1) + "C  " + String(lastHum, 1) + "%";
  } else {
    l2 = "Sin lectura";
  }

  updateDisplay(l1, l2, ip, mqtt);
}

// ... (mantén todo lo anterior igual hasta setup())

bool mqttConnect() {
  // 1. Verificar WiFi primero
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("MQTT: WiFi no conectado, no se puede conectar");
    return false;
  }

  // 2. Cliente ID estable (usa chip ID)
  String clientId = "despacho_1_" + String(ESP.getChipId(), HEX);
  
  Serial.print("MQTT: conectando... (");
  Serial.print(clientId);
  Serial.print(") ");

  // 3. Intentar conexión
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
    Serial.println("OK - conectado");
    
    // 4. Publicar discovery SOLO si estamos conectados
    Serial.println("MQTT: enviando discovery...");
    client.publish(topic_temp_config, temp_config_payload, true);
    client.publish(topic_hum_config, hum_config_payload, true);
    discoverySent = true;
    
    Serial.println("MQTT: discovery enviado");
    return true;
  }

  // 5. Error: mostrar motivo
  Serial.print("MQTT: ERROR - rc=");
  Serial.println(client.state());
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== Arrancando ===");

  dht.begin();
  Serial.println("1. DHT inicializado");

  Wire.begin(D2, D1);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("2. OLED no encontrada");
  } else {
    Serial.println("2. OLED inicializada");
  }

  // 3. Conectar WiFi
  connectWiFi();

  // 4. Configurar cliente MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(1024);

  // 5. Conectar MQTT SI WiFi está conectado
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("5. Intentando MQTT...");
    mqttConnect();
  } else {
    Serial.println("5. WiFi no conectado, no se intenta MQTT");
  }

  Serial.println("=== Arranque completado ===");
}

void loop() {
  // 1. Verificar WiFi y MQTT
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("loop: WiFi perdido");
    unsigned long now = millis();
    if (now - lastWiFiAttempt >= wifiRetryInterval) {
      lastWiFiAttempt = now;
      WiFi.disconnect();
      delay(200);
      connectWiFi();
    }
    delay(1000);
    return;
  }

  if (!client.connected()) {
    Serial.println("loop: MQTT desconectado");
    unsigned long now = millis();
    if (now - lastMQTTAttempt >= mqttRetryInterval) {
      lastMQTTAttempt = now;
      mqttConnect();
    }
    delay(1000);
    return;
  }

  // 2. Mantener loop MQTT
  client.loop();

  // 3. Publicar si es el intervalo
  unsigned long now = millis();
  if (now - lastPublish >= publishInterval) {
    lastPublish = now;

    // Publicar discovery si no se ha enviado
    if (!discoverySent) {
      Serial.println("loop: enviando discovery...");
      client.publish(topic_temp_config, temp_config_payload, true);
      client.publish(topic_hum_config, hum_config_payload, true);
      discoverySent = true;
      Serial.println("loop: discovery enviado");
    }

    // Publicar sensores
    publishSensorData();
  }

  delay(10);
}