#include "network/WiFiAP.h"

void initWiFiAP() {
    WiFi.softAPConfig(IP_modulo_central, IP_modulo_central, MASCARA_SUBRED);

    if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 1, false, 4)) {
        Serial.println("[WiFi] Error: no se pudo crear la red WiFi.");
        return;
    }

    Serial.printf("[WiFi] Red '%s' creada.\n"
                  "[WiFi] Acceso por:\n"
                  "   http://%s\n"
                  "   http://levi.local\n",
                  WIFI_SSID, WiFi.softAPIP().toString().c_str());

    if (MDNS.begin("levi")) {
        Serial.println("[mDNS] Nombre publicado: levi.local");
    } else {
        Serial.println("[mDNS] Error al iniciar mDNS.");
    }
}