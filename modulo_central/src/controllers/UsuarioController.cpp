#include "controllers/UsuarioController.h"

// ---------------------------------------------------------------------------
// POST /api/usuarios
// ---------------------------------------------------------------------------
/**
 * @brief Crea un usuario. No requiere sesión: se autoriza con la clave
 * maestra del rol. Body: {"usuario", "nombre", "apellido", "rol",
 * "referencia", "contacto", "password", "confirmar", "claveMaestra"}.
 *
 * Responde 201 con idUsuario, o 400 con el motivo.
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

    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, "JSON inválido.");
        return;
    }

    Usuario u;
    u.usuario = doc["usuario"] | "";
    u.nombre = doc["nombre"] | "";
    u.apellido = doc["apellido"] | "";
    u.rol = doc["rol"] | "";
    u.referencia = doc["referencia"] | "";
    u.contacto = doc["contacto"] | "";

    String password = doc["password"] | "";
    String confirmar = doc["confirmar"] | "";
    String claveMaestra = doc["claveMaestra"] | "";

    UsuarioResult result = UsuarioService::getInstance().crear(u, password, confirmar, claveMaestra);
    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }

    StaticJsonDocument<128> resp;
    resp["ok"] = true;
    resp["idUsuario"] = result.id;
    resp["mensaje"] = "Usuario creado correctamente.";

    String json;
    serializeJson(resp, json);
    enviarJSON(request, 201, json);
}

// ---------------------------------------------------------------------------
// DELETE /api/usuario?id=X
// ---------------------------------------------------------------------------
/**
 * @brief Elimina el usuario del parámetro "id", según los permisos de la
 * sesión (ver UsuarioService::eliminar()).
 * @param request Request HTTP entrante.
 */
static void handleEliminar(AsyncWebServerRequest * request) {
    if (!verificarSesionPanel(request))
        return;

    if (!request->hasParam("id")) {
        enviarError(request, 400, "Falta el ID del usuario.");
        return;
    }

    int idUsuario = request->getParam("id")->value().toInt();

    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();

    UsuarioResult result = UsuarioService::getInstance().eliminar(idUsuario, sesion.idUsuario, sesion.rol);

    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }

    enviarOk(request, "Usuario eliminado exitosamente.");
}

// ---------------------------------------------------------------------------
// PUT /api/usuarios/perfil
// ---------------------------------------------------------------------------
/**
 * @brief Edita el perfil del usuario logueado (403 si es invitado).
 *
 * Body: {"usuario", "nombre", "apellido", "referencia", "contacto"}; los
 * campos que no vienen conservan su valor actual. Si se guarda, actualiza
 * también el nombre de la sesión.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleEditarPerfil(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index,
                               size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionPanel(request)) {
        body = "";
        return;
    }

    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();
    if (sesion.rol == "invitado") {
        body = "";
        enviarError(request, 403, "El invitado no tiene perfil para editar.");
        return;
    }

    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, "JSON inválido.");
        return;
    }

    Usuario u = UsuarioService::getInstance().obtenerPerfil(sesion.idUsuario);
    if (u.idUsuario == 0) {
        enviarError(request, 404, "Usuario no encontrado.");
        return;
    }

    u.usuario = doc["usuario"] | u.usuario;
    u.nombre = doc["nombre"] | u.nombre;
    u.apellido = doc["apellido"] | u.apellido;
    u.referencia = doc["referencia"] | u.referencia;
    u.contacto = doc["contacto"] | u.contacto;

    UsuarioResult result = UsuarioService::getInstance().editarPerfil(u);
    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }

    String nombreCompleto = u.nombre + " " + u.apellido;
    SessionManager::getInstance().actualizarNombrePanel(nombreCompleto);

    enviarOk(request, "Perfil actualizado correctamente.");
}

// ---------------------------------------------------------------------------
// PUT /api/usuarios/password
// ---------------------------------------------------------------------------
/**
 * @brief Recupera la contraseña de un usuario sin sesión, con la clave
 * maestra de su rol. Body: {"usuario", "claveMaestra", "passwordNueva",
 * "confirmar"}.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleRecuperarPassword(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index,
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

    String usuario = doc["usuario"] | "";
    String claveMaestra = doc["claveMaestra"] | "";
    String nuevaPassword = doc["passwordNueva"] | "";
    String confirmar = doc["confirmar"] | "";

    UsuarioResult result =
        UsuarioService::getInstance().recuperarPassword(usuario, claveMaestra, nuevaPassword, confirmar);

    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }

    enviarOk(request, "Contraseña recuperada correctamente.");
}

// ---------------------------------------------------------------------------
// PUT /api/usuarios/perfil/password
// ---------------------------------------------------------------------------
/**
 * @brief Cambia la contraseña del usuario logueado. Body:
 * {"passwordActual", "passwordNueva", "confirmar"}.
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleCambiarPassword(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index,
                                  size_t total) {
    static String body;

    if (!acumularBody(body, data, len, index, total))
        return;

    if (!verificarSesionPanel(request)) {
        body = "";
        return;
    }

    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();

    DynamicJsonDocument doc(body.length() + 512);
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        enviarError(request, 400, "JSON inválido.");
        return;
    }

    String actual = doc["passwordActual"] | "";
    String nueva = doc["passwordNueva"] | "";
    String confirmar = doc["confirmar"] | "";

    UsuarioResult result = UsuarioService::getInstance().cambiarPassword(sesion.idUsuario, actual, nueva, confirmar);

    if (!result.ok) {
        enviarError(request, 400, result.mensaje);
        return;
    }

    enviarOk(request, "Contraseña actualizada correctamente.");
}

// ---------------------------------------------------------------------------
// Listados de usuarios por rol
// ---------------------------------------------------------------------------

/**
 * @brief Responde una página del listado de usuarios de un rol
 * (ELEMENTOS_POR_PAGINA por página, parámetro de query "pagina").
 *
 * Requiere sesión de profesor o tutor (403 si es invitado).
 * El JSON se arma a mano para no depender del tamaño de un JsonDocument.
 *
 * @param request          Request HTTP entrante.
 * @param rol              "profesor" o "tutor".
 * @param nombreArray      Clave del array en la respuesta ("profesores" / "tutores").
 * @param nombreReferencia Clave del campo referencia ("materia" / "vinculo").
 */
