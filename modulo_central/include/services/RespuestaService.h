#ifndef RESPUESTA_SERVICE_H
#define RESPUESTA_SERVICE_H

#include <Arduino.h>
#include "../storage/models/Models.h"
#include "../session/SessionManager.h"
#include "services/CuestionarioService.h"
#include "storage/database/repositories/CuestionarioRepository.h"
#include "storage/database/repositories/PreguntaRepository.h"
#include "storage/database/repositories/OpcionRepository.h"
#include "storage/database/DatabaseManager.h"
#include <string.h>

/**
 * @file RespuestaService.h
 * @brief Lógica del flujo del alumno durante el examen.
 *
 * Responsabilidades:
 *   - Iniciar (o recuperar) la sesión del alumno.
 *   - Determinar el estado actual del examen para mostrar en pantalla y
 *     hacer avanzar el cronómetro del examen.
 *   - Registrar la respuesta del alumno y evaluar si fue correcta.
 *   - Responder la pregunta del invitado guardada en RAM.
 *   - Mantener un resumen en RAM del estado para la pantalla del módulo central.
 */

/**
 * @brief Estado actual del examen para el alumno.
 */
struct EstadoAlumno {
    String estado = "esperando"; ///< "esperando" | "en_progreso" | "invitado"
    bool hayPregunta = false;
    String tituloCuestionario = ""; ///< "Modo Invitado" en el modo invitado.

    /// Datos de la pregunta actual (válidos si hayPregunta == true).
    PreguntaAlumno pregunta;
};

/**
 * @brief Resultado de registrar una respuesta del alumno.
 */
struct RespuestaResult {
    bool ok = false;
    bool fueCorrecto = false;
    bool finalizo = false; ///< true si era la última pregunta (siempre true en modo invitado).
    bool aprobado = false; ///< Válido solo si finalizo == true.
    String mensaje = "";   ///< Motivo del error; en modo invitado, el texto de la opción elegida.
    String codigo = "";    ///< Código de error para el front ("PREGUNTA_INVITADO_NO_DISPONIBLE")

    /// Datos de puntaje/tiempo, válidos solo si finalizo == true (modo normal).
    ResultadoFinalizacion resultado;
};

/**
 * @brief Snapshot liviano del último estado calculado por obtenerEstado().
 *
 * Se actualiza como efecto secundario cada vez que alguien llama a
 * obtenerEstado() (hoy: el handler HTTP de /api/alumno/estado, que ya
 * corre en la tarea del webserver). NO dispara ninguna consulta nueva a
 * la base — es solo una copia en RAM para que loop() en main.cpp pueda
 * leerla sin tocar la SD/SQLite desde una segunda tarea.
 *
 * Usa char[] en vez de String a propósito: son campos que se leen desde
 * otra tarea sin mutex, y un char[] fijo es más simple de copiar sin
 * riesgo que un String (que internamente maneja un puntero a heap).
 */
struct EstadoExamenResumen {
    char estado[16] = "esperando"; ///< "esperando" | "en_progreso" | "invitado" | "finalizado"
    int numeroPregunta = 0;
    int totalPreguntas = 0;
    char tituloCuestionario[64] = "";

    /// Válidos solo cuando estado == "finalizado" (ver _registrarFinalizacion).
    float puntajeObtenido = 0;
    float puntajeParaAprobar = 0;
    float puntajeMaximo = 0;
    float tiempoSegundos = 0;
    bool aprobado = false;
};

/**
 * @brief Singleton con la lógica de la pantalla del alumno.
 */
class RespuestaService {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al RespuestaService.
     */
    static RespuestaService & getInstance() {
        static RespuestaService instance;
        return instance;
    }

    /**
     * @brief Inicia (o recupera) la sesión del alumno.
     *
     * Primero aplica una desconexión pendiente (pausa el examen si
     * corresponde). Si el examen se había pausado porque este mismo
     * dispositivo se desconectó, lo reanuda.
     *
     * @param idDispositivo Identificador del dispositivo, generado por el front.
     * @return SesionResult con token si tuvo éxito.
     */
    SesionResult iniciarSesion(const String & idDispositivo);

