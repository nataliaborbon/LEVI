#include "services/RespuestaService.h"

SesionResult RespuestaService::iniciarSesion(const String & idDispositivo) {
    SessionManager & sesiones = SessionManager::getInstance();

    sesiones.procesarPendientes();

    SesionResult result = sesiones.iniciarSesionAlumno(idDispositivo);
    if (result.ok)
        sesiones.reanudarPorReconexion(idDispositivo);
    return result;
}

EstadoAlumno RespuestaService::obtenerEstado() {
    EstadoAlumno estado = _calcularEstado();

    if ((long)(millis() - _finalizadoHastaMs) >= 0) {
        _actualizarResumenCache(estado);
    }

    if (estado.estado == "en_progreso") {
        Cuestionario activo = CuestionarioRepository::getInstance().obtenerActivo();
        if (activo.idCuestionario != 0) {
            CuestionarioService::getInstance().procesarHeartbeatCronometro(activo.idCuestionario);
        }
    }

    return estado;
}

void RespuestaService::_actualizarResumenCache(const EstadoAlumno & estado) {
    strncpy(_resumenCache.estado, estado.estado.c_str(), sizeof(_resumenCache.estado) - 1);
    _resumenCache.estado[sizeof(_resumenCache.estado) - 1] = '\0';
    _resumenCache.numeroPregunta = estado.hayPregunta ? estado.pregunta.numeroPregunta : 0;
    _resumenCache.totalPreguntas = estado.hayPregunta ? estado.pregunta.totalPreguntas : 0;
    strncpy(_resumenCache.tituloCuestionario, estado.tituloCuestionario.c_str(),
            sizeof(_resumenCache.tituloCuestionario) - 1);
    _resumenCache.tituloCuestionario[sizeof(_resumenCache.tituloCuestionario) - 1] = '\0';
}

void RespuestaService::_registrarFinalizacion(const ResultadoFinalizacion & resultado, bool aprobado,
                                              const String & titulo) {
    strncpy(_resumenCache.estado, "finalizado", sizeof(_resumenCache.estado) - 1);
    _resumenCache.estado[sizeof(_resumenCache.estado) - 1] = '\0';
    strncpy(_resumenCache.tituloCuestionario, titulo.c_str(), sizeof(_resumenCache.tituloCuestionario) - 1);
    _resumenCache.tituloCuestionario[sizeof(_resumenCache.tituloCuestionario) - 1] = '\0';
    _resumenCache.numeroPregunta = 0;
    _resumenCache.totalPreguntas = 0;
    _resumenCache.puntajeObtenido = resultado.puntajeObtenido;
    _resumenCache.puntajeParaAprobar = resultado.puntajeParaAprobar;
    _resumenCache.puntajeMaximo = resultado.puntajeMaximo;
    _resumenCache.tiempoSegundos = resultado.tiempoSegundos;
    _resumenCache.aprobado = aprobado;
    _finalizadoHastaMs = millis() + 15000UL;
}

EstadoAlumno RespuestaService::_calcularEstado() {
    EstadoAlumno estado;

    bool hayOficialEnCurso = CuestionarioRepository::getInstance().hayUnoEnProgreso();

    const SesionPanel & panel = SessionManager::getInstance().getSesionPanel();
    if (!hayOficialEnCurso && panel.activa && panel.rol == "invitado") {
        const PreguntaInvitado & pregInv = SessionManager::getInstance().getPreguntaInvitado();
        if (pregInv.cargada) {
            estado.estado = "invitado";
            estado.hayPregunta = true;
            estado.tituloCuestionario = "Modo Invitado";

            estado.pregunta.idPregunta = 0;
            estado.pregunta.textoPregunta = pregInv.textoOpregunta;
            estado.pregunta.numeroPregunta = 1;
            estado.pregunta.totalPreguntas = 1;
            estado.pregunta.cantOpciones = pregInv.cantOpciones;

            for (int i = 0; i < pregInv.cantOpciones; i++) {
                estado.pregunta.opciones[i].idOpcion = i;
                estado.pregunta.opciones[i].opcion = pregInv.opciones[i];
            }
            return estado;
        }
        estado.estado = "esperando";
        return estado;
    }

    Cuestionario activo = CuestionarioRepository::getInstance().obtenerActivo();

    if (activo.idCuestionario == 0) {
        estado.estado = "esperando";
        return estado;
    }

    estado.tituloCuestionario = activo.titulo;
    estado.estado = "en_progreso";

    PreguntaAlumno pregAlumno;
    bool hay = PreguntaRepository::getInstance().obtenerSiguienteParaAlumno(activo.idCuestionario, pregAlumno);

    if (!hay) {
        estado.hayPregunta = false;
        return estado;
    }

    pregAlumno.cantOpciones =
        OpcionRepository::getInstance().listarParaAlumno(pregAlumno.idPregunta, pregAlumno.opciones, 4);

    estado.hayPregunta = true;
    estado.pregunta = pregAlumno;
    return estado;
}

