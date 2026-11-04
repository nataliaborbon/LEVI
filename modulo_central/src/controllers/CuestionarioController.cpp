#include "controllers/CuestionarioController.h"

// ---------------------------------------------------------------------------
// GET /api/cuestionarios
// ---------------------------------------------------------------------------

/**
 * @brief Agrega a 'json' los campos comunes de un resumen de cuestionario
 * (sin las llaves). Sirve para profesor y tutor, porque el resumen del
 * tutor hereda del del profesor.
 * @param json JSON que se está armando.
 * @param r    Resumen a agregar.
 */
static void agregarCamposResumen(String & json, const CuestionarioResumenProfesor & r) {
    json += "\"idCuestionario\":" + String(r.idCuestionario) + ",";
    json += "\"titulo\":\"" + escapeJson(r.titulo) + "\",";
    json += "\"estado\":\"" + r.estado + "\",";
    json += "\"puntajeObtenido\":" + String(r.puntajeObtenido) + ",";
    json += "\"puntajeParaAprobar\":" + String(r.puntajeParaAprobar) + ",";
    json += "\"cantPreguntas\":" + String(r.cantPreguntas) + ",";
    json += "\"fechaInicio\":\"" + r.fechaInicio + "\",";
    json += "\"aprobado\":" + String(r.aprobado ? "true" : "false");
}

/**
 * @brief Lista una página de cuestionarios: los propios si es profesor,
 * todos (con la materia) si es tutor. 403 para el invitado.
 * @param request Request HTTP entrante.
 */
