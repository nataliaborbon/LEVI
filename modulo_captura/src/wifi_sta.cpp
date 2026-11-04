#include "wifi_sta.h"

void startWifi() {
    while (true) {
        WiFi.mode(WIFI_STA);

        if (!WiFi.config(IP_modulo_captura, IP_modulo_central, MASCARA_SUBRED)) {
            Serial.println("Error configurando la IP estática");
        }

        WiFi.setSleep(false);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        Serial.print("Conectando a WiFi");

        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < TIMEOUT_CONEXION_WIFI_MS) {
            Serial.print(".");
            delay(500);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi conectado exitosamente");
            Serial.print("Dirección IP Fija: ");
            Serial.println(WiFi.localIP());
            return;
        }

        Serial.println("\nNo se pudo conectar al WiFi, reintentando...");
        WiFi.disconnect(true);
        delay(2000);
    }
}
