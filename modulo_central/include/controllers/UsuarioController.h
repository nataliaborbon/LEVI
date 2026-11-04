#ifndef USUARIO_CONTROLLER_H
#define USUARIO_CONTROLLER_H

#include <ESPAsyncWebServer.h>
#include "middleware/Middleware.h"
#include "services/UsuarioService.h"
#include "session/SessionManager.h"
#include <ArduinoJson.h>
#include "config/Limites.h"

/**
 * @file UsuarioController.h
 * @brief Rutas HTTP de gestión de usuarios.
 *
 * Rutas registradas:
 *   POST   /api/usuarios                  → crear usuario (requiere clave maestra, sin sesión)
 *   DELETE /api/usuario?id=X              → eliminar usuario (tutor: profesores o
 *                                           su cuenta; profesor: su cuenta)
 *   PUT    /api/usuarios/perfil           → editar perfil propio
 *   PUT    /api/usuarios/perfil/password  → cambiar contraseña propia
 *   PUT    /api/usuarios/password         → recuperar contraseña con la clave
 *                                           maestra del rol (sin sesión)
 *   GET    /api/usuarios/profesores       → listar profesores, paginado (tutor y profesor)
 *   GET    /api/usuarios/tutores          → listar tutores, paginado (tutor y profesor)
 */

/**
 * @brief Registra las rutas de usuarios en el servidor.
 * @param server Instancia del servidor web.
 */
void registrarUsuarioController(AsyncWebServer & server);

#endif // USUARIO_CONTROLLER_H
