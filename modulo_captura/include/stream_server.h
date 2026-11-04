#pragma once
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "camera.h"

/**
 * @file stream_server.h
 * @brief Servidor HTTP que transmite el video de la cámara como MJPEG.
 */

/**
 * @brief Inicia el servidor HTTP en el puerto 80 con la ruta GET /stream.
 *
 * /stream responde un multipart/x-mixed-replace con un JPEG por parte, a
 * 20 FPS como máximo y con CORS abierto. Mientras hay un cliente conectado
 * el LED IR queda encendido. El servidor atiende un solo cliente a la vez.
 */
void startStreamServer();