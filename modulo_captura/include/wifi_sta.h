#pragma once
#include <WiFi.h>
#include "data.h"

/**
 * @file wifi_sta.h
 * @brief Conexión de la cámara a la red WiFi LEVI como cliente (Station).
 */

/// @brief Tiempo máximo (ms) de cada intento de conexión al WiFi.
const unsigned long TIMEOUT_CONEXION_WIFI_MS = 15000;

/**
 * @brief Conecta el módulo de captura como cliente (Station) a la red WiFi
 * creada por el módulo central, con la IP fija IP_modulo_captura.
 *
 * Desactiva el ahorro de energía del WiFi para no agregar latencia al
 * stream. Si no se conecta en TIMEOUT_CONEXION_WIFI_MS, se desconecta,
 * espera 2 s y vuelve a intentar: no retorna hasta estar conectado.
 */
void startWifi();