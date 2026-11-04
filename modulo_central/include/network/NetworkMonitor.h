#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <cstdint>
#include "config/NetworkConfig.h"
#include <WiFi.h>
#include "ping/ping_sock.h"
#include "lwip/inet.h"

/**
 * @file NetworkMonitor.h
 * @brief Cantidad de dispositivos en el AP (lista WiFi) y estado de la cámara (ping activo).
 *
 * El estado de la cámara se obtiene con una sesión de ping de ESP-IDF hacia
 * IP_modulo_captura: un ping cada 2 s con 500 ms de timeout. La sesión corre en su
 * propio task, así que se destruye mientras la pantalla está suspendida.
 */

/**
 * @brief Arranca la sesión de ping hacia la cámara. Llamar una vez en
 * setup(), después de que el AP ya esté levantado.
 */
void networkMonitor_init();

/**
 * @brief Actualiza la cantidad de dispositivos conectados al AP.
 * @note La llama ui_loop() mientras la pantalla está encendida.
 */
void networkMonitor_loop();

/**
 * @brief Cantidad de dispositivos conectados al AP, según la última
 * llamada a networkMonitor_loop().
 * @return Cantidad de estaciones (incluye a la cámara si está conectada).
 */
int networkMonitor_getCantidadDispositivos();

/**
 * @brief Indica si la cámara (IP_modulo_captura) respondió el último ping.
 * @return true si respondió; false si no respondió o si el ping está detenido.
 */
bool networkMonitor_isCamaraConectada();

/**
 * @brief Detiene y destruye la sesión de ping hacia la cámara.
 *
 * Se usa cuando la pantalla se suspende por inactividad. Deja la cámara
 * marcada como desconectada, porque sin ping no hay forma de saber su
 * estado real. Si ya estaba detenido no hace nada.
 */
void networkMonitor_detenerPingCamara();

/**
 * @brief Vuelve a crear la sesión de ping hacia la cámara.
 *
 * Se usa cuando la pantalla se despierta. Si el ping ya estaba corriendo
 * no hace nada.
 */
void networkMonitor_reanudarPingCamara();

#endif // NETWORK_MONITOR_H