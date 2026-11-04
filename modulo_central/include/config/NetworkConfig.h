#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <IPAddress.h>

/**
 * @file NetworkConfig.h
 * @brief Credenciales WiFi e IPs fijas de la red local del sistema.
 *
 * El módulo central opera como Access Point. Las IPs son estáticas para
 * que el módulo de captura (ESP32-CAM) siempre sea encontrable en la
 * misma dirección.
 *
 * @note Deben coincidir con las de data.h del firmware del módulo de captura.
 */

// ---------------------------------------------------------------------------
// Credenciales de la red WiFi
// ---------------------------------------------------------------------------

#ifndef WIFI_SSID
/// @brief Nombre de la red WiFi creada por el módulo central.
#define WIFI_SSID "LEVI"
#endif

#ifndef WIFI_PASSWORD
/// @brief Contraseña de la red WiFi creada por el módulo central.
#define WIFI_PASSWORD "12345678"
#endif

// ---------------------------------------------------------------------------
// IPs estáticas de la red
// ---------------------------------------------------------------------------

/// @brief IP del módulo central (Access Point, gateway de la red y servidor web).
const IPAddress IP_modulo_central(192, 168, 4, 1);

/// @brief IP fija del módulo de captura (ESP32-CAM).
const IPAddress IP_modulo_captura(192, 168, 4, 50);

/// @brief Máscara de subred.
const IPAddress MASCARA_SUBRED(255, 255, 255, 0);

#endif // NETWORK_CONFIG_H
