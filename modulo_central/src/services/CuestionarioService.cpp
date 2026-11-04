#include "services/CuestionarioService.h"

// ---------------------------------------------------------------------------
// Helpers privados
// ---------------------------------------------------------------------------

bool CuestionarioService::_esDuenio(int idCuestionario, int idUsuario) {
    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);
    return c.idCuestionario != 0 && c.idUsuario == idUsuario;
}

bool CuestionarioService::_validarCuestionario(const Cuestionario & c, String & error) {
    if (c.titulo.length() == 0) {
        error = "El título no puede estar vacío.";
        return false;
    }

    bool repetido =
        (c.idCuestionario == 0)
            ? CuestionarioRepository::getInstance().existeTitulo(c.idUsuario, c.titulo)
            : CuestionarioRepository::getInstance().existeTituloExcluyendo(c.idUsuario, c.titulo, c.idCuestionario);

    if (repetido) {
        error = "Ya existe un cuestionario con ese título.";
        return false;
    }

    return true;
}

bool CuestionarioService::_insertarPreguntas(int idCuestionario, const PreguntaCompleta * preguntas, int cant,
                                             String & error) {
    for (int i = 0; i < cant; i++) {
        esp_task_wdt_reset();
        yield();

        Pregunta p = preguntas[i].pregunta;
        p.idCuestionario = idCuestionario;

        DbResult dbPreg = PreguntaRepository::getInstance().crear(p);
        if (!dbPreg.ok) {
            error = "Error creando pregunta " + String(i + 1) + ": " + dbPreg.mensaje;
            return false;
        }

        int idPregunta = dbPreg.id;
        int idOpcionCorrecta = 0;

        for (int j = 0; j < preguntas[i].cantOpciones; j++) {
            esp_task_wdt_reset();
            yield();

            DbResult dbOpc = OpcionRepository::getInstance().crear(idPregunta, preguntas[i].opciones[j].opcion);
            if (!dbOpc.ok) {
                error = "Error creando opción en pregunta " + String(i + 1) + ": " + dbOpc.mensaje;
                return false;
            }

            if (preguntas[i].opciones[j].esCorrecta)
                idOpcionCorrecta = dbOpc.id;
        }

        if (idOpcionCorrecta == 0) {
            error = "No se encontró opción correcta en pregunta " + String(i + 1);
            return false;
        }

        esp_task_wdt_reset();

        DbResult dbCorrecta = PreguntaRepository::getInstance().asignarOpcionCorrecta(idPregunta, idOpcionCorrecta);
        if (!dbCorrecta.ok) {
            error = "Error al asignar opción correcta en pregunta " + String(i + 1) + ": " + dbCorrecta.mensaje;
            return false;
        }

        esp_task_wdt_reset();
        vTaskDelay(1);
    }

    return true;
}

bool CuestionarioService::_validarPreguntas(const PreguntaCompleta * preguntas, int cant, String & error) {
    if (cant < 1) {
        error = "El cuestionario debe tener al menos una pregunta.";
        return false;
    }

    for (int i = 0; i < cant; i++) {
        if (preguntas[i].pregunta.pregunta.length() == 0) {
            error = "Pregunta vacía";
            return false;
        }

        if (preguntas[i].cantOpciones < 2 || preguntas[i].cantOpciones > 4) {
            error = "Cantidad de opciones inválida";
            return false;
        }

        int correctas = 0;
        for (int j = 0; j < preguntas[i].cantOpciones; j++)
            if (preguntas[i].opciones[j].esCorrecta)
                correctas++;

        if (correctas != 1) {
            error = "La pregunta " + String(i + 1) + " debe tener una correcta.";
            return false;
        }
    }

    return true;
}

float CuestionarioService::_calcularPuntaje(int idCuestionario) {
    Pregunta preguntas[MAX_PREGUNTAS_POR_CUESTIONARIO];
    int cant = PreguntaRepository::getInstance().listarPorCuestionario(idCuestionario, preguntas,
                                                                       MAX_PREGUNTAS_POR_CUESTIONARIO);

    float puntaje = 0.0f;
    for (int i = 0; i < cant; i++) {
        if (preguntas[i].idOpcionElegida == 0)
            continue; // No respondida

        if (preguntas[i].idOpcionElegida == preguntas[i].idOpcionCorrecta) {
            puntaje += preguntas[i].puntajeCorrecta;
        } else {
            puntaje -= preguntas[i].puntajeIncorrecta;
        }
    }
    return puntaje;
}

