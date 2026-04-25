#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// -- Constantes hardware ----------

const int PIN_TMP36 = A0;
const float VOLT_REFERENCIA = 1.0;    // ESP8266 A0 max 1V
const int ADC_RESOLUCION = 1024;
const float DIVISOR_VOLTAJE = 2.0;    // R1=R2=10kΩ
const float TMP36_OFFSET = 0.5;       // 500mV a 0ºC
const float TMP36_ESCALA = 0.01;      // 10mV por ºC


// -- Constantes timing ------------

const unsigned long INTERVALO_SENSOR = 30000; // 30s entre lecturas
const unsigned long WIFI_RETRY_INTERVAL = 5000;
const unsigned long MQTT_RETRY_INTERVAL = 5000;

// -- Variables de estado ----------

unsigned long lastSensorRead = 0;
unsigned long lastWifiRetry = 0;
unsigned long lastMqttRetry = 0;

// -- Clientes ---------------------

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// -- Prototipos -------------------

void connectWifi();
void connectMqtt();
float leerTemperatura();
void publicarTemperatura(float temperatura);

// ---------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n== HomeLab IoT Sensor v1.0 ==");
  
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  
  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    connectMqtt();
  }
}

// ---------------------------------

void loop() {
  // --- Mantener conexión WiFi ----
  if (WiFi.status() != WL_CONNECTED) {
    // Reconexión automática sin bloquear el programa
    unsigned long now = millis();
    if (now - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
      lastWifiRetry = now;
      Serial.println("[WiFi] Conexión perdida. Reintentando...");
      connectWifi();
    }
    return; // sin WiFi no tiene sentido continuar
  }

  // --- Mantener conexión MQTT ----
  if (!mqttClient.connected()) {
    // Reconexión automática sin bloquear el programa
    unsigned long now = millis();
    if (now - lastMqttRetry >= MQTT_RETRY_INTERVAL) {
      lastMqttRetry = now;
      connectMqtt();
    }
    return; // sin MQTT no se publica
  }

  // --- Loop MQTT (mantiene conexión activa)
  mqttClient.loop();

  // --- Leer y publicar temperatura
  unsigned long now = millis();
  if (now - lastSensorRead >= INTERVALO_SENSOR) {
    lastSensorRead = now;
    float temperatura = leerTemperatura();

    if (temperatura > -40.0 && temperatura < 125.0) {
      publicarTemperatura(temperatura);
    } else {
      Serial.printf("[Sensor] Lectura fuera de rango: %.2fºC\n", temperatura);
    }
  }
}

// ---------------------------------

float leerTemperatura() {
  // Promedio de 10 lecturas para reducir ruido ADC
  int suma = 0;
  for (int i = 0; i < 10; i++) {
    suma += analogRead(PIN_TMP36);
    delay(10);
  }
  float valorADC = suma / 10.0;

  // Convertir ADC a voltaje (compensando divisor de voltaje)
  float voltaje = (valorADC / ADC_RESOLUCION) * VOLT_REFERENCIA * DIVISOR_VOLTAJE + VALOR_CALIBRACION;

  // Convertir voltaje a temperatura
  float temperatura = (voltaje - TMP36_OFFSET) / TMP36_ESCALA;

  Serial.printf("[Sensor] ADC: %.0f | Voltaje: %.3fV | Temp: %.2fºC\n", valorADC, voltaje, temperatura);

  return temperatura;
}

// ---------------------------------

void publicarTemperatura(float temperatura) {
  // Construir payload JSON
  StaticJsonDocument<128> doc;
  doc["dispositivo"] = MQTT_CLIENT_ID;
  doc["temperatura"] = serialized(String(temperatura, 2));
  doc["unidad"] = "C";
  doc["rssi"] = WiFi.RSSI();

  char payload[128];
  serializeJson(doc, payload);

  // Publicar con retain=true para que HA tenga siempre el último valor disponible
  bool ok = mqttClient.publish(TOPIC_TEMPERATURA, payload, true);

  if (ok) {
    Serial.printf("[MQTT] Publicado en %s: %s\n", TOPIC_TEMPERATURA, payload);
  } else {
    Serial.println("[MQTT] Error al publicar");
  }
}

// ---------------------------------

void connectWifi() {
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);    // modo cliente, no punto de acceso
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Conectado!");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n[WiFi] Fallo. Reintentando en 5s...");
  }
}

// ---------------------------------

void connectMqtt() {
  Serial.printf("[MQTT] Conectando a %s:%d...\n", MQTT_SERVER, MQTT_PORT);

  // Mensaje de último testamento - HA sabrá si el dispositivo se desconecta inesperadamente
  const char* willTopic = TOPIC_STATUS;
  const char* willMessage = "{\"estado\":\"offline\"}";

  bool conectado = mqttClient.connect(
    MQTT_CLIENT_ID,
    MQTT_USER,
    MQTT_PASSWORD,
    willTopic,
    1,              // QoS
    true,           // retain
    willMessage
  );

  if (conectado) {
    Serial.println("[MQTT] Conectado!");
    // Publicar estado online
    mqttClient.publish(TOPIC_STATUS, "{\"estado\":\"online\"}", true);
  } else {
    Serial.printf("[MQTT] Fallo. Código: %d\n", mqttClient.state());
  }
}



