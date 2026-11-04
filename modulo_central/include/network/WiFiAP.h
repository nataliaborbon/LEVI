#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <WiFi.h>
#include <ESPmDNS.h>

#include "../config/NetworkConfig.h"

/**
 * @file WiFiAP.h
 * @brief Configura el módulo central como Access Point WiFi.
 *
 * Además de crear la red LEVI, publica el nombre
 * levi.local mediante mDNS para permitir el acceso
 * sin necesidad de conocer la dirección IP.
 *
 * El sistema puede accederse mediante:
 *   http://levi.local
 *   http://192.168.4.1
 */

/**
 * @brief Levanta el Access Point con IP estática.
 *
 * Configura la IP, gateway y subred antes de crear
 * la red WiFi, garantizando que el módulo central siempre sea
 * accesible en IP_modulo_central. La red usa el canal 1, es
 * visible y acepta hasta 4 clientes a la vez.
 *
 * También inicia el servicio mDNS para publicar
 * el nombre levi.local.
 *
 * Si no se puede crear la red, lo informa por Serial
 * y no inicia mDNS.
 */
void initWiFiAP();

#endif // WIFI_AP_H