float CuestionarioService::_calcularPuntajeMaximo(int idCuestionario) {
    Pregunta preguntas[MAX_PREGUNTAS_POR_CUESTIONARIO];
    int cant = PreguntaRepository::getInstance().listarPorCuestionario(idCuestionario, preguntas,
                                                                       MAX_PREGUNTAS_POR_CUESTIONARIO);

    float maximo = 0.0f;
    for (int i = 0; i < cant; i++) {
        maximo += preguntas[i].puntajeCorrecta;
    }
    return maximo;
}

// ---------------------------------------------------------------------------
// Cronómetro (Impulsado por Heartbeats)
// ---------------------------------------------------------------------------

void CuestionarioService::_iniciarCronometro(int idCuestionario) {
    portENTER_CRITICAL(&_cronometroMux);
    _idCuestionarioTimer = idCuestionario;
    _tiempoAcumuladoSeg = 0;
    _tiempoAcumuladoMs = 0;
    _ultimoHeartbeatMs = millis();
    _ultimoGuardadoMs = _ultimoHeartbeatMs;
    portEXIT_CRITICAL(&_cronometroMux);
}

DbResult CuestionarioService::_guardarTiempoCronometro(int idCuestionario) {
    bool debePersistir = false;
    int tiempoAPersistir = 0;

    portENTER_CRITICAL(&_cronometroMux);
    if (_idCuestionarioTimer == idCuestionario) {
        tiempoAPersistir = _tiempoAcumuladoSeg;
        debePersistir = true;
    }
    portEXIT_CRITICAL(&_cronometroMux);

    if (!debePersistir) {
        DbResult nada;
        nada.ok = true;
        return nada;
    }

    return CuestionarioRepository::getInstance().actualizarTiempoParcial(idCuestionario, tiempoAPersistir);
}

void CuestionarioService::detenerCronometro(int idCuestionario) {
    portENTER_CRITICAL(&_cronometroMux);
    if (_idCuestionarioTimer == idCuestionario) {
        _idCuestionarioTimer = 0;
    }
    portEXIT_CRITICAL(&_cronometroMux);
}

void CuestionarioService::_reanudarCronometro(int idCuestionario) {
    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    portENTER_CRITICAL(&_cronometroMux);
    _tiempoAcumuladoSeg = c.tiempoSegundos;
    _tiempoAcumuladoMs = (unsigned long)c.tiempoSegundos * 1000UL;
    _idCuestionarioTimer = idCuestionario;
    _ultimoHeartbeatMs = millis();
    _ultimoGuardadoMs = _ultimoHeartbeatMs;
    portEXIT_CRITICAL(&_cronometroMux);
}

int CuestionarioService::_tiempoTranscurridoSeg(int idCuestionario, int tiempoGuardado) {
    portENTER_CRITICAL(&_cronometroMux);
    int seg = (_idCuestionarioTimer == idCuestionario) ? _tiempoAcumuladoSeg : tiempoGuardado;
    portEXIT_CRITICAL(&_cronometroMux);
    return seg;
}

void CuestionarioService::procesarHeartbeatCronometro(int idCuestionario) {
    portENTER_CRITICAL(&_cronometroMux);
    bool midiendo = (_idCuestionarioTimer == idCuestionario);
    portEXIT_CRITICAL(&_cronometroMux);

    if (!midiendo) {
        _reanudarCronometro(idCuestionario);
        return;
    }

    bool guardar = false;
    int tiempoAGuardar = 0;

    portENTER_CRITICAL(&_cronometroMux);
    if (_idCuestionarioTimer == idCuestionario) {
        unsigned long ahora = millis();
        unsigned long deltaMs = ahora - _ultimoHeartbeatMs;

        if (deltaMs > HEARTBEAT_MAX_GAP_MS) {
            deltaMs = 0;
        }

        _tiempoAcumuladoMs += deltaMs;
        _tiempoAcumuladoSeg = (int)(_tiempoAcumuladoMs / 1000UL);
        _ultimoHeartbeatMs = ahora;

        if (ahora - _ultimoGuardadoMs >= GUARDADO_PERIODICO_MS) {
            guardar = true;
            tiempoAGuardar = _tiempoAcumuladoSeg;
            _ultimoGuardadoMs = ahora;
        }
    }
    portEXIT_CRITICAL(&_cronometroMux);

    if (guardar) {
        CuestionarioRepository::getInstance().actualizarTiempoParcial(idCuestionario, tiempoAGuardar);
    }
}

// ---------------------------------------------------------------------------
// Crear
// ---------------------------------------------------------------------------

