#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "../session/SessionManager.h"
#include "config/Limites.h"

/**
 * @file Middleware.h
 * @brief Funciones auxiliares para verificación de tokens y respuestas HTTP.
 *
 * Todos los controllers usan estas funciones para:
 *   - Extraer el token del header Authorization.
 *   - Verificar que la sesión sea válida antes de procesar el request.
 *   - Enviar respuestas JSON estandarizadas (con CORS abierto).
 *   - Juntar bodies fragmentados y armar la paginación.
 */

/**
 * @brief Extrae el token del header "Authorization: Bearer <token>".
 * @param request Request HTTP entrante.
 * @return Token extraído, o cadena vacía si no existe o tiene formato incorrecto.
 */
String extraerToken(AsyncWebServerRequest * request);

/**
 * @brief Extrae el id de la pestaña del header "X-Pestana".
 * @param request Request HTTP entrante.
 * @return Id de la pestaña, o cadena vacía si no viene.
 */
String extraerPestana(AsyncWebServerRequest * request);

/**
 * @brief Verifica que haya una sesión panel válida para el request.
 *
 * Antes de verificar ejecuta SessionManager::procesarPendientes().
 *
 * Si la sesión no es válida responde 401 con codigo "SESION_CERRADA" y, si
 * se sabe, el motivo del cierre ("inactividad", "desconexion", "expulsado",
 * "reemplazada", "logout").
 *
 * Si la sesión es válida pero el request viene de una pestaña que no es la
 * activa, responde 409 con codigo "OTRA_PESTANA". Con el header
 * "X-Tomar-Pestana: 1" la pestaña pasa a ser la activa.
 *
 * Si el rol no es el requerido responde 403 con codigo "SIN_PERMISO".
 *
 * Si todo está bien renueva la sesión (ver SessionManager::heartbeatPanel()).
 *
 * @param request      Request HTTP entrante.
 * @param rolRequerido Rol exacto requerido. Vacío = cualquier rol de panel.
 * @param interaccion  true si el request cuenta como uso de la pantalla
 *                     (todos menos el heartbeat automático).
 * @return true si la sesión es válida y tiene el rol correcto.
 */
bool verificarSesionPanel(AsyncWebServerRequest * request, const String & rolRequerido = "", bool interaccion = true);

/**
 * @brief Verifica que haya una sesión de alumno válida para el request.
 *
 * Antes de verificar ejecuta SessionManager::procesarPendientes(). Si la
 * sesión no es válida responde 401 "SESION_CERRADA" (sin motivo). Igual
 * que en el panel, solo acepta la pestaña activa (409 "OTRA_PESTANA").
 * Si todo está bien renueva la sesión del alumno.
 *
 * @param request Request HTTP entrante.
 * @return true si la sesión de alumno es válida.
 */
bool verificarSesionAlumno(AsyncWebServerRequest * request);

/**
 * @brief Envía una respuesta JSON con código de estado y el header
 * Access-Control-Allow-Origin: *.
 * @param request Request HTTP entrante.
 * @param codigo  Código HTTP (200, 201, 400, 401, 403, 404, 409, 500).
 * @param json    Cuerpo JSON como String.
 */
void enviarJSON(AsyncWebServerRequest * request, int codigo, const String & json);

/**
 * @brief Envía una respuesta de error estandarizada:
 * {"ok":false,"codigo":"...","mensaje":"..."}.
 * @param request     Request HTTP entrante.
 * @param codigo      Código HTTP de error.
 * @param mensaje     Mensaje de error legible.
 * @param codigoError Código para que el front distinga el error sin leer el
 *                    mensaje (ej: "ALUMNO_OCUPADO"). Vacío = no se envía.
 */
void enviarError(AsyncWebServerRequest * request, int codigo, const String & mensaje, const String & codigoError = "");

/**
 * @brief Junta en 'body' los pedazos del cuerpo de un request.
 *
 * AsyncWebServer entrega el body en fragmentos si es grande. Llamar en cada
 * fragmento desde el handler de body. El primer fragmento (index 0) vacía
 * 'body' y reserva el tamaño total.
 *
 * @param body  Acumulador del body.
 * @param data  Datos del fragmento recibido.
 * @param len   Largo del fragmento.
 * @param index Posición del fragmento dentro del body.
 * @param total Largo total del body.
 * @return true cuando ya llegó el body completo.
 */
bool acumularBody(String & body, uint8_t * data, size_t len, size_t index, size_t total);

/**
 * @brief Envía una respuesta 200 OK con mensaje de éxito:
 * {"ok":true,"mensaje":"..."}.
 * @param request  Request HTTP entrante.
 * @param mensaje  Mensaje de éxito.
 */
void enviarOk(AsyncWebServerRequest * request, const String & mensaje);

/**
 * @brief Escapa un texto para poder meterlo entre comillas dentro de un JSON.
 *
 * Reemplaza \ por \\, " por \", saltos de línea por \n y elimina \r.
 * Usar en todo texto que venga del usuario o de la BD (títulos, preguntas,
 * opciones, mensajes) al armar JSON a mano.
 *
 * @param texto Texto original.
 * @return Texto escapado.
 */
String escapeJson(String texto);

/**
 * @brief Lee el parámetro de query "pagina" de un listado paginado.
 *
 * Si falta, no es un número o es menor que 1, devuelve 1.
 *
 * @param request Request HTTP entrante.
 * @return Número de página (empieza en 1).
 */
int obtenerPagina(AsyncWebServerRequest * request);

/**
 * @brief Arma los campos de paginación para agregar a la respuesta JSON
 * (sin llaves ni coma inicial):
 * "pagina":N,"porPagina":ELEMENTOS_POR_PAGINA,"total":T,"totalPaginas":P
 *
 * totalPaginas vale al menos 1, aunque no haya elementos.
 *
 * @param pagina Página pedida.
 * @param total  Cantidad total de elementos.
 * @return Texto con los campos.
 */
String jsonPaginacion(int pagina, int total);

#endif // MIDDLEWARE_H