static void handleListar(AsyncWebServerRequest * request) {
    if (!verificarSesionPanel(request))
        return;
    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();
    if (sesion.rol == "invitado") {
        enviarError(request, 403, "Sin permisos.");
        return;
    }

    int pagina = obtenerPagina(request);
    int offset = (pagina - 1) * ELEMENTOS_POR_PAGINA;
    int total = 0;

    String json = "{\"ok\":true,\"cuestionarios\":[";
    if (sesion.rol == "profesor") {
        total = CuestionarioService::getInstance().contarProfesor(sesion.idUsuario);

        CuestionarioResumenProfesor buffer[ELEMENTOS_POR_PAGINA];
        int cant =
            CuestionarioService::getInstance().listarProfesor(sesion.idUsuario, buffer, ELEMENTOS_POR_PAGINA, offset);
        for (int i = 0; i < cant; i++) {
            if (i > 0)
                json += ",";
            json += "{";
            agregarCamposResumen(json, buffer[i]);
            json += "}";
        }
    } else {
        total = CuestionarioService::getInstance().contarTutor();

        CuestionarioResumenTutor buffer[ELEMENTOS_POR_PAGINA];
        int cant = CuestionarioService::getInstance().listarTutor(buffer, ELEMENTOS_POR_PAGINA, offset);
        for (int i = 0; i < cant; i++) {
            if (i > 0)
                json += ",";
            json += "{";
            agregarCamposResumen(json, buffer[i]);
            json += ",\"materia\":\"" + escapeJson(buffer[i].materia) + "\"}";
        }
    }
    json += "]," + jsonPaginacion(pagina, total) + "}";
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// GET /api/cuestionario?id=X
// ---------------------------------------------------------------------------
/**
 * @brief Devuelve el cuestionario del parámetro "id" con sus preguntas y
 * opciones (marcando la correcta), para cargarlo en el editor.
 * @note Solo para el profesor dueño: incluye las respuestas correctas.
 * @param request Request HTTP entrante.
 */
static void handleObtener(AsyncWebServerRequest * request) {
    if (!verificarSesionPanel(request, "profesor"))
        return;
    if (!request->hasParam("id")) {
        enviarError(request, 400, "Falta ID");
        return;
    }
    int idCuestionario = request->getParam("id")->value().toInt();

    Cuestionario c;
    PreguntaCompleta * preguntas = new PreguntaCompleta[MAX_PREGUNTAS_POR_CUESTIONARIO];
    int cantPreguntas = 0;

    if (!CuestionarioService::getInstance().obtenerCompleto(idCuestionario, c, preguntas, cantPreguntas)) {
        delete[] preguntas;
        enviarError(request, 404, "No encontrado.");
        return;
    }

    if (c.idUsuario != SessionManager::getInstance().getSesionPanel().idUsuario) {
        delete[] preguntas;
        enviarError(request, 403, "No tenés permisos para ver este cuestionario.", "SIN_PERMISO");
        return;
    }

    String json;
    json.reserve(2048);
    json += "{\"ok\":true,";
    json += "\"titulo\":\"" + escapeJson(c.titulo) + "\",";
    json += "\"puntajeParaAprobar\":" + String(c.puntajeParaAprobar) + ",";
    json += "\"preguntas\":[";

    for (int i = 0; i < cantPreguntas; i++) {
        if (i > 0)
            json += ",";
        json += "{\"pregunta\":\"" + escapeJson(preguntas[i].pregunta.pregunta) + "\",";
        json += "\"puntajeCorrecta\":" + String(preguntas[i].pregunta.puntajeCorrecta) + ",";
        json += "\"puntajeIncorrecta\":" + String(preguntas[i].pregunta.puntajeIncorrecta) + ",";
        json += "\"opciones\":[";
        for (int j = 0; j < preguntas[i].cantOpciones; j++) {
            if (j > 0)
                json += ",";
            json += "{\"opcion\":\"" + escapeJson(preguntas[i].opciones[j].opcion) + "\",";
            json += "\"esCorrecta\":" + String(preguntas[i].opciones[j].esCorrecta ? "true" : "false") + "}";
        }
        json += "]}";
    }
    json += "]}";
    delete[] preguntas;
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// Helpers para crear / editar
// ---------------------------------------------------------------------------

/**
 * @brief Pasa el array "preguntas" del JSON a PreguntaCompleta.
 *
 * Solo controla los límites de tamaño; el resto de la validación la hace
 * CuestionarioService.
 *
 * @param preguntasJson Array "preguntas" del body.
 * @param preguntas     Array destino de MAX_PREGUNTAS_POR_CUESTIONARIO.
 * @param cant          Se completa con la cantidad de preguntas.
 * @param error         Se llena con el motivo si falla.
 * @return false si falta el array, si hay más de MAX_PREGUNTAS_POR_CUESTIONARIO
 *         o si alguna pregunta tiene más de MAX_OPCIONES_POR_PREGUNTA.
 */
static bool parsearPreguntas(JsonArray preguntasJson, PreguntaCompleta * preguntas, int & cant, String & error) {
    if (preguntasJson.isNull()) {
        error = "Falta 'preguntas'.";
        return false;
    }

    if (preguntasJson.size() > MAX_PREGUNTAS_POR_CUESTIONARIO) {
        error = "Máximo " + String(MAX_PREGUNTAS_POR_CUESTIONARIO) + " preguntas por cuestionario.";
        return false;
    }

    cant = 0;

    for (JsonObject pj : preguntasJson) {
        PreguntaCompleta & p = preguntas[cant];

        p.pregunta.pregunta = pj["pregunta"] | "";
        p.pregunta.puntajeCorrecta = pj["puntajeCorrecta"] | 0.0f;
        p.pregunta.puntajeIncorrecta = pj["puntajeIncorrecta"] | 0.0f;

        JsonArray opcs = pj["opciones"].as<JsonArray>();

        if (opcs.size() > MAX_OPCIONES_POR_PREGUNTA) {
            error =
                "La pregunta " + String(cant + 1) + " tiene más de " + String(MAX_OPCIONES_POR_PREGUNTA) + " opciones.";
            return false;
        }

        p.cantOpciones = 0;

        for (JsonObject oj : opcs) {
            int j = p.cantOpciones++;
            p.opciones[j].opcion = oj["opcion"] | "";
            p.opciones[j].esCorrecta = oj["esCorrecta"] | false;
        }

        cant++;
    }

    return true;
}

// ---------------------------------------------------------------------------
// POST /api/cuestionarios (Crear Nuevo)
// ---------------------------------------------------------------------------
/**
 * @brief Crea un cuestionario del profesor logueado. Body: {"titulo",
 * "puntajeParaAprobar", "preguntas": [{"pregunta", "puntajeCorrecta",
 * "puntajeIncorrecta", "opciones": [{"opcion", "esCorrecta"}]}]}.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleCrear(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index, size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionPanel(request, "profesor")) {
        body = "";
        return;
    }

    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();

    DynamicJsonDocument doc(body.length() + 2048);

    DeserializationError err = deserializeJson(doc, body);

    if (err) {
        body = "";
        enviarError(request, 400, err.c_str());
        return;
    }

    body = "";

    Cuestionario c;
    c.idUsuario = sesion.idUsuario;
    c.titulo = doc["titulo"] | "";
    c.puntajeParaAprobar = doc["puntajeParaAprobar"] | 0.0f;

    PreguntaCompleta preguntas[MAX_PREGUNTAS_POR_CUESTIONARIO];
    int cantPreguntas = 0;
    String error;

    if (!parsearPreguntas(doc["preguntas"].as<JsonArray>(), preguntas, cantPreguntas, error)) {
        enviarError(request, 400, error);
        return;
    }

    CuestionarioResult result = CuestionarioService::getInstance().crear(c, preguntas, cantPreguntas);

    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }

    String json = "{\"ok\":true,\"idCuestionario\":" + String(result.id) + "}";

    enviarJSON(request, 201, json);
}
// ---------------------------------------------------------------------------
// PUT /api/cuestionario?id=X Editar
// ---------------------------------------------------------------------------
/**
 * @brief Reemplaza título, puntaje y preguntas del cuestionario del
 * parámetro "id". Mismo body que al crear.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleEditar(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index, size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionPanel(request, "profesor")) {
        body = "";
        return;
    }
    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();

    if (!request->hasParam("id")) {
        body = "";
        enviarError(request, 400, "Falta ID");
        return;
    }
    int idCuestionario = request->getParam("id")->value().toInt();

    DynamicJsonDocument doc(body.length() + 2048);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, err.c_str());
        return;
    }

    Cuestionario c;
    c.idCuestionario = idCuestionario;
    c.idUsuario = sesion.idUsuario;
    c.titulo = doc["titulo"] | "";
    c.puntajeParaAprobar = doc["puntajeParaAprobar"] | 0.0f;

    PreguntaCompleta preguntas[MAX_PREGUNTAS_POR_CUESTIONARIO];
    int cantPreguntas = 0;
    String error;

    if (!parsearPreguntas(doc["preguntas"].as<JsonArray>(), preguntas, cantPreguntas, error)) {
        enviarError(request, 400, error);
        return;
    }

    CuestionarioResult result = CuestionarioService::getInstance().editar(c, preguntas, cantPreguntas);
    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }
    enviarOk(request, "Cuestionario actualizado.");
}

// ---------------------------------------------------------------------------
// Acciones Básicas y Registro
// ---------------------------------------------------------------------------
/**
 * @brief Elimina el cuestionario del parámetro "id".
 * @param r Request HTTP entrante.
 */
static void handleEliminar(AsyncWebServerRequest * r) {
    if (!verificarSesionPanel(r, "profesor"))
        return;
    if (!r->hasParam("id")) {
        enviarError(r, 400, "Falta ID");
        return;
    }
    CuestionarioResult result = CuestionarioService::getInstance().eliminar(
        r->getParam("id")->value().toInt(), SessionManager::getInstance().getSesionPanel().idUsuario);
    if (!result.ok)
        enviarError(r, 400, result.mensaje);
    else
        enviarOk(r, "Eliminado.");
}

/**
 * @brief Inicia el cuestionario del parámetro "id". Body: {"fechaISO"}.
 * @details Olvida una pausa por desconexión anterior.
 * @param r     Request HTTP entrante.
 * @param data  Fragmento del body.
 * @param len   Largo del fragmento.
 * @param index Posición del fragmento dentro del body.
 * @param total Largo total del body.
 */
static void handleIniciar(AsyncWebServerRequest * r, uint8_t * data, size_t len, size_t index, size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionPanel(r, "profesor")) {
        body = "";
        return;
    }

    if (!r->hasParam("id")) {
        body = "";
        enviarError(r, 400, "Falta ID");
        return;
    }
    int idCuestionario = r->getParam("id")->value().toInt();

    DynamicJsonDocument doc(body.length() + 256);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(r, 400, err.c_str());
        return;
    }

    String fechaInicio = doc["fechaISO"] | "";
    if (fechaInicio == "") {
        enviarError(r, 400, "Falta 'fecha'.");
        return;
    }

    CuestionarioResult result = CuestionarioService::getInstance().iniciar(
        idCuestionario, SessionManager::getInstance().getSesionPanel().idUsuario, fechaInicio);
    if (!result.ok) {
        enviarError(r, 400, result.mensaje);
        return;
    }
    SessionManager::getInstance().limpiarPausaDesconexion();
    enviarOk(r, "Iniciado.");
}

