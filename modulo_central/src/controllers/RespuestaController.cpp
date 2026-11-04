#include "controllers/RespuestaController.h"

// ---------------------------------------------------------------------------
// POST /api/alumno/iniciar
// ---------------------------------------------------------------------------
/**
 * @brief Inicia o recupera la sesión del alumno. Body: {"idDispositivo"}
 * (1 a 64 caracteres).
 *
 * Responde 200 con el token, o 409 con codigo "ALUMNO_OCUPADO" si hay otro
 * dispositivo conectado. La pestaña que lo pidió pasa a ser la activa.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleAlumnoIniciar(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index,
                                size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, "JSON inválido.");
        return;
    }

    String idDispositivo = doc["idDispositivo"] | "";
    if (idDispositivo.length() == 0 || idDispositivo.length() > 64) {
        enviarError(request, 400, "El campo 'idDispositivo' es obligatorio.");
        return;
    }

    SesionResult result = RespuestaService::getInstance().iniciarSesion(idDispositivo);

    if (!result.ok) {
        enviarError(request, 409, result.mensaje, result.codigo);
        return;
    }

    SessionManager::getInstance().asignarPestanaAlumno(result.token, extraerPestana(request));

    String json = "{\"ok\":true,\"token\":\"" + result.token + "\"}";
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// GET /api/alumno/estado
// ---------------------------------------------------------------------------
/**
 * @brief Devuelve el estado del examen y, si hay, la pregunta actual con
 * sus opciones (ver RespuestaService::obtenerEstado()).
 * @param request Request HTTP entrante.
 */
