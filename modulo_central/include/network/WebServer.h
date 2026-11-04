#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include "controllers/AuthController.h"
#include "controllers/UsuarioController.h"
#include "controllers/CuestionarioController.h"
#include "controllers/RespuestaController.h"

/**
 * @file WebServer.h
 * @brief Inicialización del servidor web asíncrono (puerto 80).
 *
 * Gestiona dos orígenes de archivos estáticos:
 *   - LittleFS: build del front en Preact (index.html + assets).
 *   - SD:       opencv.js, servido con caché de un año.
 *
 * Además, redirige automáticamente los accesos realizados
 * mediante la dirección IP hacia levi.local para mantener
 * una URL consistente y amigable para el usuario.
 *
 * Las rutas de la API (/api/...) las registra cada Controller
 * desde initWebServer().
 */

/// @brief Servidor web asíncrono en el puerto 80.
extern AsyncWebServer server;

/**
 * @brief Registra las rutas de la API y las estáticas, e inicia el servidor.
 *
 * Rutas, en orden de registro:
 *   - /api/...      : las de cada Controller.
 *   - /opencv.js    : desde la SD (404 si no está).
 *   - /             : index.html, o redirección a http://levi.local si se
 *                     entró por IP.
 *   - Estáticos     : cualquier otro archivo de LittleFS.
 *   - No encontrado : responde los preflight OPTIONS de CORS, devuelve un
 *                     404 en JSON para /api/... desconocidas y, para el
 *                     resto, redirige a levi.local conservando la ruta o
 *                     devuelve index.html (rutas del router del front).
 *
 * Debe llamarse después de montar LittleFS y SD,
 * ya que sirve archivos desde ambos sistemas.
 */
void initWebServer();

#endif // WEB_SERVER_H