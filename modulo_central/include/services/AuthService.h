#ifndef AUTH_SERVICE_H
#define AUTH_SERVICE_H

#include <Arduino.h>
#include "../storage/models/Models.h"
#include "../session/SessionManager.h"
#include "storage/database/repositories/UsuarioRepository.h"
#include "mbedtls/md.h"
#include <storage/database/repositories/CuestionarioRepository.h>

/**
 * @file AuthService.h
 * @brief Lógica de autenticación, hashing de contraseñas y sesiones del panel.
 *
 * Responsabilidades:
 *   - Generar salt y hashear contraseñas con SHA256.
 *   - Verificar contraseñas al hacer login.
 *   - Abrir y cerrar la sesión del panel via SessionManager.
 *   - Proveer utilidades de hash para UsuarioService.
 */

/**
 * @brief Resultado de una operación de autenticación.
 */
struct AuthResult {
    bool ok = false;
    String token = "";
    String rol = "";    ///< "profesor" | "tutor" | "invitado"
    int idUsuario = 0;  ///< 0 si es invitado.
    String nombre = ""; ///< Nombre y apellido. Vacío si es invitado.
    String mensaje = "";
    String codigo = ""; ///< Código de error para el front ("PANEL_OCUPADO", "CUESTIONARIO_EN_CURSO")
};

/**
 * @brief Singleton con la lógica de login, logout y hash de contraseñas.
 */
class AuthService {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al AuthService.
     */
    static AuthService & getInstance() {
        static AuthService instance;
        return instance;
    }

    /**
     * @brief Intenta hacer login con usuario y contraseña.
     *
     * Busca el usuario en BD, verifica el hash de la contraseña
     * y crea una sesión panel si todo es correcto. La sesión puede ser
     * rechazada por la prioridad de roles (ver
     * SessionManager::iniciarSesionPanel()).
     *
     * @param nombreUsuario Nombre de login.
     * @param password      Contraseña en texto plano.
     * @return AuthResult con ok=true, token y datos del usuario si tuvo éxito.
     */
    AuthResult login(const String & nombreUsuario, const String & password);

    /**
     * @brief Inicia sesión como invitado.
     *
     * Se rechaza con codigo "CUESTIONARIO_EN_CURSO" si hay un cuestionario
     * en progreso, o con "PANEL_OCUPADO" si ya hay otra sesión del panel.
     *
     * @return AuthResult con ok=true y token si tuvo éxito.
     */
    AuthResult loginInvitado();

    /**
     * @brief Cierra la sesión panel activa con motivo "logout".
     */
    void logout();

    /**
     * @brief Genera un salt aleatorio de 32 caracteres hexadecimales.
     * @return Salt generado con esp_random().
     */
    String generarSalt();

    /**
     * @brief Aplica SHA256 a (password + salt) y devuelve el hash en hex.
     * @param password Contraseña en texto plano.
     * @param salt     Salt a concatenar.
     * @return Hash SHA256 de 64 caracteres hexadecimales.
     */
    String hashPassword(const String & password, const String & salt);

    /**
     * @brief Verifica si una contraseña coincide con un hash almacenado.
     * @param password     Contraseña en texto plano a verificar.
     * @param salt         Salt almacenado del usuario.
     * @param hashGuardado Hash almacenado del usuario.
     * @return true si la contraseña es correcta.
     */
    bool verificarPassword(const String & password, const String & salt, const String & hashGuardado);

private:
    AuthService() {
    }
    AuthService(const AuthService &) = delete;
    AuthService & operator=(const AuthService &) = delete;
};

#endif // AUTH_SERVICE_H
