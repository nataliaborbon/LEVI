#include "session/SessionManager.h"

// ---------------------------------------------------------------------------
// Bloqueo del mutex
// ---------------------------------------------------------------------------

namespace {
/**
 * @brief Toma el mutex recursivo al construirse y lo libera al destruirse
 * (al salir del bloque donde se declaró).
 */
class Bloqueo {
public:
    /**
     * @brief Toma el mutex, esperando lo que haga falta.
     * @param mutex Mutex recursivo a tomar.
     */
    explicit Bloqueo(SemaphoreHandle_t mutex) : _mutex(mutex) {
        xSemaphoreTakeRecursive(_mutex, portMAX_DELAY);
    }
    /// @brief Libera el mutex.
    ~Bloqueo() {
        xSemaphoreGiveRecursive(_mutex);
    }

private:
    SemaphoreHandle_t _mutex;
};
}

/// @brief Crea el mutex recursivo que protege todo el estado.
SessionManager::SessionManager() {
    _mutex = xSemaphoreCreateRecursiveMutex();
}

// ---------------------------------------------------------------------------
// Helpers privados
// ---------------------------------------------------------------------------

String SessionManager::_generarToken() {
    String token = "";
    for (int i = 0; i < 16; i++) {
        byte b = (byte)(esp_random() % 256);
        if (b < 16)
            token += "0";
        token += String(b, HEX);
    }
    return token;
}

int SessionManager::_prioridad(const String & rol) {
    if (rol == "tutor")
        return 3;
    if (rol == "profesor")
        return 2;
    if (rol == "invitado")
        return 1;
    return 0;
}

void SessionManager::_cerrarPanel(const String & motivo) {
    if (!_panel.activa)
        return;

    if (_panel.rol == "invitado")
        limpiarPreguntaInvitado();

    _tokenPanelCerrado = _panel.token;
    _motivoPanelCerrado = motivo;
    _panel = SesionPanel();
}

// ---------------------------------------------------------------------------
// Sesión panel
// ---------------------------------------------------------------------------

SesionResult SessionManager::iniciarSesionPanel(const String & rol, int idUsuario, const String & nombre) {
    Bloqueo bloqueo(_mutex);

    SesionResult result;
    int prioridadNuevo = _prioridad(rol);
    int prioridadActual = _panel.activa ? _prioridad(_panel.rol) : 0;

    if (_panel.activa) {
        bool mismoUsuario = idUsuario != 0 && idUsuario == _panel.idUsuario;

        if (mismoUsuario) {
            _cerrarPanel("reemplazada");
        } else if (prioridadNuevo > prioridadActual) {
            _cerrarPanel("expulsado");
        } else if (prioridadNuevo == prioridadActual) {
            result.mensaje = "Ya hay un " + rol + " conectado.";
            result.codigo = "PANEL_OCUPADO";
            return result;
        } else {
            result.mensaje = "Hay un " + _panel.rol + " conectado con mayor prioridad.";
            result.codigo = "PANEL_OCUPADO";
            return result;
        }
    }

    unsigned long ahora = millis();
    _panel.token = _generarToken();
    _panel.rol = rol;
    _panel.idUsuario = idUsuario;
    _panel.nombre = nombre;
    _panel.ultimoHeartbeat = ahora;
    _panel.ultimaInteraccion = ahora;
    _panel.activa = true;

    result.ok = true;
    result.token = _panel.token;
    return result;
}

void SessionManager::cerrarSesionPanel(const String & motivo) {
    Bloqueo bloqueo(_mutex);
    _cerrarPanel(motivo);
}

bool SessionManager::verificarTokenPanel(const String & token) {
    Bloqueo bloqueo(_mutex);
    return _panel.activa && token.length() > 0 && _panel.token == token;
}

bool SessionManager::heartbeatPanel(const String & token, bool interaccion) {
    Bloqueo bloqueo(_mutex);
    if (!verificarTokenPanel(token))
        return false;

    unsigned long ahora = millis();
    _panel.ultimoHeartbeat = ahora;
    if (interaccion)
        _panel.ultimaInteraccion = ahora;
    return true;
}

bool SessionManager::asignarPestanaPanel(const String & token, const String & pestana) {
    Bloqueo bloqueo(_mutex);
    if (!verificarTokenPanel(token))
        return false;
    _panel.pestana = pestana;
    return true;
}

bool SessionManager::esPestanaPanel(const String & token, const String & pestana) {
    Bloqueo bloqueo(_mutex);
    return verificarTokenPanel(token) && _panel.pestana == pestana;
}

String SessionManager::motivoCierrePanel(const String & token) {
    Bloqueo bloqueo(_mutex);
    if (token.length() == 0 || token != _tokenPanelCerrado)
        return "";
    return _motivoPanelCerrado;
}

SesionPanel SessionManager::getSesionPanel() const {
    Bloqueo bloqueo(_mutex);
    return _panel;
}

bool SessionManager::hayPanelActivo() const {
    Bloqueo bloqueo(_mutex);
    return _panel.activa;
}

String SessionManager::nombrePanelActivo() const {
    Bloqueo bloqueo(_mutex);
    return _panel.activa ? _panel.nombre : String("");
}

void SessionManager::actualizarNombrePanel(const String & nuevoNombre) {
    Bloqueo bloqueo(_mutex);
    if (_panel.activa) {
        _panel.nombre = nuevoNombre;
    }
}

// ---------------------------------------------------------------------------
// Sesión alumno
// ---------------------------------------------------------------------------

