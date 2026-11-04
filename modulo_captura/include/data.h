#ifndef DATA_H
#define DATA_H

#include <Arduino.h>

/**
 * @file data.h
 * @brief Credenciales WiFi e IPs fijas de la red LEVI.
 *
 * La red la crea como Access Point el módulo central; este módulo de
 * captura se conecta como cliente con IP fija para que siempre se lo
 * encuentre en la misma dirección.
 *
 * @note Deben coincidir con las de config/NetworkConfig.h del módulo central.
 */

#ifndef WIFI_SSID
/// @brief Nombre de la red WiFi
#define WIFI_SSID "LEVI"
#endif

#ifndef WIFI_PASSWORD
/// @brief Contraseña de la red WiFi
#define WIFI_PASSWORD "12345678"
#endif

/// @brief IP del módulo central (Access Point y gateway de la red).
const IPAddress IP_modulo_central(192, 168, 4, 1);

/// @brief IP fija de este módulo de captura (el central la busca en esta dirección).
const IPAddress IP_modulo_captura(192, 168, 4, 50);

/// @brief Máscara de subred.
const IPAddress MASCARA_SUBRED(255, 255, 255, 0);

#endif