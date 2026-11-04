#ifndef USUARIO_SERVICE_H
#define USUARIO_SERVICE_H

#include <Arduino.h>
#include "../storage/models/Models.h"
#include "services/AuthService.h"
#include "config/ConfigManager.h"
#include "storage/database/repositories/UsuarioRepository.h"
#include "esp_task_wdt.h"

/**
 * @file UsuarioService.h
 * @brief Lógica de negocio para la gestión de usuarios.
 *
 * Responsabilidades:
 *   - Validar datos antes de crear o editar un usuario.
 *   - Verificar la clave maestra correspondiente al rol.
 *   - Hashear contraseñas antes de pasarlas al repository.
 *   - Coordinar UsuarioRepository y AuthService.
 */

/**
 * @brief Resultado estándar de operaciones de usuario.
 */
struct UsuarioResult {
    bool ok = false;
    String mensaje = "";
    int id = 0; ///< Id del usuario creado o editado. 0 en las demás operaciones.
};

/**
 * @brief Singleton con la lógica de alta, edición, contraseñas y baja de usuarios.
 */
class UsuarioService {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al UsuarioService.
     */
    static UsuarioService & getInstance() {
        static UsuarioService instance;
        return instance;
    }

    /**
     * @brief Crea un usuario nuevo previa verificación de la clave maestra.
     *
     * Valida, en este orden:
     *   - Que la clave maestra corresponda al rol a crear (un rol que no sea
     *     'profesor' ni 'tutor' se rechaza acá).
     *   - Que el usuario no tenga espacios y tenga entre 4 y 30 caracteres.
     *   - Que nombre, apellido, contacto y referencia (materia del profesor
     *     o vínculo del tutor) no estén vacíos.
     *   - Que la contraseña no esté vacía y coincida con la confirmación.
     *   - Que el nombre de usuario no exista ya.
     *
     * @param u            Usuario a crear (sin hashPassword ni salt, se calculan acá
     *                     y quedan cargados en u).
     * @param password     Contraseña en texto plano.
     * @param confirmar    Confirmación de la contraseña.
     * @param claveMaestra Clave maestra del rol correspondiente.
     * @return UsuarioResult con ok=true e id generado si tuvo éxito.
     */
    UsuarioResult crear(Usuario & u, const String & password, const String & confirmar, const String & claveMaestra);

    /**
     * @brief Edita el perfil del usuario logueado.
     *
     * Valida:
     *   - Que el usuario no tenga espacios y tenga entre 4 y 30 caracteres.
     *   - Que nombre, apellido, contacto y referencia no estén vacíos.
     *   - Que ningún otro usuario tenga ese nombre de usuario.
     *
     * @param u Usuario con los datos nuevos (idUsuario debe ser válido).
     * @return UsuarioResult con ok=true si tuvo éxito.
     */
    UsuarioResult editarPerfil(const Usuario & u);

    /**
     * @brief Recupera la contraseña de un usuario sin estar logueado.
     *
     * Valida:
     *   - Que la nueva contraseña no esté vacía y coincida con su confirmación.
     *   - Que el usuario exista.
     *   - Que la clave maestra sea la del rol de ese usuario.
     *
     * @param usuario         Nombre de login.
     * @param claveMaestra    Clave maestra del rol del usuario (profesor o tutor).
     * @param nueva           Nueva contraseña en texto plano.
     * @param confirmar       Confirmación de la nueva contraseña.
     * @return UsuarioResult con ok=true si tuvo éxito.
     */
    UsuarioResult recuperarPassword(String usuario, String claveMaestra, String nueva, String confirmar);

    /**
     * @brief Cambia la contraseña del usuario logueado.
     *
     * Valida:
     *   - Que la nueva contraseña no esté vacía y coincida con su confirmación.
     *   - Que el usuario exista.
     *   - Que la contraseña actual sea correcta.
     *
     * @param idUsuario       ID del usuario
     * @param actual          Contraseña actual en texto plano
     * @param nueva           Nueva contraseña en texto plano.
     * @param confirmar       Confirmación de la nueva contraseña.
     * @return UsuarioResult con ok=true si tuvo éxito.
     */
    UsuarioResult cambiarPassword(int idUsuario, const String & actual, const String & nueva, const String & confirmar);

    /**
     * @brief Elimina un usuario por número de ID.
     *
     * Valida:
     *   - Que el usuario exista.
     *   - Permisos: un tutor puede borrar profesores y su propia cuenta, un
     *     profesor solo su propia cuenta y el invitado no puede borrar a nadie.
     *
     * Por ON DELETE CASCADE se borran también sus cuestionarios con sus
     * preguntas y opciones.
     *
     * @param idUsuario      Numero ID del usuario a eliminar
     * @param idSolicitante  ID del usuario de la sesión que pide el borrado
     * @param rolSolicitante Rol de la sesión que pide el borrado
     * @return UsuarioResult con ok=true si tuvo éxito.
     */
    UsuarioResult eliminar(int idUsuario, int idSolicitante, const String & rolSolicitante);

    /**
     * @brief Obtiene el perfil completo de un usuario por id.
     * @param idUsuario Id del usuario.
     * @return Usuario encontrado, o struct con idUsuario=0 si no existe.
     */
    Usuario obtenerPerfil(int idUsuario);

    /**
     * @brief Lista una página de los usuarios de un rol en formato resumido.
     * @param rol     "profesor" o "tutor".
     * @param buffer  Array destino.
     * @param maxSize Capacidad del array (= tamaño de página).
     * @param offset  Cantidad de usuarios a saltear (páginas anteriores).
     * @return Cantidad de usuarios cargados en el buffer.
     */
    int listarPorRol(const String & rol, UsuarioResumen * buffer, int maxSize, int offset);

    /**
     * @brief Cuenta los usuarios de un rol (para la paginación).
     * @param rol "profesor" o "tutor".
     * @return Cantidad de usuarios con ese rol.
     */
    int contarPorRol(const String & rol);

private:
    UsuarioService() {
    }
    UsuarioService(const UsuarioService &) = delete;
    UsuarioService & operator=(const UsuarioService &) = delete;

    /**
     * @brief Valida los campos básicos de un usuario: login de 4 a 30
     * caracteres sin espacios, y nombre, apellido, contacto y referencia
     * no vacíos.
     * @param u Usuario a validar.
     * @return Mensaje de error, o cadena vacía si todo está bien.
     */
    String _validarCampos(const Usuario & u);

    /**
     * @brief Valida una contraseña nueva: que no esté vacía y que coincida
     * con la confirmación. La usan crear, recuperarPassword y cambiarPassword.
     * @param password  Contraseña nueva.
     * @param confirmar Confirmación de la contraseña.
     * @return Mensaje de error, o cadena vacía si todo está bien.
     */
    String _validarPassword(const String & password, const String & confirmar);

    /**
     * @brief Genera salt y hash para la contraseña nueva y la guarda en BD.
     * @note Asume que la contraseña ya fue validada con _validarPassword.
     * @param idUsuario Usuario al que se le cambia la contraseña.
     * @param nueva     Contraseña nueva en texto plano.
     * @return Resultado con ok/mensaje listo para devolver.
     */
    UsuarioResult _guardarPassword(int idUsuario, const String & nueva);
};

#endif // USUARIO_SERVICE_H
