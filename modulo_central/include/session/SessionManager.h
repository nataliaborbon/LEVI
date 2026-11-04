#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../storage/models/Models.h"
#include "config/Limites.h"
#include "storage/database/repositories/CuestionarioRepository.h"
#include "services/CuestionarioService.h"

/**
 * @file SessionManager.h
 * @brief Gestión de sesiones activas en RAM.
 *
 * Maneja dos slots independientes:
 *   - Sesión panel: profesor, tutor o invitado (uno a la vez, con prioridad).
 *   - Sesión alumno: un solo dispositivo a la vez.
 *
 * Las sesiones no persisten entre reinicios de la ESP32.
 * Los tokens se generan con esp_random() al iniciar cada sesión.
 *
 * Cada sesión del panel tiene dos relojes (valores en config/Limites.h):
 *   - Vida: se renueva con cualquier request (incluido el heartbeat).
 *     Vence si se cierra la pestaña o se pierde la conexión.
 *   - Inactividad: se renueva solo cuando el usuario usa la pantalla.
 * El alumno solo tiene reloj de vida.
 *
 * Cada sesión se usa desde una sola pestaña a la vez: el front manda en
 * cada request el id de su pestaña (header X-Pestana) y el módulo central solo acepta
 * los de la pestaña activa. Una pestaña nueva toma el control al iniciar
 * sesión o mandando X-Tomar-Pestana: 1.
 *
 * Es thread-safe: tick() corre en el loopTask y los handlers HTTP en la
 * tarea de AsyncTCP. Todos los métodos públicos toman un mutex recursivo
 * (salvo procesarPendientes() y reanudarPorReconexion(), que lo sueltan
 * mientras usan la BD) y los getters devuelven copias.
 *
 * tick() no toca la base de datos. Lo que requiere la BD (pausar el examen
 * cuando se desconecta el alumno) queda pendiente y lo ejecuta
 * procesarPendientes() desde la tarea web.
 */

// ---------------------------------------------------------------------------
// Structs internos de sesión
// ---------------------------------------------------------------------------

/**
 * @brief Sesión activa del panel (profesor, tutor o invitado).
 */
struct SesionPanel {
    String token = "";                   ///< 32 caracteres hexadecimales
    String rol = "";                     ///< "profesor" | "tutor" | "invitado"
    int idUsuario = 0;                   ///< 0 si es invitado
    String nombre = "";                  ///< Para mostrar en el panel
    unsigned long ultimoHeartbeat = 0;   ///< millis() del último request (reloj de vida)
    unsigned long ultimaInteraccion = 0; ///< millis() de la última acción del usuario
    String pestana = "";                 ///< Id de la pestaña que usa la sesión
    bool activa = false;
};

/**
 * @brief Sesión activa del alumno.
 */
struct SesionAlumno {
    String token = "";                 ///< 32 caracteres hexadecimales
    String idDispositivo = "";         ///< Generado por el front, identifica al dispositivo
    String pestana = "";               ///< Id de la pestaña que usa la sesión
    unsigned long ultimoHeartbeat = 0; ///< millis() del último request (reloj de vida)
    bool activa = false;
};

/**
 * @brief Pregunta del modo invitado almacenada en RAM.
 * @note Nunca se persiste en la BD. Se borra cuando el alumno la responde
 *       o al cerrar la sesión del invitado.
 */
struct PreguntaInvitado {
    String textoOpregunta = ""; ///< Texto de la pregunta.
    String opciones[4];
    int cantOpciones = 0; ///< 2 a 4.
    bool cargada = false;
};

/**
 * @brief Examen pausado automáticamente porque se desconectó el alumno.
 * @note Se reanuda solo si vuelve el mismo dispositivo.
 */
struct PausaDesconexion {
    int idCuestionario = 0;
    String idDispositivo = "";
    bool activa = false;
};

// ---------------------------------------------------------------------------
// Resultado de operaciones de sesión
// ---------------------------------------------------------------------------

/**
 * @brief Resultado de un intento de inicio de sesión.
 */
struct SesionResult {
    bool ok = false;
    String token = "";
    String mensaje = "";
    String codigo = ""; ///< Código de error para el front ("PANEL_OCUPADO", "ALUMNO_OCUPADO")
};

// ---------------------------------------------------------------------------
// SessionManager
// ---------------------------------------------------------------------------

/**
 * @brief Singleton que guarda en RAM la sesión del panel, la del alumno,
 * la pregunta del invitado y la pausa por desconexión.
 */

