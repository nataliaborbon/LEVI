#ifndef RESPUESTA_CONTROLLER_H
#define RESPUESTA_CONTROLLER_H

#include <ESPAsyncWebServer.h>
#include "middleware/Middleware.h"
#include "services/RespuestaService.h"
#include "storage/database/repositories/CuestionarioRepository.h"
#include "session/SessionManager.h"
#include <ArduinoJson.h>

/**
 * @file RespuestaController.h
 * @brief Rutas HTTP del flujo del alumno y del invitado.
 *
 * Rutas registradas:
 *   POST /api/alumno/iniciar    → obtener (o recuperar) el token de sesión alumno
 *   GET  /api/alumno/estado     → qué mostrar en pantalla (también hace
 *                                 avanzar el cronómetro del examen)
 *   POST /api/alumno/responder  → registrar opción elegida
 *   POST /api/alumno/heartbeat  → mantener sesión activa
 *   POST /api/alumno/salir      → cerrar la sesión del alumno
 *   POST /api/invitado/pregunta → guardar pregunta del invitado en RAM
 */

/**
 * @brief Registra las rutas del alumno e invitado en el servidor.
 * @param server Instancia del servidor web.
 */
void registrarRespuestaController(AsyncWebServer & server);

#endif // RESPUESTA_CONTROLLER_H
