#include "controllers/AuthController.h"

// ---------------------------------------------------------------------------
// POST /api/auth/login
// ---------------------------------------------------------------------------
/**
 * @brief Login con usuario y contraseña. Body: {"usuario", "password"}.
 *
 * Responde 200 con token, rol, idUsuario y nombre, o 401 con el motivo
 * (y codigo "PANEL_OCUPADO" si la sesión fue rechazada por prioridad).
 * @param request Request HTTP entrante.
 * @param data    Fragmento del body.
 * @param len     Largo del fragmento.
 * @param index   Posición del fragmento dentro del body.
 * @param total   Largo total del body.
 */
static void handleLogin(AsyncWebServerRequest * request, uint8_t * data, size_t len, size_t index, size_t total) {
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
    String password = doc["password"] | "";

    AuthResult result = AuthService::getInstance().login(usuario, password);

    if (!result.ok) {
        enviarError(request, 401, result.mensaje, result.codigo);
        return;
    }

    SessionManager::getInstance().asignarPestanaPanel(result.token, extraerPestana(request));

    StaticJsonDocument<256> resp;
    resp["ok"] = true;
    resp["token"] = result.token;
    resp["rol"] = result.rol;
    resp["idUsuario"] = result.idUsuario;
    resp["nombre"] = result.nombre;

    String json;
    serializeJson(resp, json);
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// POST /api/auth/invitado
// ---------------------------------------------------------------------------
/**
 * @brief Login como invitado. Responde 200 con token y rol, o 409 con
 * codigo "CUESTIONARIO_EN_CURSO" o "PANEL_OCUPADO".
 * @param request Request HTTP entrante.
 */
static void handleLoginInvitado(AsyncWebServerRequest * request) {
    AuthResult result = AuthService::getInstance().loginInvitado();

    if (!result.ok) {
        enviarError(request, 409, result.mensaje, result.codigo);
        return;
    }

    SessionManager::getInstance().asignarPestanaPanel(result.token, extraerPestana(request));

    StaticJsonDocument<128> resp;
    resp["ok"] = true;
    resp["token"] = result.token;
    resp["rol"] = result.rol;

    String json;
    serializeJson(resp, json);
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// GET /api/auth/perfil
// ---------------------------------------------------------------------------
/**
 * @brief Devuelve los datos del usuario logueado. Para el invitado solo
 * devuelve el rol.
 * @param request Request HTTP entrante.
 */
static void handlePerfil(AsyncWebServerRequest * request) {
    if (!verificarSesionPanel(request))
        return;

    SesionPanel sesion = SessionManager::getInstance().getSesionPanel();

    if (sesion.rol == "invitado") {
        StaticJsonDocument<128> resp;
        resp["ok"] = true;
        resp["rol"] = "invitado";
        String json;
        serializeJson(resp, json);
        enviarJSON(request, 200, json);
        return;
    }

    Usuario u = UsuarioService::getInstance().obtenerPerfil(sesion.idUsuario);
    if (u.idUsuario == 0) {
        enviarError(request, 404, "Usuario no encontrado.");
        return;
    }

    StaticJsonDocument<512> resp;
    resp["ok"] = true;
    resp["idUsuario"] = u.idUsuario;
    resp["usuario"] = u.usuario;
    resp["nombre"] = u.nombre;
    resp["apellido"] = u.apellido;
    resp["rol"] = u.rol;
    resp["referencia"] = u.referencia;
    resp["contacto"] = u.contacto;

    String json;
    serializeJson(resp, json);
    enviarJSON(request, 200, json);
}

// ---------------------------------------------------------------------------
// Registro de rutas
// ---------------------------------------------------------------------------
void registrarAuthController(AsyncWebServer & server) {
    server.on(
        "/api/auth/login", HTTP_POST,
        [](AsyncWebServerRequest * r) {
        },
        nullptr, handleLogin);

    server.on("/api/auth/invitado", HTTP_POST, handleLoginInvitado);

    server.on("/api/auth/logout", HTTP_POST, [](AsyncWebServerRequest * r) {
        if (!verificarSesionPanel(r))
            return;
        AuthService::getInstance().logout();
        enviarOk(r, "Sesión cerrada.");
    });

    server.on("/api/auth/heartbeat", HTTP_POST, [](AsyncWebServerRequest * r) {
        bool interaccion = r->hasParam("interaccion") && r->getParam("interaccion")->value() == "1";
        if (!verificarSesionPanel(r, "", interaccion))
            return;
        enviarOk(r, "ok");
    });

    server.on("/api/auth/perfil", HTTP_GET, handlePerfil);
}
