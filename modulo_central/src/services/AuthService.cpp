#include "services/AuthService.h"

// ---------------------------------------------------------------------------
// Hash y salt
// ---------------------------------------------------------------------------

String AuthService::generarSalt() {
    String salt = "";
    for (int i = 0; i < 16; i++) {
        byte b = (byte)(esp_random() % 256);
        if (b < 16)
            salt += "0";
        salt += String(b, HEX);
    }
    return salt;
}

String AuthService::hashPassword(const String & password, const String & salt) {
    String input = password + salt;

    byte hash[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const byte *)input.c_str(), input.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    String resultado = "";
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16)
            resultado += "0";
        resultado += String(hash[i], HEX);
    }
    return resultado;
}

bool AuthService::verificarPassword(const String & password, const String & salt, const String & hashGuardado) {
    return hashPassword(password, salt) == hashGuardado;
}

// ---------------------------------------------------------------------------
// Login / Logout
// ---------------------------------------------------------------------------

AuthResult AuthService::login(const String & nombreUsuario, const String & password) {
    AuthResult result;

    if (nombreUsuario.length() == 0 || password.length() == 0) {
        result.mensaje = "Usuario y contraseña son obligatorios.";
        return result;
    }

    Usuario u = UsuarioRepository::getInstance().buscarPorUsuario(nombreUsuario);
    if (u.idUsuario == 0) {
        result.mensaje = "Usuario o contraseña incorrectos.";
        return result;
    }

    if (!verificarPassword(password, u.salt, u.hashPassword)) {
        result.mensaje = "Usuario o contraseña incorrectos.";
        return result;
    }

    SesionResult sesion =
        SessionManager::getInstance().iniciarSesionPanel(u.rol, u.idUsuario, u.nombre + " " + u.apellido);
    if (!sesion.ok) {
        result.mensaje = sesion.mensaje;
        result.codigo = sesion.codigo;
        return result;
    }

    result.ok = true;
    result.token = sesion.token;
    result.rol = u.rol;
    result.idUsuario = u.idUsuario;
    result.nombre = u.nombre + " " + u.apellido;
    return result;
}

AuthResult AuthService::loginInvitado() {
    AuthResult result;

    if (CuestionarioRepository::getInstance().hayUnoEnProgreso()) {
        result.mensaje = "Hay un cuestionario del profesor en curso. Probá más tarde.";
        result.codigo = "CUESTIONARIO_EN_CURSO";
        return result;
    }

    SesionResult sesion = SessionManager::getInstance().iniciarSesionPanel("invitado", 0, "Invitado");
    if (!sesion.ok) {
        result.mensaje = sesion.mensaje;
        result.codigo = sesion.codigo;
        return result;
    }

    result.ok = true;
    result.token = sesion.token;
    result.rol = "invitado";
    return result;
}

void AuthService::logout() {
    SessionManager::getInstance().cerrarSesionPanel("logout");
}