static void handleAlumnoEstado(AsyncWebServerRequest * request) {
    if (!verificarSesionAlumno(request))
        return;

    EstadoAlumno estado = RespuestaService::getInstance().obtenerEstado();

    String json = "{\"ok\":true,\"estado\":\"" + estado.estado + "\"";

    if (estado.estado == "en_progreso" || estado.estado == "invitado") {
        if (estado.hayPregunta) {
            json += ",\"pregunta\":{";
            json += "\"idPregunta\":" + String(estado.pregunta.idPregunta) + ",";
            json += "\"textoPregunta\":\"" + escapeJson(estado.pregunta.textoPregunta) + "\",";
            json += "\"numeroPregunta\":" + String(estado.pregunta.numeroPregunta) + ",";
            json += "\"totalPreguntas\":" + String(estado.pregunta.totalPreguntas) + ",";
            json += "\"opciones\":[";
            for (int i = 0; i < estado.pregunta.cantOpciones; i++) {
                if (i > 0)
                    json += ",";
                json += "{\"idOpcion\":" + String(estado.pregunta.opciones[i].idOpcion) + ",\"opcion\":\"" +
                        escapeJson(estado.pregunta.opciones[i].opcion) + "\"}";
            }
            json += "]}";
        }
    }

    json += "}";
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// POST /api/alumno/responder
// ---------------------------------------------------------------------------
/**
 * @brief Registra la opción elegida. Body: {"idPregunta", "idOpcion"}
 * (idPregunta 0 para la pregunta del invitado).
 *
 * Responde si fue correcta y si finalizó; si finalizó, agrega puntajes,
 * tiempo y si aprobó.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleAlumnoResponder(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index,
                                  size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionAlumno(request)) {
        body = "";
        return;
    }

    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, "JSON inválido.");
        return;
    }

    int idPregunta = doc["idPregunta"] | 0;
    int idOpcion = doc["idOpcion"] | -1;

    if (idOpcion < 0) {
        enviarError(request, 400, "El campo 'idOpcion' es obligatorio.");
        return;
    }

    RespuestaResult result = RespuestaService::getInstance().responder(idPregunta, idOpcion);

    if (!result.ok) {
        enviarError(request, 400, result.mensaje, result.codigo);
        return;
    }

    String json = "{\"ok\":true";
    json += ",\"fueCorrecto\":" + String(result.fueCorrecto ? "true" : "false");
    json += ",\"finalizo\":" + String(result.finalizo ? "true" : "false");
    if (result.finalizo) {
        json += ",\"puntajeObtenido\":" + String(result.resultado.puntajeObtenido);
        json += ",\"puntajeParaAprobar\":" + String(result.resultado.puntajeParaAprobar);
        json += ",\"puntajeMaximo\":" + String(result.resultado.puntajeMaximo);
        json += ",\"aprobado\":" + String(result.aprobado ? "true" : "false");
        json += ",\"tiempoSegundos\":" + String(result.resultado.tiempoSegundos);
    }
    if (result.mensaje.length() > 0) {
        json += ",\"mensaje\":\"" + escapeJson(result.mensaje) + "\"";
    }
    json += "}";

    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// POST /api/alumno/heartbeat
// ---------------------------------------------------------------------------
/**
 * @brief Solo mantiene viva la sesión del alumno.
 *
 * No mueve el cronómetro: el tiempo del examen lo cuenta /api/alumno/estado,
 * que el front pide solo en la pantalla de examen. No usa body (si viene,
 * se ignora).
 *
 * @param request Request HTTP entrante.
 */
static void handleAlumnoHeartbeat(AsyncWebServerRequest * request) {
    if (!verificarSesionAlumno(request))
        return;
    enviarOk(request, "ok");
}

// ---------------------------------------------------------------------------
// POST /api/alumno/salir
// ---------------------------------------------------------------------------
/**
 * @brief Cierra la sesión del alumno. Si había un examen en progreso, lo
 * pausa como en una desconexión (se reanuda si vuelve el mismo dispositivo).
 * @param request Request HTTP entrante.
 */
static void handleAlumnoSalir(AsyncWebServerRequest * request) {
    if (!verificarSesionAlumno(request))
        return;
    if (!SessionManager::getInstance().cerrarSesionAlumno(extraerToken(request))) {
        enviarError(request, 401, "Sesión de alumno inválida o expirada.", "SESION_CERRADA");
        return;
    }
    SessionManager::getInstance().procesarPendientes();
    enviarOk(request, "Sesión cerrada.");
}

// ---------------------------------------------------------------------------
// POST /api/invitado/pregunta
// ---------------------------------------------------------------------------
/**
 * @brief Guarda en RAM la pregunta del invitado. Body: {"pregunta",
 * "opciones": [...]} con 2 a 4 opciones (las que sobran se ignoran).
 *
 * Requiere sesión de invitado. Responde 409 "CUESTIONARIO_EN_CURSO" si hay
 * un cuestionario en progreso.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleInvitadoPregunta(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index,
                                   size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionPanel(request, "invitado")) {
        body = "";
        return;
    }

    if (CuestionarioRepository::getInstance().hayUnoEnProgreso()) {
        body = "";
        enviarError(request, 409, "Hay un cuestionario del profesor en curso. Probá más tarde.",
                    "CUESTIONARIO_EN_CURSO");
        return;
    }

    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, "JSON inválido.");
        return;
    }

    PreguntaInvitado pregunta;
    pregunta.textoOpregunta = doc["pregunta"] | "";

    if (pregunta.textoOpregunta.length() == 0) {
        enviarError(request, 400, "La pregunta no puede estar vacía.");
        return;
    }

    JsonArray opcsJson = doc["opciones"].as<JsonArray>();
    if (opcsJson.isNull()) {
        enviarError(request, 400, "El campo 'opciones' es obligatorio.");
        return;
    }

    pregunta.cantOpciones = 0;
    for (const char * opc : opcsJson) {
        if (pregunta.cantOpciones >= 4)
            break;
        pregunta.opciones[pregunta.cantOpciones++] = String(opc);
    }

    if (pregunta.cantOpciones < 2) {
        enviarError(request, 400, "Se necesitan al menos 2 opciones.");
        return;
    }

    pregunta.cargada = true;
    SessionManager::getInstance().guardarPreguntaInvitado(pregunta);

    enviarOk(request, "Pregunta guardada.");
}

// ---------------------------------------------------------------------------
// Registro de rutas
// ---------------------------------------------------------------------------
void registrarRespuestaController(AsyncWebServer & server) {
    server.on(
        "/api/alumno/iniciar", HTTP_POST,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleAlumnoIniciar);

    server.on("/api/alumno/estado", HTTP_GET, handleAlumnoEstado);

    server.on(
        "/api/alumno/responder", HTTP_POST,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleAlumnoResponder);

    server.on("/api/alumno/heartbeat", HTTP_POST, handleAlumnoHeartbeat);

    server.on("/api/alumno/salir", HTTP_POST, handleAlumnoSalir);

    server.on(
        "/api/invitado/pregunta", HTTP_POST,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleInvitadoPregunta);
}