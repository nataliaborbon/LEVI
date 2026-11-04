#ifndef CUESTIONARIO_CONTROLLER_H
#define CUESTIONARIO_CONTROLLER_H

#include <ESPAsyncWebServer.h>
#include "storage/database/repositories/CuestionarioRepository.h"
#include "middleware/Middleware.h"
#include "services/CuestionarioService.h"
#include "session/SessionManager.h"
#include <ArduinoJson.h>
#include "config/Limites.h"

/**
 * @file CuestionarioController.h
 * @brief Rutas HTTP de gestión de cuestionarios.
 *
 * Rutas registradas:
 *   GET    /api/cuestionarios?pagina=N            → listar paginado (profesor: los
 *                                                   suyos, tutor: todos)
 *   POST   /api/cuestionarios                     → crear cuestionario completo
 *   GET    /api/cuestionario?id=X                 → cuestionario completo (para editar,
 *                                                   solo el profesor dueño)
 *   PUT    /api/cuestionario?id=X                 → editar título, puntaje y preguntas
 *                                                   (solo pendientes)
 *   DELETE /api/cuestionario?id=X                 → eliminar
 *   PATCH  /api/cuestionario/iniciar?id=X         → pendiente → en_progreso
 *   PATCH  /api/cuestionario/pausar?id=X          → en_progreso → pausado
 *   PATCH  /api/cuestionario/reanudar?id=X        → pausado → en_progreso
 *   PATCH  /api/cuestionario/finalizar?id=X       → en_progreso | pausado → finalizado
 *   GET    /api/cuestionario/revision?id=X        → detalle con respuestas del alumno
 *                                                   (tutor: todos, profesor: los suyos)
 *
 * Crear, editar, eliminar y cambiar de estado requieren sesión de profesor.
 */

/**
 * @brief Registra las rutas de cuestionarios en el servidor.
 * @param server Instancia del servidor web.
 */
void registrarCuestionarioController(AsyncWebServer & server);

#endif // CUESTIONARIO_CONTROLLER_H