/**
 * @brief Pausa el cuestionario del parámetro "id".
 * @details Olvida una pausa por desconexión anterior. No usa body (si viene,
 * se ignora).
 * @param r Request HTTP entrante.
 */
static void handlePausar(AsyncWebServerRequest * r) {
    if (!verificarSesionPanel(r, "profesor"))
        return;
    if (!r->hasParam("id")) {
        enviarError(r, 400, "Falta ID");
        return;
    }
    CuestionarioResult result = CuestionarioService::getInstance().pausar(
        r->getParam("id")->value().toInt(), SessionManager::getInstance().getSesionPanel().idUsuario);
    if (!result.ok) {
        enviarError(r, 400, result.mensaje);
        return;
    }
    SessionManager::getInstance().limpiarPausaDesconexion();
    enviarOk(r, "Pausado.");
}

/**
 * @brief Reanuda el cuestionario del parámetro "id".
 * @details Olvida una pausa por desconexión anterior. No usa body (si viene,
 * se ignora).
 * @param r Request HTTP entrante.
 */
static void handleReanudar(AsyncWebServerRequest * r) {
    if (!verificarSesionPanel(r, "profesor"))
        return;
    if (!r->hasParam("id")) {
        enviarError(r, 400, "Falta ID");
        return;
    }
    CuestionarioResult result = CuestionarioService::getInstance().reanudar(
        r->getParam("id")->value().toInt(), SessionManager::getInstance().getSesionPanel().idUsuario);
    if (!result.ok) {
        enviarError(r, 400, result.mensaje);
        return;
    }
    SessionManager::getInstance().limpiarPausaDesconexion();
    enviarOk(r, "Reanudado.");
}

