#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "services/RespuestaService.h" // EstadoExamenResumen
#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <Adafruit_CST8XX.h>
#include <Adafruit_PCF8574.h>
#include <lvgl.h>
#include "ui/screens.h"
#include "session/SessionManager.h"
#include "network/NetworkMonitor.h"

/**
 * @file ui_manager.h
 * @brief Inicialización y ciclo de la pantalla táctil de la ONX3248G035
 * (ST7796 con TFT_eSPI, táctil CST826 y LVGL).
 *
 * La pantalla se apaga (backlight) tras 60 s sin tocarla y se despierta con
 * un toque; ese primer toque no se pasa a LVGL.
 */

/**
 * @brief Inicializa la pantalla, el táctil y LVGL, y carga la interfaz.
 *
 * Resetea el LCD por el expansor PCF8574, enciende el backlight y registra
 * en LVGL el driver de pantalla y el del táctil.
 *
 * @param isFirstBoot true para cargar la pantalla de setup (falta la
 *                    configuración inicial); false para las pantallas normales.
 */
void ui_init(bool isFirstBoot);

/**
 * @brief Mantiene viva la interfaz gráfica. Llamar en cada vuelta del loop()
 * de main.cpp.
 *
 * Ejecuta lv_timer_handler() cuando corresponde y, con la pantalla
 * encendida, actualiza usuario, cámara, dispositivos y examen. Si pasan
 * 60 s sin tocar la pantalla, apaga el backlight y detiene el ping a la
 * cámara.
 *
 * @param resumenExamen Último estado del examen (ver
 *                      RespuestaService::obtenerResumenCacheado()).
 */
void ui_loop(const EstadoExamenResumen & resumenExamen);

#endif