    /**
     * @brief Devuelve el estado actual del examen para mostrar en la pantalla del alumno.
     *
     * Estados posibles:
     *   - "en_progreso" → hay un cuestionario activo. Trae la siguiente
     *                     pregunta sin responder, si queda alguna.
     *   - "invitado"    → no hay cuestionario en progreso, hay sesión de
     *                     invitado y cargó una pregunta. Trae esa pregunta.
     *   - "esperando"   → no hay nada para mostrar.
     *
     * El resultado de finalización NO viaja en este EstadoAlumno: llega en
     * la respuesta directa de responder() (ver RespuestaResult), porque es
     * en ese momento cuando efectivamente se calcula y persiste.
     *
     * Efectos secundarios:
     *   - Actualiza el resumen cacheado que usa la pantalla (ver
     *     obtenerResumenCacheado), salvo mientras haya una finalización
     *     reciente "congelada" ahí (ver _registrarFinalizacion).
     *   - Si hay un examen en progreso, hace avanzar su cronómetro (ver
     *     CuestionarioService::procesarHeartbeatCronometro()).
     *
     * @return EstadoAlumno con toda la información necesaria para el frontend.
     */
    EstadoAlumno obtenerEstado();

    /**
     * @brief Devuelve el último estado calculado, SIN tocar la base.
     *
     * Es una simple lectura de RAM (se actualiza como efecto secundario
     * dentro de obtenerEstado()). main.cpp lo lee en cada vuelta de loop()
     * y se lo pasa a ui_loop() para alimentar la pantalla del módulo central, sin
     * generar una segunda consulta a la SD/SQLite desde otra tarea.
     *
     * Justo después de que el alumno termina la última pregunta, este
     * resumen queda "congelado" en estado "finalizado" (con puntaje,
     * tiempo y si aprobó) durante 15 segundos, para que la pantalla tenga
     * tiempo de mostrar el resultado antes de que vuelva a "esperando".
     *
     * @return Copia del último EstadoExamenResumen conocido.
     */
    EstadoExamenResumen obtenerResumenCacheado() const {
        return _resumenCache;
    }

    /**
     * @brief Registra la respuesta del alumno a la pregunta actual.
     *
     * Si es modo normal: guarda idOpcionElegida en BD y evalúa si fue correcta.
     * Si es la última pregunta del cuestionario, delega en
     * CuestionarioService::finalizarComoAlumno() para calcular y persistir
     * el resultado, y lo devuelve ya calculado en RespuestaResult. La
     * respuesta y el resultado se guardan en una sola transacción: si algo
     * falla no queda guardado nada y se devuelve el error.
     * Si es modo invitado: devuelve el texto de la opción elegida en
     * mensaje (sin BD) y borra la pregunta del invitado de la RAM.
     *
     * @param idPregunta Id de la pregunta respondida (0 si es invitado).
     *                   Si es 0 y la pregunta del invitado ya no existe, devuelve
     *                   codigo "PREGUNTA_INVITADO_NO_DISPONIBLE".
     * @param idOpcion   Id de la opción elegida (índice 0-3 si es invitado).
     * @return RespuestaResult con si fue correcta y si finalizó el examen.
     */
    RespuestaResult responder(int idPregunta, int idOpcion);

private:
    RespuestaService() {
    }
    RespuestaService(const RespuestaService &) = delete;
    RespuestaService & operator=(const RespuestaService &) = delete;

    /**
     * @brief Lógica real de obtenerEstado() (sin tocar el cache ni el cronómetro).
     * @return EstadoAlumno calculado desde la BD y la sesión del invitado.
     */
    EstadoAlumno _calcularEstado();

    /**
     * @brief Actualiza _resumenCache a partir de un EstadoAlumno recién calculado.
     * @param estado Estado recién calculado.
     */
    void _actualizarResumenCache(const EstadoAlumno & estado);

    /**
     * @brief Congela el resumen cacheado en "finalizado" con los datos del
     * resultado durante 15 s (hasta _finalizadoHastaMs). Se llama desde
     * responder() cuando la última pregunta se acaba de contestar.
     *
     * Es necesario porque, para cuando el próximo obtenerEstado() se
     * calcule, el cuestionario ya no está "en_progreso" (obtenerActivo()
     * ya no lo encuentra) y _calcularEstado() daría "esperando" — pisando
     * el resultado antes de que la pantalla del módulo central llegue a mostrarlo.
     *
     * @param resultado Puntajes y tiempo del examen finalizado.
     * @param aprobado  Si alcanzó el puntaje para aprobar.
     * @param titulo    Título del cuestionario.
     */
    void _registrarFinalizacion(const ResultadoFinalizacion & resultado, bool aprobado, const String & titulo);

    /// @brief Último resumen del estado, para la pantalla del módulo central.
    EstadoExamenResumen _resumenCache;

    /// @brief millis() hasta el cual _resumenCache no debe ser pisado por el
    /// cálculo normal de obtenerEstado() (ver _registrarFinalizacion).
    unsigned long _finalizadoHastaMs = 0;
};

#endif // RESPUESTA_SERVICE_H