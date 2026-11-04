#include "middleware/Middleware.h"

String extraerToken(AsyncWebServerRequest * request) {
    if (!request->hasHeader("Authorization"))
        return "";

    String auth = request->getHeader("Authorization")->value();

    if (!auth.startsWith("Bearer "))
        return "";

    return auth.substring(7);
}

String extraerPestana(AsyncWebServerRequest * request) {
    if (!request->hasHeader("X-Pestana"))
        return "";
    return request->getHeader("X-Pestana")->value();
}

/**
 * @brief Indica si el request pide que su pestaña tome el control de la sesión.
 * @param request Request HTTP entrante.
 * @return true si trae el header "X-Tomar-Pestana: 1".
 */
static bool pideTomarPestana(AsyncWebServerRequest * request) {
    return request->hasHeader("X-Tomar-Pestana") && request->getHeader("X-Tomar-Pestana")->value() == "1";
}

/**
 * @brief Responde 409 con codigo "OTRA_PESTANA".
 * @param request Request HTTP entrante.
 */
static void enviarOtraPestana(AsyncWebServerRequest * request) {
    enviarError(request, 409, "La sesión está abierta en otra pestaña.", "OTRA_PESTANA");
}

/**
 * @brief Responde 401 con codigo "SESION_CERRADA" y el motivo del cierre.
 * @param request Request HTTP entrante.
 * @param mensaje Mensaje legible.
 * @param motivo  Motivo del cierre ("" si no se conoce).
 */
static void enviarSesionCerrada(AsyncWebServerRequest * request, const String & mensaje, const String & motivo) {
    String json = "{\"ok\":false,\"codigo\":\"SESION_CERRADA\",\"motivo\":\"" + escapeJson(motivo) +
                  "\",\"mensaje\":\"" + escapeJson(mensaje) + "\"}";
    enviarJSON(request, 401, json);
}

bool verificarSesionPanel(AsyncWebServerRequest * request, const String & rolRequerido, bool interaccion) {
    SessionManager & sesiones = SessionManager::getInstance();
    sesiones.procesarPendientes();

    String token = extraerToken(request);

    if (!sesiones.verificarTokenPanel(token)) {
        enviarSesionCerrada(request, "Sesión inválida o expirada.", sesiones.motivoCierrePanel(token));
        return false;
    }

    String pestana = extraerPestana(request);
    if (pideTomarPestana(request)) {
        sesiones.asignarPestanaPanel(token, pestana);
    } else if (!sesiones.esPestanaPanel(token, pestana)) {
        enviarOtraPestana(request);
        return false;
    }

    if (rolRequerido.length() > 0) {
        if (sesiones.getSesionPanel().rol != rolRequerido) {
            enviarError(request, 403, "No tenés permisos para esta acción.", "SIN_PERMISO");
            return false;
        }
    }

    sesiones.heartbeatPanel(token, interaccion);
    return true;
}

bool verificarSesionAlumno(AsyncWebServerRequest * request) {
    SessionManager & sesiones = SessionManager::getInstance();
    sesiones.procesarPendientes();

    String token = extraerToken(request);

    if (!sesiones.verificarTokenAlumno(token)) {
        enviarSesionCerrada(request, "Sesión de alumno inválida o expirada.", "");
        return false;
    }

    String pestana = extraerPestana(request);
    if (pideTomarPestana(request)) {
        sesiones.asignarPestanaAlumno(token, pestana);
    } else if (!sesiones.esPestanaAlumno(token, pestana)) {
        enviarOtraPestana(request);
        return false;
    }

    if (!sesiones.heartbeatAlumno(token)) {
        enviarSesionCerrada(request, "Sesión de alumno inválida o expirada.", "");
        return false;
    }

    return true;
}

void enviarJSON(AsyncWebServerRequest * request, int codigo, const String & json) {
    AsyncWebServerResponse * response = request->beginResponse(codigo, "application/json", json);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void enviarError(AsyncWebServerRequest * request, int codigo, const String & mensaje, const String & codigoError) {
    String json = "{\"ok\":false,";
    if (codigoError.length() > 0) {
        json += "\"codigo\":\"" + escapeJson(codigoError) + "\",";
    }
    json += "\"mensaje\":\"" + escapeJson(mensaje) + "\"}";
    enviarJSON(request, codigo, json);
}

bool acumularBody(String & body, uint8_t * data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        body = "";
        body.reserve(total);
    }

    body.concat((const char *)data, len);

    return index + len >= total;
}

void enviarOk(AsyncWebServerRequest * request, const String & mensaje) {
    String json = "{\"ok\":true,\"mensaje\":\"" + escapeJson(mensaje) + "\"}";
    enviarJSON(request, 200, json);
}

int obtenerPagina(AsyncWebServerRequest * request) {
    if (!request->hasParam("pagina"))
        return 1;

    int pagina = request->getParam("pagina")->value().toInt();
    return (pagina < 1) ? 1 : pagina;
}

String jsonPaginacion(int pagina, int total) {
    int totalPaginas = (total + ELEMENTOS_POR_PAGINA - 1) / ELEMENTOS_POR_PAGINA;
    if (totalPaginas < 1)
        totalPaginas = 1;

    return "\"pagina\":" + String(pagina) + ",\"porPagina\":" + String(ELEMENTOS_POR_PAGINA) +
           ",\"total\":" + String(total) + ",\"totalPaginas\":" + String(totalPaginas);
}

String escapeJson(String texto) {
    texto.replace("\\", "\\\\");
    texto.replace("\"", "\\\"");
    texto.replace("\n", "\\n");
    texto.replace("\r", "");
    return texto;
}
