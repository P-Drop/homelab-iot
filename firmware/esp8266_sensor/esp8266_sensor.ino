#include <ESP8266WiFi.h>
#include "config.h"

// Intervalo de reintento Wifi (ms)
const unsigned long WIFI_RETRY_INTERVAL = 5000;
unsigned long lastWifiRetry = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n== HomeLab IoT Sensor ==");
  connectWifi();
}

void loop() {
  // Reconexión automática sin bloquear el programa
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
      lastWifiRetry = now;
      Serial.println("[WiFi] Conexión perdida. Reintentando...");
      connectWifi();
    }
  }

  // Aquí irá la lógica del sensor en FW-03
}

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