static void responderListadoPorRol(AsyncWebServerRequest * request, const String & rol, const String & nombreArray,
                                   const String & nombreReferencia) {
    if (!verificarSesionPanel(request))
        return;

    const SesionPanel & sesion = SessionManager::getInstance().getSesionPanel();
    if (sesion.rol == "invitado") {
        enviarError(request, 403, "No tenés permisos para esta acción.");
        return;
    }

    int pagina = obtenerPagina(request);
    int offset = (pagina - 1) * ELEMENTOS_POR_PAGINA;
    int total = UsuarioService::getInstance().contarPorRol(rol);

    UsuarioResumen buffer[ELEMENTOS_POR_PAGINA];
    int cant = UsuarioService::getInstance().listarPorRol(rol, buffer, ELEMENTOS_POR_PAGINA, offset);

    String json = "{\"ok\":true,\"" + nombreArray + "\":[";

    for (int i = 0; i < cant; i++) {
        if (i > 0)
            json += ",";
        json += "{\"idUsuario\":" + String(buffer[i].idUsuario) + ",";
        json += "\"nombre\":\"" + escapeJson(buffer[i].nombre) + "\",";
        json += "\"apellido\":\"" + escapeJson(buffer[i].apellido) + "\",";
        json += "\"" + nombreReferencia + "\":\"" + escapeJson(buffer[i].referencia) + "\",";
        json += "\"contacto\":\"" + escapeJson(buffer[i].contacto) + "\"}";
    }

    json += "]," + jsonPaginacion(pagina, total) + "}";

    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// GET /api/usuarios/profesores
// ---------------------------------------------------------------------------
/**
 * @brief Lista los profesores (con su materia), paginado.
 * @param request Request HTTP entrante.
 */
static void handleListarProfesores(AsyncWebServerRequest * request) {
    responderListadoPorRol(request, "profesor", "profesores", "materia");
}

// ---------------------------------------------------------------------------
// GET /api/usuarios/tutores
// ---------------------------------------------------------------------------
/**
 * @brief Lista los tutores (con su vínculo), paginado.
 * @param request Request HTTP entrante.
 */
static void handleListarTutores(AsyncWebServerRequest * request) {
    responderListadoPorRol(request, "tutor", "tutores", "vinculo");
}

// ---------------------------------------------------------------------------
// Registro de rutas
// ---------------------------------------------------------------------------
void registrarUsuarioController(AsyncWebServer & server) {
    server.on(
        "/api/usuarios/perfil/password", HTTP_PUT,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleCambiarPassword);
    server.on(
        "/api/usuarios/perfil", HTTP_PUT,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleEditarPerfil);

    server.on(
        "/api/usuarios/password", HTTP_PUT,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleRecuperarPassword);
    server.on(
        "/api/usuarios", HTTP_POST,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleCrear);
    server.on("/api/usuario", HTTP_DELETE, handleEliminar);

    server.on("/api/usuarios/profesores", HTTP_GET, handleListarProfesores);
    server.on("/api/usuarios/tutores", HTTP_GET, handleListarTutores);
}