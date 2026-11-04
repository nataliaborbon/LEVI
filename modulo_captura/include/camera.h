#pragma once
#include <Arduino.h>
#include "esp_camera.h"
#include "camera_pins.h"

/**
 * @file camera.h
 * @brief Inicialización de la cámara OV2640 y control del LED infrarrojo.
 *
 * La cámara queda capturando JPEG QVGA (320x240) de forma continua; el LED
 * IR solo se enciende mientras hay un cliente mirando el stream.
 */

/// @brief Pin del LED infrarrojo (se configura como salida en main.cpp).
#define IR_LED_PIN 13

/**
 * @brief Inicializa la cámara y ajusta el sensor.
 *
 * Configura los pines de camera_pins.h, XCLK a 16 MHz, JPEG QVGA con
 * calidad 16, 2 frame buffers en PSRAM y modo CAMERA_GRAB_LATEST (siempre
 * entrega el frame más nuevo). Deja ganancia, exposición y balance de
 * blancos automáticos, y el LED IR apagado.
 *
 * Si esp_camera_init() falla, lo informa por Serial y no hace nada más.
 *
 * @return true si la cámara se inicializó correctamente.
 */
bool startCamera();

/// @brief Enciende el LED infrarrojo al iniciar un stream.
void wakeCamera();

/// @brief Apaga el LED infrarrojo al terminar el stream.
void sleepCamera();
