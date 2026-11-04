#include "services/UsuarioService.h"

// ---------------------------------------------------------------------------
// Validación de campos
// ---------------------------------------------------------------------------

String UsuarioService::_validarCampos(const Usuario & u) {
    if (u.usuario.length() < 4 || u.usuario.length() > 30)
        return "El nombre de usuario debe tener entre 4 y 30 caracteres.";

    if (u.usuario.indexOf(' ') >= 0)
        return "El nombre de usuario no puede contener espacios.";

    if (u.nombre.length() == 0)
        return "El nombre es obligatorio.";

    if (u.apellido.length() == 0)
        return "El apellido es obligatorio.";

    if (u.contacto.length() == 0)
        return "El contacto es obligatorio.";

    if (u.referencia.length() == 0) {
        if (u.rol == "profesor")
            return "La materia es obligatoria para profesores.";
        else
            return "El vinculo es obligatorio para los tutores.";
    }

    return "";
}

String UsuarioService::_validarPassword(const String & password, const String & confirmar) {
    if (password.length() == 0)
        return "La contraseña es obligatoria.";

    if (password != confirmar)
        return "Las contraseñas no coinciden.";

    return "";
}

UsuarioResult UsuarioService::_guardarPassword(int idUsuario, const String & nueva) {
    UsuarioResult resultado;
    resultado.ok = false;

    String nuevoSalt = AuthService::getInstance().generarSalt();
    String nuevoHash = AuthService::getInstance().hashPassword(nueva, nuevoSalt);

    DbResult db = UsuarioRepository::getInstance().actualizarPassword(idUsuario, nuevoHash, nuevoSalt);

    if (db.ok) {
        resultado.ok = true;
        resultado.mensaje = "Contraseña cambiada con éxito.";
    } else {
        resultado.mensaje = "Error interno al guardar: " + db.mensaje;
    }

    return resultado;
}

// ---------------------------------------------------------------------------
// Crear usuario
// ---------------------------------------------------------------------------

UsuarioResult UsuarioService::crear(Usuario & u, const String & password, const String & confirmar,
                                    const String & claveMaestra) {
    UsuarioResult result;

    if (!ConfigManager::getInstance().verificarClavePorRol(claveMaestra, u.rol)) {
        result.mensaje = "Clave maestra incorrecta.";
        return result;
    }

    String error = _validarCampos(u);
    if (error.length() > 0) {
        result.mensaje = error;
        return result;
    }

    error = _validarPassword(password, confirmar);
    if (error.length() > 0) {
        result.mensaje = error;
        return result;
    }

    if (UsuarioRepository::getInstance().existeUsuario(u.usuario)) {
        result.mensaje = "El nombre de usuario ya está en uso.";
        return result;
    }

    u.salt = AuthService::getInstance().generarSalt();
    u.hashPassword = AuthService::getInstance().hashPassword(password, u.salt);

    DbResult db = UsuarioRepository::getInstance().crear(u);
    if (!db.ok) {
        result.mensaje = "Error al crear el usuario: " + db.mensaje;
        return result;
    }

    result.ok = true;
    result.id = db.id;
    return result;
}

// ---------------------------------------------------------------------------
// Editar perfil
// ---------------------------------------------------------------------------

UsuarioResult UsuarioService::editarPerfil(const Usuario & u) {
    UsuarioResult result;

    String error = _validarCampos(u);
    if (error.length() > 0) {
        result.mensaje = error;
        return result;
    }

    if (UsuarioRepository::getInstance().existeUsuarioExcluyendo(u.usuario, u.idUsuario)) {
        result.mensaje = "El nombre de usuario ya está en uso.";
        return result;
    }

    DbResult db = UsuarioRepository::getInstance().actualizar(u);
    if (!db.ok) {
        result.mensaje = "Error al actualizar el perfil: " + db.mensaje;
        return result;
    }

    result.ok = true;
    result.id = u.idUsuario;
    return result;
}

// ---------------------------------------------------------------------------
// Recuperar contraseña
// ---------------------------------------------------------------------------

UsuarioResult UsuarioService::recuperarPassword(String usuario, String claveMaestra, String nueva, String confirmar) {
    UsuarioResult resultado;
    resultado.ok = false;

    String error = _validarPassword(nueva, confirmar);
    if (error.length() > 0) {
        resultado.mensaje = error;
        return resultado;
    }

    Usuario u = UsuarioRepository::getInstance().buscarPorUsuario(usuario);
    if (u.idUsuario == 0) {
        resultado.mensaje = "El usuario no existe.";
        return resultado;
    }

    if (!ConfigManager::getInstance().verificarClavePorRol(claveMaestra, u.rol)) {
        resultado.mensaje = "Clave maestra incorrecta.";
        return resultado;
    }

    return _guardarPassword(u.idUsuario, nueva);
}

// ---------------------------------------------------------------------------
// Cambiar contraseña
// ---------------------------------------------------------------------------
UsuarioResult UsuarioService::cambiarPassword(int idUsuario, const String & actual, const String & nueva,
                                              const String & confirmar) {
    UsuarioResult resultado;
    resultado.ok = false;

    String error = _validarPassword(nueva, confirmar);
    if (error.length() > 0) {
        resultado.mensaje = error;
        return resultado;
    }

    Usuario u = UsuarioRepository::getInstance().buscarPorId(idUsuario);
    if (u.idUsuario == 0) {
        resultado.mensaje = "Usuario no encontrado.";
        return resultado;
    }

    String hashActual = AuthService::getInstance().hashPassword(actual, u.salt);
    if (hashActual != u.hashPassword) {
        resultado.mensaje = "La contraseña actual es incorrecta.";
        return resultado;
    }

    return _guardarPassword(idUsuario, nueva);
}

// ---------------------------------------------------------------------------
// Eliminar usuario
// ---------------------------------------------------------------------------
UsuarioResult UsuarioService::eliminar(int idUsuario, int idSolicitante, const String & rolSolicitante) {
    UsuarioResult result;

    Usuario u = UsuarioRepository::getInstance().buscarPorId(idUsuario);
    if (u.idUsuario == 0) {
        result.ok = false;
        result.mensaje = "Usuario no encontrado en el sistema.";
        return result;
    }

    bool esPropiaCuenta = (u.idUsuario == idSolicitante);

    bool permitido = (rolSolicitante == "tutor" && (u.rol == "profesor" || esPropiaCuenta)) ||
                     (rolSolicitante == "profesor" && esPropiaCuenta);

    if (!permitido) {
        result.ok = false;
        result.mensaje = "No tenés permisos para eliminar este usuario.";
        return result;
    }

    esp_task_wdt_reset();
    DbResult db = UsuarioRepository::getInstance().eliminar(idUsuario);
    esp_task_wdt_reset();
    yield();

    if (!db.ok) {
        result.ok = false;
        result.mensaje = "Error al eliminar el usuario: " + db.mensaje;
        return result;
    }

    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Obtener perfil
// ---------------------------------------------------------------------------

Usuario UsuarioService::obtenerPerfil(int idUsuario) {
    return UsuarioRepository::getInstance().buscarPorId(idUsuario);
}

// ---------------------------------------------------------------------------
// Listados
// ---------------------------------------------------------------------------

int UsuarioService::listarPorRol(const String & rol, UsuarioResumen * buffer, int maxSize, int offset) {
    return UsuarioRepository::getInstance().listarPorRol(rol, buffer, maxSize, offset);
}

int UsuarioService::contarPorRol(const String & rol) {
    return UsuarioRepository::getInstance().contarPorRol(rol);
}