SesionResult SessionManager::iniciarSesionAlumno(const String & idDispositivo) {
    Bloqueo bloqueo(_mutex);

    SesionResult result;

    if (_alumno.activa) {
        if (_alumno.idDispositivo == idDispositivo) {
            _alumno.ultimoHeartbeat = millis();
            result.ok = true;
            result.token = _alumno.token;
            return result;
        }
        result.mensaje = "Ya hay un alumno conectado.";
        result.codigo = "ALUMNO_OCUPADO";
        return result;
    }

    _alumno.token = _generarToken();
    _alumno.idDispositivo = idDispositivo;
    _alumno.ultimoHeartbeat = millis();
    _alumno.activa = true;

    result.ok = true;
    result.token = _alumno.token;
    return result;
}

bool SessionManager::cerrarSesionAlumno(const String & token) {
    Bloqueo bloqueo(_mutex);
    if (!verificarTokenAlumno(token))
        return false;
    _alumnoExpirado = true;
    _dispositivoExpirado = _alumno.idDispositivo;
    _alumno = SesionAlumno();
    return true;
}

bool SessionManager::verificarTokenAlumno(const String & token) {
    Bloqueo bloqueo(_mutex);
    return _alumno.activa && token.length() > 0 && _alumno.token == token;
}

bool SessionManager::asignarPestanaAlumno(const String & token, const String & pestana) {
    Bloqueo bloqueo(_mutex);
    if (!verificarTokenAlumno(token))
        return false;
    _alumno.pestana = pestana;
    return true;
}

bool SessionManager::esPestanaAlumno(const String & token, const String & pestana) {
    Bloqueo bloqueo(_mutex);
    return verificarTokenAlumno(token) && _alumno.pestana == pestana;
}

bool SessionManager::heartbeatAlumno(const String & token) {
    Bloqueo bloqueo(_mutex);
    if (!verificarTokenAlumno(token))
        return false;
    _alumno.ultimoHeartbeat = millis();
    return true;
}

bool SessionManager::hayAlumnoActivo() const {
    Bloqueo bloqueo(_mutex);
    return _alumno.activa;
}

// ---------------------------------------------------------------------------
// Pregunta del invitado
// ---------------------------------------------------------------------------

void SessionManager::guardarPreguntaInvitado(const PreguntaInvitado & pregunta) {
    Bloqueo bloqueo(_mutex);
    _preguntaInvitado = pregunta;
}

PreguntaInvitado SessionManager::getPreguntaInvitado() const {
    Bloqueo bloqueo(_mutex);
    return _preguntaInvitado;
}

void SessionManager::limpiarPreguntaInvitado() {
    Bloqueo bloqueo(_mutex);
    _preguntaInvitado = PreguntaInvitado();
}

// ---------------------------------------------------------------------------
// Pausa por desconexión del alumno (tarea web)
// ---------------------------------------------------------------------------

void SessionManager::procesarPendientes() {
    String dispositivo;
    {
        Bloqueo bloqueo(_mutex);
        if (!_alumnoExpirado)
            return;
        _alumnoExpirado = false;
        dispositivo = _dispositivoExpirado;
        _dispositivoExpirado = "";
    }

    Cuestionario activo = CuestionarioRepository::getInstance().obtenerActivo();
    if (activo.idCuestionario == 0 || activo.estado != "en_progreso")
        return;

    CuestionarioResult r = CuestionarioService::getInstance().pausar(activo.idCuestionario, activo.idUsuario);
    if (!r.ok)
        return;

    Bloqueo bloqueo(_mutex);
    _pausaDesconexion.idCuestionario = activo.idCuestionario;
    _pausaDesconexion.idDispositivo = dispositivo;
    _pausaDesconexion.activa = true;
}

void SessionManager::reanudarPorReconexion(const String & idDispositivo) {
    int idCuestionario = 0;
    {
        Bloqueo bloqueo(_mutex);
        if (!_pausaDesconexion.activa || _pausaDesconexion.idDispositivo != idDispositivo)
            return;
        idCuestionario = _pausaDesconexion.idCuestionario;
        _pausaDesconexion = PausaDesconexion();
    }

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);
    if (c.idCuestionario == 0 || c.estado != "pausado")
        return;

    if (CuestionarioRepository::getInstance().hayUnoEnProgreso())
        return;

    CuestionarioService::getInstance().reanudar(idCuestionario, c.idUsuario);
}

void SessionManager::limpiarPausaDesconexion() {
    Bloqueo bloqueo(_mutex);
    _pausaDesconexion = PausaDesconexion();
}

// ---------------------------------------------------------------------------
// Tick de timeouts
// ---------------------------------------------------------------------------

void SessionManager::tick() {
    Bloqueo bloqueo(_mutex);

    unsigned long ahora = millis();

    if (_panel.activa) {
        bool esInvitado = (_panel.rol == "invitado");
        unsigned long vida = esInvitado ? SESION_INVITADO_VIDA_MS : SESION_PANEL_VIDA_MS;
        unsigned long inactividad = esInvitado ? SESION_INVITADO_INACTIVIDAD_MS : SESION_PANEL_INACTIVIDAD_MS;

        if (ahora - _panel.ultimoHeartbeat > vida) {
            _cerrarPanel("desconexion");
        } else if (ahora - _panel.ultimaInteraccion > inactividad) {
            _cerrarPanel("inactividad");
        }
    }

    if (_alumno.activa && ahora - _alumno.ultimoHeartbeat > SESION_ALUMNO_VIDA_MS) {
        _alumnoExpirado = true;
        _dispositivoExpirado = _alumno.idDispositivo;
        _alumno = SesionAlumno();
    }
}
