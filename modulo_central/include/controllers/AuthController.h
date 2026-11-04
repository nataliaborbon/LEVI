#ifndef AUTH_CONTROLLER_H
#define AUTH_CONTROLLER_H

#include <ESPAsyncWebServer.h>
#include "middleware/Middleware.h"
#include "services/AuthService.h"
#include "services/UsuarioService.h"
#include "session/SessionManager.h"
#include <ArduinoJson.h>

/**
 * @file AuthController.h
 * @brief Rutas HTTP de autenticación.
 *
 * Rutas registradas:
 *   POST /api/auth/login      → login con usuario y contraseña
 *   POST /api/auth/invitado   → login como invitado
 *   POST /api/auth/logout     → cierre de sesión
 *   POST /api/auth/heartbeat  → mantener sesión activa (?interaccion=1 si
 *                               el usuario usó la pantalla)
 *   GET  /api/auth/perfil     → datos del usuario logueado
 *
 * Al hacer login, la pestaña que lo pidió (header X-Pestana) pasa a ser la
 * que controla la sesión.
 */

/**
 * @brief Registra las rutas de autenticación en el servidor.
 * @param server Instancia del servidor web.
 */
void registrarAuthController(AsyncWebServer & server);

#endif // AUTH_CONTROLLER_H