CuestionarioResult CuestionarioService::crear(const Cuestionario & c, const PreguntaCompleta * preguntas, int cant) {
    CuestionarioResult result;

    if (!_validarCuestionario(c, result.mensaje))
        return result;

    if (!_validarPreguntas(preguntas, cant, result.mensaje))
        return result;

    auto abortar = [](const String & motivo) -> CuestionarioResult {
        CuestionarioResult r;
        r.mensaje = motivo;

        DatabaseManager::getInstance().rollback();

        return r;
    };

    if (!DatabaseManager::getInstance().beginTransaction()) {
        result.mensaje = "Error iniciando transacción.";
        return result;
    }

    DbResult dbCues = CuestionarioRepository::getInstance().crear(c);
    if (!dbCues.ok)
        return abortar(dbCues.mensaje);

    int idCuestionario = dbCues.id;

    String error;
    if (!_insertarPreguntas(idCuestionario, preguntas, cant, error))
        return abortar(error);

    esp_task_wdt_reset();

    if (!DatabaseManager::getInstance().commit())
        return abortar("Error en commit del cuestionario.");

    esp_task_wdt_reset();

    result.ok = true;
    result.id = idCuestionario;
    return result;
}

// ---------------------------------------------------------------------------
// Eliminar
// ---------------------------------------------------------------------------

CuestionarioResult CuestionarioService::eliminar(int idCuestionario, int idUsuario) {
    CuestionarioResult result;

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    if (c.idCuestionario == 0) {
        result.mensaje = "Cuestionario no encontrado.";
        return result;
    }

    if (c.idUsuario != idUsuario) {
        result.mensaje = "No tenés permisos para eliminar este cuestionario.";
        return result;
    }

    if (c.estado == "en_progreso" || c.estado == "pausado") {
        result.mensaje = "No se puede eliminar un cuestionario en progreso o pausado.";
        return result;
    }

    esp_task_wdt_reset();
    DbResult db = CuestionarioRepository::getInstance().eliminar(idCuestionario);
    esp_task_wdt_reset();
    result.ok = db.ok;
    if (!result.ok)
        result.mensaje = db.mensaje;
    return result;
}

