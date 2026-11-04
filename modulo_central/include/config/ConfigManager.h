#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @file ConfigManager.h
 * @brief Gestión de la configuración inicial del sistema via NVS.
 *
 * Almacena en la memoria NVS (Non-Volatile Storage) interna de la ESP32,
 * inaccesible extrayendo la tarjeta SD:
 *   - Clave de profesor: se pide para crear usuarios con rol 'profesor' y
 *     para recuperar su contraseña.
 *   - Clave de tutor: se pide para crear usuarios con rol 'tutor' y para
 *     recuperar su contraseña.
 *   - Nombre del alumno: se muestra en la pantalla de estado.
 *
 * Los tres datos se cargan en el primer arranque desde el teclado en
 * pantalla (screen_setup); mientras falte alguno, se vuelve a mostrar
 * esa pantalla.
 */
class ConfigManager {
public:
    /**
     * @brief Devuelve la instancia única (Singleton).
     * @return Referencia a la instancia de ConfigManager.
     */
    static ConfigManager & getInstance() {
        static ConfigManager instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    // Estado de configuración
    // -----------------------------------------------------------------------

    /**
     * @brief Indica si la configuración inicial ya se completó.
     * @return true si la clave de profesor, la de tutor y el nombre del
     *         alumno existen en NVS.
     */
    bool clavesConfiguradas();

    /**
     * @brief Indica si la clave de profesor fue configurada.
     * @return true si existe en NVS.
     */
    bool claveProfesorConfigurada();

    /**
     * @brief Indica si la clave de tutor fue configurada.
     * @return true si existe en NVS.
     */
    bool claveTutorConfigurada();

    // -----------------------------------------------------------------------
    // Guardar claves (primer arranque)
    // -----------------------------------------------------------------------

    /**
     * @brief Guarda la clave maestra para profesores en NVS.
     * @param clave Clave a guardar. No puede ser vacía.
     * @return true si se guardó correctamente.
     */
    bool guardarClaveProfesor(const String & clave);

    /**
     * @brief Guarda la clave maestra para tutores en NVS.
     * @param clave Clave a guardar. No puede ser vacía.
     * @return true si se guardó correctamente.
     */
    bool guardarClaveTutor(const String & clave);

    // -----------------------------------------------------------------------
    // Verificar claves (en cada operación)
    // -----------------------------------------------------------------------

    /**
     * @brief Verifica si una clave coincide con la clave maestra de profesores.
     * @param clave Clave a verificar.
     * @return true si coincide.
     */
    bool verificarClaveProfesor(const String & clave);

    /**
     * @brief Verifica si una clave coincide con la clave maestra de tutores.
     * @param clave Clave a verificar.
     * @return true si coincide.
     */
    bool verificarClaveTutor(const String & clave);

    /**
     * @brief Verifica la clave maestra correspondiente al rol dado.
     * @param clave Clave a verificar.
     * @param rol   "profesor" o "tutor".
     * @return true si la clave coincide con la del rol indicado; false si
     *         no coincide o si el rol no es ninguno de los dos.
     */
    bool verificarClavePorRol(const String & clave, const String & rol);

    // -----------------------------------------------------------------------
    // Nombre del alumno
    // -----------------------------------------------------------------------

    /**
     * @brief Guarda el nombre del alumno en NVS.
     * @param nombre Nombre del alumno a guardar. No puede ser vacío.
     * @return true si se guardó correctamente.
     */
    bool guardarNombreAlumno(const String & nombre);

    /**
     * @brief Lee el nombre del alumno desde NVS.
     * @return Nombre del alumno, o cadena vacía si no existe.
     */
    String leerNombreAlumno();

private:
    ConfigManager() {
    }
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager & operator=(const ConfigManager &) = delete;

    /// @brief Namespace usado en NVS para aislar los datos de esta app.
    static constexpr const char * NVS_NAMESPACE = "levi_cfg";

    /// @brief Clave NVS para la clave maestra de profesores.
    static constexpr const char * KEY_CLAVE_PROFESOR = "clave_prof";

    /// @brief Clave NVS para la clave maestra de tutores.
    static constexpr const char * KEY_CLAVE_TUTOR = "clave_tutor";

    /// @brief Clave NVS para el nombre del alumno.
    static constexpr const char * KEY_NOMBRE_ALUMNO = "nombre_alumno";

    /**
     * @brief Lee un valor del NVS por su key interna.
     * @param key Key NVS a leer.
     * @return Valor almacenado, o cadena vacía si no existe.
     */
    String _leer(const char * key);

    /**
     * @brief Guarda un valor en NVS.
     * @param key   Key NVS destino.
     * @param valor Valor a guardar.
     * @return true si se guardó correctamente; false si el valor es vacío
     *         o si falló la escritura.
     */
    bool _guardar(const char * key, const String & valor);
};

#endif // CONFIG_MANAGER_H