RespuestaResult RespuestaService::responder(int idPregunta, int idOpcion) {
    RespuestaResult result;

    if (idPregunta == 0) {
        SesionPanel panel = SessionManager::getInstance().getSesionPanel();
        PreguntaInvitado pregInv = SessionManager::getInstance().getPreguntaInvitado();

        if (!panel.activa || panel.rol != "invitado" || !pregInv.cargada) {
            result.mensaje = "La pregunta del invitado ya no está disponible.";
            result.codigo = "PREGUNTA_INVITADO_NO_DISPONIBLE";
            return result;
        }
        if (idOpcion < 0 || idOpcion >= pregInv.cantOpciones) {
            result.mensaje = "Opción inválida.";
            return result;
        }
        result.ok = true;
        result.finalizo = true;
        result.mensaje = pregInv.opciones[idOpcion];
        SessionManager::getInstance().limpiarPreguntaInvitado();
        return result;
    }

    Pregunta p = PreguntaRepository::getInstance().buscarPorId(idPregunta);
    if (p.idPregunta == 0) {
        result.mensaje = "Pregunta no encontrada.";
        return result;
    }

    Cuestionario activo = CuestionarioRepository::getInstance().obtenerActivo();
    if (activo.idCuestionario == 0 || p.idCuestionario != activo.idCuestionario) {
        result.mensaje = "No hay un examen activo o la pregunta no pertenece al mismo.";
        return result;
    }

    Opcion opc = OpcionRepository::getInstance().buscarPorId(idOpcion);
    if (opc.idOpcion == 0 || opc.idPregunta != idPregunta) {
        result.mensaje = "Opción inválida.";
        return result;
    }

    DatabaseManager & bd = DatabaseManager::getInstance();
    if (!bd.beginTransaction()) {
        result.mensaje = "Error al guardar la respuesta.";
        return result;
    }

    DbResult db = PreguntaRepository::getInstance().guardarRespuesta(idPregunta, idOpcion);
    if (!db.ok) {
        bd.rollback();
        result.mensaje = "Error al guardar la respuesta.";
        return result;
    }

    int total = PreguntaRepository::getInstance().contarTotal(activo.idCuestionario);
    int respondidas = PreguntaRepository::getInstance().contarRespondidas(activo.idCuestionario);
    bool esUltima = (respondidas >= total);

    CuestionarioResult finalRes;
    if (esUltima) {
        finalRes = CuestionarioService::getInstance().finalizarComoAlumno(activo.idCuestionario);
        if (!finalRes.ok) {
            bd.rollback();
            result.mensaje = "Error al finalizar el examen.";
            return result;
        }
    }

    if (!bd.commit()) {
        bd.rollback();
        result.mensaje = "Error al guardar la respuesta.";
        return result;
    }

    result.ok = true;
    result.fueCorrecto = (idOpcion == p.idOpcionCorrecta);
    result.finalizo = esUltima;

    if (esUltima) {
        CuestionarioService::getInstance().detenerCronometro(activo.idCuestionario);
        result.resultado = finalRes.resultado;
        result.aprobado = result.resultado.puntajeObtenido >= result.resultado.puntajeParaAprobar;
        _registrarFinalizacion(result.resultado, result.aprobado, activo.titulo);
    }

    return result;
}