CuestionarioResult CuestionarioService::editar(const Cuestionario & c, const PreguntaCompleta * preguntas, int cant) {
    CuestionarioResult result;
    Cuestionario actual = CuestionarioRepository::getInstance().buscarPorId(c.idCuestionario);

    if (actual.idCuestionario == 0) {
        result.mensaje = "No encontrado.";
        return result;
    }
    if (actual.idUsuario != c.idUsuario) {
        result.mensaje = "No tenés permisos para editar este cuestionario.";
        return result;
    }
    if (actual.estado != "pendiente") {
        result.mensaje = "Solo se editan pendientes.";
        return result;
    }

    if (!_validarCuestionario(c, result.mensaje))
        return result;

    if (!_validarPreguntas(preguntas, cant, result.mensaje))
        return result;

    auto abortar = [](const String & motivo) -> CuestionarioResult {
        CuestionarioResult r;
        r.mensaje = motivo;

        DatabaseManager::getInstance().rollback();

        return r;
    };

    if (!DatabaseManager::getInstance().beginTransaction()) {
        result.mensaje = "Error iniciando transacción.";
        return result;
    }

    DbResult db = CuestionarioRepository::getInstance().actualizar(c);
    if (!db.ok)
        return abortar(db.mensaje);

    esp_task_wdt_reset();
    DbResult dbViejas = PreguntaRepository::getInstance().eliminarPorCuestionario(c.idCuestionario);
    if (!dbViejas.ok)
        return abortar("Error borrando las preguntas viejas: " + dbViejas.mensaje);

    String error;
    if (!_insertarPreguntas(c.idCuestionario, preguntas, cant, error))
        return abortar(error);

    esp_task_wdt_reset();

    if (!DatabaseManager::getInstance().commit())
        return abortar("Error en commit de la edición.");

    esp_task_wdt_reset();

    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Iniciar
// ---------------------------------------------------------------------------

CuestionarioResult CuestionarioService::iniciar(int idCuestionario, int idUsuario, const String & fechaInicio) {
    CuestionarioResult result;

    if (!_esDuenio(idCuestionario, idUsuario)) {
        result.mensaje = "Cuestionario no encontrado o sin permisos.";
        return result;
    }

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    if (c.estado != "pendiente") {
        result.mensaje = "Solo se puede iniciar un cuestionario pendiente.";
        return result;
    }

    DatabaseManager & bd = DatabaseManager::getInstance();
    if (!bd.beginTransaction()) {
        result.mensaje = "Error iniciando transacción.";
        return result;
    }

    CuestionarioResult pausa = _pausarActivoSiCorresponde(idCuestionario);
    if (!pausa.ok) {
        bd.rollback();
        return pausa;
    }

    DbResult db = CuestionarioRepository::getInstance().cambiarEstado(idCuestionario, "en_progreso", fechaInicio);
    if (!db.ok) {
        bd.rollback();
        result.mensaje = db.mensaje;
        return result;
    }

    if (!bd.commit()) {
        bd.rollback();
        result.mensaje = "Error al guardar el cambio de estado.";
        return result;
    }

    _iniciarCronometro(idCuestionario);
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Pausar
// ---------------------------------------------------------------------------

CuestionarioResult CuestionarioService::pausar(int idCuestionario, int idUsuario) {
    CuestionarioResult result;

    if (!_esDuenio(idCuestionario, idUsuario)) {
        result.mensaje = "Cuestionario no encontrado o sin permisos.";
        return result;
    }

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    if (c.estado != "en_progreso") {
        result.mensaje = "Solo se puede pausar un cuestionario en progreso.";
        return result;
    }

    DatabaseManager & bd = DatabaseManager::getInstance();
    if (!bd.beginTransaction()) {
        result.mensaje = "Error iniciando transacción.";
        return result;
    }

    DbResult db = CuestionarioRepository::getInstance().cambiarEstado(idCuestionario, "pausado");
    if (!db.ok) {
        bd.rollback();
        result.mensaje = db.mensaje;
        return result;
    }

    DbResult dbTiempo = _guardarTiempoCronometro(idCuestionario);
    if (!dbTiempo.ok) {
        bd.rollback();
        result.mensaje = "No se pudo guardar el tiempo del examen: " + dbTiempo.mensaje;
        return result;
    }

    if (!bd.commit()) {
        bd.rollback();
        result.mensaje = "Error al guardar el cambio de estado.";
        return result;
    }

    detenerCronometro(idCuestionario);
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Reanudar
// ---------------------------------------------------------------------------

CuestionarioResult CuestionarioService::reanudar(int idCuestionario, int idUsuario) {
    CuestionarioResult result;

    if (!_esDuenio(idCuestionario, idUsuario)) {
        result.mensaje = "Cuestionario no encontrado o sin permisos.";
        return result;
    }

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    if (c.estado != "pausado") {
        result.mensaje = "Solo se puede reanudar un cuestionario pausado.";
        return result;
    }

    DatabaseManager & bd = DatabaseManager::getInstance();
    if (!bd.beginTransaction()) {
        result.mensaje = "Error iniciando transacción.";
        return result;
    }

    CuestionarioResult pausa = _pausarActivoSiCorresponde(idCuestionario);
    if (!pausa.ok) {
        bd.rollback();
        return pausa;
    }

    DbResult db = CuestionarioRepository::getInstance().cambiarEstado(idCuestionario, "en_progreso");
    if (!db.ok) {
        bd.rollback();
        result.mensaje = db.mensaje;
        return result;
    }

    if (!bd.commit()) {
        bd.rollback();
        result.mensaje = "Error al guardar el cambio de estado.";
        return result;
    }

    _reanudarCronometro(idCuestionario);
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Finalizar
// ---------------------------------------------------------------------------

void CuestionarioService::_finalizarInterno(int idCuestionario, CuestionarioResult & result) {
    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    float puntaje = _calcularPuntaje(idCuestionario);
    float puntajeMaximo = _calcularPuntajeMaximo(idCuestionario);
    int tiempoSegundos = _tiempoTranscurridoSeg(idCuestionario, c.tiempoSegundos);

    DbResult db = CuestionarioRepository::getInstance().guardarResultado(idCuestionario, puntaje, tiempoSegundos);
    result.ok = db.ok;
    if (!result.ok) {
        result.mensaje = db.mensaje;
        return;
    }

    result.resultado.puntajeObtenido = puntaje;
    result.resultado.puntajeMaximo = puntajeMaximo;
    result.resultado.puntajeParaAprobar = c.puntajeParaAprobar;
    result.resultado.tiempoSegundos = tiempoSegundos;
}

CuestionarioResult CuestionarioService::finalizar(int idCuestionario, int idUsuario) {
    CuestionarioResult result;

    if (!_esDuenio(idCuestionario, idUsuario)) {
        result.mensaje = "Cuestionario no encontrado o sin permisos.";
        return result;
    }

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    if (c.estado != "en_progreso" && c.estado != "pausado") {
        result.mensaje = "Solo se puede finalizar un cuestionario en progreso o pausado.";
        return result;
    }

    _finalizarInterno(idCuestionario, result);
    if (result.ok)
        detenerCronometro(idCuestionario);
    return result;
}

CuestionarioResult CuestionarioService::finalizarComoAlumno(int idCuestionario) {
    CuestionarioResult result;

    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);

    if (c.idCuestionario == 0 || c.estado != "en_progreso") {
        result.mensaje = "El cuestionario no está en progreso.";
        return result;
    }

    int total = PreguntaRepository::getInstance().contarTotal(idCuestionario);
    int respondidas = PreguntaRepository::getInstance().contarRespondidas(idCuestionario);
    if (respondidas < total) {
        result.mensaje = "Todavía quedan preguntas sin responder.";
        return result;
    }

    _finalizarInterno(idCuestionario, result);
    return result;
}

// ---------------------------------------------------------------------------
// Pausar el activo
// ---------------------------------------------------------------------------
CuestionarioResult CuestionarioService::_pausarActivoSiCorresponde(int idCuestionarioAExcluir) {
    CuestionarioResult result;
    result.ok = true;

    Cuestionario activo = CuestionarioRepository::getInstance().obtenerActivo();
    if (activo.estado == "en_progreso" && activo.idCuestionario != idCuestionarioAExcluir) {
        DbResult dbPausa = CuestionarioRepository::getInstance().cambiarEstado(activo.idCuestionario, "pausado");
        if (!dbPausa.ok) {
            result.ok = false;
            result.mensaje = "No se pudo pausar el cuestionario en progreso: " + dbPausa.mensaje;
            return result;
        }

        DbResult dbTiempo = _guardarTiempoCronometro(activo.idCuestionario);
        if (!dbTiempo.ok) {
            result.ok = false;
            result.mensaje = "No se pudo guardar el tiempo del cuestionario en progreso: " + dbTiempo.mensaje;
            return result;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Obtener Completo
// ---------------------------------------------------------------------------
bool CuestionarioService::obtenerCompleto(int idCuestionario, Cuestionario & c, PreguntaCompleta * bufferPreguntas,
                                          int & cantPreguntas) {
    c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);
    if (c.idCuestionario == 0)
        return false;

    Pregunta * pregs = new Pregunta[MAX_PREGUNTAS_POR_CUESTIONARIO];
    cantPreguntas =
        PreguntaRepository::getInstance().listarPorCuestionario(idCuestionario, pregs, MAX_PREGUNTAS_POR_CUESTIONARIO);

    for (int i = 0; i < cantPreguntas; i++) {
        bufferPreguntas[i].pregunta = pregs[i];

        Opcion opcs[MAX_OPCIONES_POR_PREGUNTA];
        int cantOp =
            OpcionRepository::getInstance().listarPorPregunta(pregs[i].idPregunta, opcs, MAX_OPCIONES_POR_PREGUNTA);
        bufferPreguntas[i].cantOpciones = cantOp;

        for (int j = 0; j < cantOp; j++) {
            bufferPreguntas[i].opciones[j].opcion = opcs[j].opcion;
            bufferPreguntas[i].opciones[j].esCorrecta = (opcs[j].idOpcion == pregs[i].idOpcionCorrecta);
        }
    }

    delete[] pregs;
    return true;
}

// ---------------------------------------------------------------------------
// Listados
// ---------------------------------------------------------------------------

int CuestionarioService::listarProfesor(int idUsuario, CuestionarioResumenProfesor * buffer, int maxSize, int offset) {
    return CuestionarioRepository::getInstance().listarResumenProfesor(idUsuario, buffer, maxSize, offset);
}

int CuestionarioService::listarTutor(CuestionarioResumenTutor * buffer, int maxSize, int offset) {
    return CuestionarioRepository::getInstance().listarResumenTutor(buffer, maxSize, offset);
}

int CuestionarioService::contarProfesor(int idUsuario) {
    return CuestionarioRepository::getInstance().contarPorUsuario(idUsuario);
}

int CuestionarioService::contarTutor() {
    return CuestionarioRepository::getInstance().contarTodos();
}

int CuestionarioService::obtenerRevision(int idCuestionario, PreguntaRevision * buffer, int maxSize) {
    Cuestionario c = CuestionarioRepository::getInstance().buscarPorId(idCuestionario);
    if (c.idCuestionario == 0)
        return -1;

    return PreguntaRepository::getInstance().listarRevision(idCuestionario, buffer, maxSize);
}