/**
 * @brief Finaliza el cuestionario del parámetro "id".
 * @details Olvida una pausa por desconexión anterior. No usa body (si viene,
 * se ignora).
 * @param r Request HTTP entrante.
 */
static void handleFinalizar(AsyncWebServerRequest * r) {
    if (!verificarSesionPanel(r, "profesor"))
        return;
    if (!r->hasParam("id")) {
        enviarError(r, 400, "Falta ID");
        return;
    }
    CuestionarioResult result = CuestionarioService::getInstance().finalizar(
        r->getParam("id")->value().toInt(), SessionManager::getInstance().getSesionPanel().idUsuario);

    if (!result.ok) {
        enviarError(r, 400, result.mensaje);
        return;
    }
    SessionManager::getInstance().limpiarPausaDesconexion();
    enviarOk(r, "Finalizado.");
}

/**
 * @brief Devuelve el tiempo y, por cada pregunta, la opción elegida, la
 * correcta y si acertó, del cuestionario del parámetro "id".
 * @note El tutor puede ver cualquier cuestionario y el profesor solo los
 *       suyos. El invitado no tiene acceso (403).
 * @param r Request HTTP entrante.
 */
static void handleRevision(AsyncWebServerRequest * r) {
    if (!verificarSesionPanel(r))
        return;
    if (!r->hasParam("id")) {
        enviarError(r, 400, "Falta ID");
        return;
    }

    int idCuestionario = r->getParam("id")->value().toInt();

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);
    if (c.idCuestionario == 0) {
        enviarError(r, 404, "No encontrado");
        return;
    }

    const SesionPanel sesion = SessionManager::getInstance().getSesionPanel();
    bool permitido = (sesion.rol == "tutor") || (sesion.rol == "profesor" && c.idUsuario == sesion.idUsuario);
    if (!permitido) {
        enviarError(r, 403, "No tenés permisos para ver este cuestionario.", "SIN_PERMISO");
        return;
    }

    PreguntaRevision buffer[MAX_PREGUNTAS_POR_CUESTIONARIO];
    int cant =
        CuestionarioService::getInstance().obtenerRevision(idCuestionario, buffer, MAX_PREGUNTAS_POR_CUESTIONARIO);
    if (cant < 0) {
        enviarError(r, 404, "No encontrado");
        return;
    }

    String json = "{\"ok\":true,\"tiempoSegundos\":" + String(c.tiempoSegundos) + ",\"preguntas\":[";

    for (int i = 0; i < cant; i++) {
        if (i > 0)
            json += ",";
        json += "{\"idPregunta\":" + String(buffer[i].idPregunta) + ",\"pregunta\":\"" +
                escapeJson(buffer[i].textoPregunta) + "\",\"puntajeCorrecta\":" + String(buffer[i].puntajeCorrecta) +
                ",\"puntajeIncorrecta\":" + String(buffer[i].puntajeIncorrecta) + ",\"opcionElegida\":\"" +
                escapeJson(buffer[i].opcionElegida) + "\",\"opcionCorrecta\":\"" +
                escapeJson(buffer[i].opcionCorrecta) +
                "\",\"fueCorrecto\":" + String(buffer[i].fueCorrecto ? "true" : "false") + "}";
    }
    json += "]}";
    enviarJSON(r, 200, json);
}

void registrarCuestionarioController(AsyncWebServer & server) {
    server.on("/api/cuestionario/revision", HTTP_GET, handleRevision);
    server.on(
        "/api/cuestionario/iniciar", HTTP_PATCH,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleIniciar);
    server.on("/api/cuestionario/pausar", HTTP_PATCH, handlePausar);
    server.on("/api/cuestionario/reanudar", HTTP_PATCH, handleReanudar);
    server.on("/api/cuestionario/finalizar", HTTP_PATCH, handleFinalizar);

    server.on("/api/cuestionarios", HTTP_GET, handleListar);
    server.on(
        "/api/cuestionarios", HTTP_POST,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleCrear);
    server.on("/api/cuestionario", HTTP_GET, handleObtener);
    server.on(
        "/api/cuestionario", HTTP_PUT,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleEditar);
    server.on("/api/cuestionario", HTTP_DELETE, handleEliminar);
}