class SessionManager {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al SessionManager.
     */
    static SessionManager & getInstance() {
        static SessionManager instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    // Sesión panel
    // -----------------------------------------------------------------------

    /**
     * @brief Intenta iniciar sesión en el panel.
     *
     * Si el mismo usuario ya tenía una sesión abierta (otra pestaña u otro
     * dispositivo), la reemplaza.
     *
     * Si no, aplica la lógica de prioridad:
     *   tutor (3) > profesor (2) > invitado (1)
     *
     * Si el nuevo rol tiene mayor prioridad que el actual, expulsa la sesión
     * existente y crea una nueva. Si tiene igual o menor prioridad, rechaza.
     *
     * @param rol       "profesor" | "tutor" | "invitado"
     * @param idUsuario Id del usuario. 0 si es invitado.
     * @param nombre    Nombre para mostrar en el panel.
     * @return SesionResult con ok=true y token si se creó la sesión.
     */
    SesionResult iniciarSesionPanel(const String & rol, int idUsuario, const String & nombre);

    /**
     * @brief Cierra la sesión del panel activa.
     * @param motivo Motivo que se le informa al front si vuelve a usar el token
     *               ("logout" | "inactividad" | "desconexion" | "expulsado" | "reemplazada").
     * @note Si era invitado, también limpia la PreguntaInvitado en RAM.
     */
    void cerrarSesionPanel(const String & motivo);

    /**
     * @brief Verifica si un token de panel es válido.
     * @note No renueva la sesión (eso lo hace heartbeatPanel()).
     * @param token Token a verificar.
     * @return true si el token corresponde a la sesión activa.
     */
    bool verificarTokenPanel(const String & token);

    /**
     * @brief Renueva la sesión panel: siempre el reloj de vida y, si hubo
     * interacción, también el de inactividad.
     * @param token       Token de la sesión a actualizar.
     * @param interaccion true si el usuario usó la pantalla (renueva también la inactividad).
     * @return true si el token era válido y se actualizó.
     */
    bool heartbeatPanel(const String & token, bool interaccion);

    /**
     * @brief Hace que la sesión panel pase a usarse desde esta pestaña.
     * @param token   Token de la sesión.
     * @param pestana Id de la pestaña que toma el control.
     * @return true si el token era válido.
     */
    bool asignarPestanaPanel(const String & token, const String & pestana);

    /**
     * @brief Indica si la pestaña es la que tiene el control de la sesión panel.
     * @param token   Token de la sesión.
     * @param pestana Id de la pestaña que hizo el request.
     * @return true si el token es válido y la pestaña es la activa.
     */
    bool esPestanaPanel(const String & token, const String & pestana);

    /**
     * @brief Devuelve por qué se cerró la sesión de un token.
     * @param token Token que ya no es válido.
     * @return Motivo del cierre, o "" si ese token no es el de la última sesión cerrada.
     */
    String motivoCierrePanel(const String & token);

    /**
     * @brief Devuelve una copia de la sesión panel activa.
     * @return Copia de la SesionPanel.
     */
    SesionPanel getSesionPanel() const;

    /**
     * @brief Indica si hay una sesión panel activa.
     * @return true si hay una sesión activa.
     */
    bool hayPanelActivo() const;

    /**
     * @brief Nombre de la sesión panel activa, para la pantalla.
     * @return Nombre, o "" si no hay sesión.
     */
    String nombrePanelActivo() const;

    /**
     * @brief Cambia el nombre de la sesión panel activa (se usa al editar el
     * perfil, para que la pantalla muestre el nombre nuevo).
     * @param nuevoNombre Nombre y apellido nuevos.
     * @note Si no hay sesión activa no hace nada.
     */
    void actualizarNombrePanel(const String & nuevoNombre);

    // -----------------------------------------------------------------------
    // Sesión alumno
    // -----------------------------------------------------------------------

    /**
     * @brief Intenta iniciar sesión del alumno.
     * @note Solo puede haber un alumno conectado a la vez. Si el que pide es
     *       el mismo dispositivo que ya tiene la sesión, devuelve el mismo
     *       token (así reintentar es seguro y no quedan sesiones huérfanas).
     * @param idDispositivo Identificador del dispositivo, generado por el front.
     * @return SesionResult con ok=true y token si se creó (o recuperó) la sesión.
     *         ok=false y codigo "ALUMNO_OCUPADO" si hay otro dispositivo conectado.
     */
    SesionResult iniciarSesionAlumno(const String & idDispositivo);

    /**
     * @brief Cierra la sesión del alumno a pedido del alumno.
     * @param token Token de la sesión a cerrar.
     * @note No borra las respuestas en BD, solo libera el slot. Si había un
     *       examen en progreso queda pendiente pausarlo, igual que en una
     *       desconexión (ver procesarPendientes()).
     * @return true si el token era válido y se cerró.
     */
    bool cerrarSesionAlumno(const String & token);

    /**
     * @brief Verifica si un token de alumno es válido.
     * @param token Token a verificar.
     * @return true si el token corresponde a la sesión activa del alumno.
     */
    bool verificarTokenAlumno(const String & token);

    /**
     * @brief Hace que la sesión del alumno pase a usarse desde esta pestaña.
     * @param token   Token de la sesión.
     * @param pestana Id de la pestaña que toma el control.
     * @return true si el token era válido.
     */
    bool asignarPestanaAlumno(const String & token, const String & pestana);

    /**
     * @brief Indica si la pestaña es la que tiene el control de la sesión del alumno.
     * @param token   Token de la sesión.
     * @param pestana Id de la pestaña que hizo el request.
     * @return true si el token es válido y la pestaña es la activa.
     */
    bool esPestanaAlumno(const String & token, const String & pestana);

    /**
     * @brief Renueva el reloj de vida de la sesión del alumno.
     * @param token Token de la sesión a actualizar.
     * @return true si el token era válido y se actualizó.
     */
    bool heartbeatAlumno(const String & token);

    /**
     * @brief Indica si hay un alumno conectado.
     * @return true si hay una sesión de alumno activa.
     */
    bool hayAlumnoActivo() const;

    // -----------------------------------------------------------------------
    // Pregunta del invitado (RAM)
    // -----------------------------------------------------------------------

    /**
     * @brief Guarda la pregunta del invitado en RAM (reemplaza la anterior).
     * @param pregunta Struct con texto y opciones de la pregunta.
     */
    void guardarPreguntaInvitado(const PreguntaInvitado & pregunta);

    /**
     * @brief Devuelve una copia de la pregunta del invitado almacenada en RAM.
     * @return Copia de la PreguntaInvitado.
     */
    PreguntaInvitado getPreguntaInvitado() const;

    /**
     * @brief Limpia la pregunta del invitado de la RAM.
     */
    void limpiarPreguntaInvitado();

    // -----------------------------------------------------------------------
    // Pausa por desconexión del alumno
    // -----------------------------------------------------------------------

    /**
     * @brief Ejecuta lo que tick() dejó pendiente y requiere la BD.
     *
     * Si la sesión del alumno venció o la cerró el alumno y hay un examen
     * en progreso, lo pausa y anota la pausa por desconexión.
     *
     * La llaman verificarSesionPanel(), verificarSesionAlumno(),
     * RespuestaService::iniciarSesion() y /api/alumno/salir.
     *
     * @note Llamar solo desde la tarea web (handlers HTTP), nunca desde loop().
     */
    void procesarPendientes();

    /**
     * @brief Reanuda el examen pausado por desconexión si vuelve el mismo dispositivo.
     * @param idDispositivo Dispositivo que acaba de iniciar sesión.
     * @note No reanuda si el profesor ya lo cambió de estado o si hay otro
     *       cuestionario en progreso. Llamar solo desde la tarea web.
     */
    void reanudarPorReconexion(const String & idDispositivo);

    /**
     * @brief Olvida la pausa por desconexión.
     * @note Se llama cuando el profesor cambia a mano el estado de un
     *       cuestionario: a partir de ahí decide él.
     */
    void limpiarPausaDesconexion();

    // -----------------------------------------------------------------------
    // Tick de timeouts (llamar desde loop())
    // -----------------------------------------------------------------------

    /**
     * @brief Verifica y aplica timeouts de sesiones inactivas.
     *
     * Debe llamarse periódicamente desde el loop() de main.cpp (cada 1 s).
     * Cierra automáticamente las sesiones que superaron su tiempo límite
     * (motivo "desconexion" o "inactividad"). Si vence la del alumno, deja
     * pendiente la pausa del examen. No toca la BD (ver procesarPendientes()).
     */
    void tick();

private:
    SessionManager();
    SessionManager(const SessionManager &) = delete;
    SessionManager & operator=(const SessionManager &) = delete;

    /// @brief Mutex recursivo que protege todo el estado de abajo.
    SemaphoreHandle_t _mutex;

    SesionPanel _panel;                 ///< Sesión del panel (profesor, tutor o invitado).
    SesionAlumno _alumno;               ///< Sesión del alumno.
    PreguntaInvitado _preguntaInvitado; ///< Pregunta cargada por el invitado.
    PausaDesconexion _pausaDesconexion; ///< Examen pausado por desconexión del alumno.

    /// @brief Token de la última sesión panel cerrada.
    String _tokenPanelCerrado = "";
    /// @brief Motivo del cierre de la última sesión panel.
    String _motivoPanelCerrado = "";

    /// @brief La sesión del alumno se cerró y falta pausar el examen (ver procesarPendientes()).
    bool _alumnoExpirado = false;
    /// @brief Dispositivo de la sesión del alumno que se cerró.
    String _dispositivoExpirado = "";

    /**
     * @brief Cierra la sesión panel. Requiere tener tomado el mutex.
     *
     * Guarda token y motivo para motivoCierrePanel(). Si era invitado,
     * borra su pregunta. Si no había sesión no hace nada.
     *
     * @param motivo Motivo del cierre.
     */
    void _cerrarPanel(const String & motivo);

    /**
     * @brief Genera un token aleatorio de 32 caracteres hexadecimales.
     * @return Token generado con esp_random().
     */
    String _generarToken();

    /**
     * @brief Devuelve el nivel de prioridad de un rol.
     * @param rol "tutor" | "profesor" | "invitado"
     * @return 3 para tutor, 2 para profesor, 1 para invitado, 0 para desconocido.
     */
    int _prioridad(const String & rol);
};

#endif // SESSION_MANAGER_H
