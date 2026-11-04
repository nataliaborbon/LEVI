#ifndef SCREENS_H
#define SCREENS_H

#include <lvgl.h>
#include <string.h>
#include <Arduino.h>
#include "config/ConfigManager.h"
#include "images/qr_levi_local.c"
#include "images/qr_levi_local_x2.c"

/**
 * @file screens.h
 * @brief Pantallas LVGL de la placa (implementadas en screen_main.cpp y
 * screen_setup.cpp).
 *
 * Las imágenes del QR se incluyen acá porque son datos const: cada archivo
 * que incluye este header tiene su propia copia interna y el compilador
 * descarta las que no usa.
 */

/**
 * @brief Crea el Tileview con las 3 pantallas normales, que se cambian
 * deslizando en horizontal:
 *   1. Bienvenida, con el QR para entrar a levi.local.
 *   2. Estado: alumno, usuario del panel, cámara y dispositivos conectados.
 *   3. Examen: esperando, en progreso o finalizado.
 */
void ui_screen_main_init();

/**
 * @brief Crea la pantalla de primer uso, donde se cargan el nombre del
 * alumno y las claves maestras de profesor y tutor.
 *
 * Al guardar, los escribe en NVS (ConfigManager) y reinicia la placa.
 */
void ui_screen_setup_init();

/**
 * @brief Muestra el usuario de la sesión del panel en la pantalla de estado.
 * @param nombre Nombre a mostrar; si es vacío muestra "Sin conexion".
 */
void ui_update_usuario(const char * nombre);

/**
 * @brief Muestra la cantidad de dispositivos conectados al AP.
 * @param cantidad Cantidad de dispositivos.
 */
void ui_update_dispositivos(int cantidad);

/**
 * @brief Muestra el estado de la cámara ("Transmitiendo" o "Desconectada").
 * @param lista true si la cámara respondió el ping.
 */
void ui_update_camara(bool lista);

/**
 * @brief Actualiza la pantalla de examen según el estado.
 *
 *   - "esperando" / "sin_sesion": muestra "Esperando cuestionario".
 *   - "finalizado": muestra puntajes, tiempo y si aprobó, con una cuenta
 *     regresiva de 10 s para volver a "esperando". Solo se dibuja al
 *     entrar a este estado, no en cada llamada.
 *   - Cualquier otro ("en_progreso", "invitado"): muestra el título y el
 *     progreso numeroPregunta/totalPreguntas.
 *
 * @param estado             Estado del examen (ver EstadoExamenResumen).
 * @param titulo             Título del cuestionario.
 * @param numeroPregunta     Número de la pregunta actual.
 * @param totalPreguntas     Total de preguntas.
 * @param puntajeObtenido    Solo para "finalizado".
 * @param puntajeParaAprobar Solo para "finalizado".
 * @param puntajeMaximo      Solo para "finalizado".
 * @param tiempoSegundos     Solo para "finalizado".
 * @param aprobado           Solo para "finalizado".
 */
void ui_update_examen(const char * estado, const char * titulo, int numeroPregunta, int totalPreguntas,
                      float puntajeObtenido = 0, float puntajeParaAprobar = 0, float puntajeMaximo = 0,
                      float tiempoSegundos = 0, bool aprobado = false);

#endif
