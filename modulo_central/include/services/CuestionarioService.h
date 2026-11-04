#ifndef CUESTIONARIO_SERVICE_H
#define CUESTIONARIO_SERVICE_H

#include <Arduino.h>
#include "../storage/models/Models.h"
#include "storage/database/repositories/CuestionarioRepository.h"
#include "storage/database/repositories/PreguntaRepository.h"
#include "storage/database/repositories/OpcionRepository.h"
#include "storage/database/DatabaseManager.h"
#include "esp_task_wdt.h"
#include "config/Limites.h"

/**
 * @file CuestionarioService.h
 * @brief Lógica de negocio para la gestión de cuestionarios.
 *
 * Responsabilidades:
 *   - Validar datos antes de crear o editar un cuestionario.
 *   - Validar permisos (solo el profesor dueño) y transiciones de estado:
 *       pendiente → en_progreso → pausado ⇄ en_progreso → finalizado.
 *   - Coordinar PreguntaRepository y OpcionRepository al crear/editar,
 *     dentro de una transacción.
 *   - Llevar el cronómetro del examen en RAM (impulsado por los pedidos
 *     de estado del alumno).
 *   - Calcular puntaje y tiempo al finalizar.
 *
 * Solo puede haber un cuestionario en progreso: al iniciar o reanudar uno,
 * el que estaba en progreso se pausa.
 *
 * Las acciones con varias escrituras en la BD (crear, editar, iniciar,
 * pausar y reanudar) las hacen dentro de una transacción. El cronómetro en
 * RAM se modifica recién después del COMMIT, así un rollback deja todo
 * como estaba.
 *
 * Mientras el examen está en progreso, el tiempo se guarda en la BD una vez
 * por minuto. Si la placa se reinicia, el cronómetro se retoma desde ese
 * valor en la siguiente consulta del alumno.
 */

/**
 * @brief Resultado estándar de operaciones de cuestionario.
 */
struct CuestionarioResult {
    bool ok = false;
    String mensaje = "";
    int id = 0; ///< Id generado al crear. 0 en las demás operaciones.

    /// Válido solo si la operación fue finalizar()/finalizarComoAlumno() y ok=true.
    ResultadoFinalizacion resultado;
};

class CuestionarioService {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al CuestionarioService.
     */
    static CuestionarioService & getInstance() {
        static CuestionarioService instance;
        return instance;
    }

    /**
     * @brief Crea un cuestionario con todas sus preguntas y opciones.
     *
     * Valida el cuestionario y las preguntas, y hace todos los INSERT en una
     * sola transacción: si algo falla, el rollback no deja nada a medio crear.
     * El cuestionario queda en estado 'pendiente'.
     *
     * @param c         Cuestionario a crear (idUsuario = profesor dueño).
     * @param preguntas Preguntas con sus opciones.
     * @param cant      Cantidad de preguntas.
     * @return CuestionarioResult con ok=true e id generado si tuvo éxito.
     */
    CuestionarioResult crear(const Cuestionario & c, const PreguntaCompleta * preguntas, int cant);

    /**
     * @brief Edita un cuestionario pendiente: título, puntaje para aprobar
     * y todas sus preguntas.
     *
     * Solo el dueño puede editarlo y solo si está 'pendiente'. Las preguntas
     * viejas se borran y se insertan las nuevas, todo en una sola
     * transacción: si algo falla, el cuestionario queda como estaba.
     *
     * @param c         Cuestionario con los datos nuevos (idCuestionario e
     *                  idUsuario deben ser válidos).
     * @param preguntas Preguntas nuevas con sus opciones.
     * @param cant      Cantidad de preguntas.
     * @return CuestionarioResult con ok=true si tuvo éxito.
     */
    CuestionarioResult editar(const Cuestionario & c, const PreguntaCompleta * preguntas, int cant);

    /**
     * @brief Pausa el cuestionario en progreso, si hay uno y no es el indicado.
     *
     * La usan iniciar() y reanudar() para que nunca haya dos en progreso.
     * Cambia su estado y guarda su tiempo acumulado en la BD; no toca el
     * cronómetro en RAM, porque quien la llama lo reemplaza al iniciar o
     * reanudar el otro.
     *
     * @note Debe llamarse dentro de la transacción de quien la usa, que
     *       tiene que hacer rollback si devuelve ok=false.
     *
     * @param idCuestionarioAExcluir Cuestionario que se está por iniciar o
     *                               reanudar (no se pausa).
     * @return CuestionarioResult con ok=true si no había nada que pausar o
     *         si se pausó bien.
     */
    CuestionarioResult _pausarActivoSiCorresponde(int idCuestionarioAExcluir);

    /**
     * @brief Carga un cuestionario con todas sus preguntas y opciones.
     *
     * Se usa para mostrarlo en el editor. De cada opción se cargan el texto
     * y si es la correcta (no su id).
     *
     * @param idCuestionario  Id del cuestionario.
     * @param c               Se completa con los datos del cuestionario.
     * @param bufferPreguntas Array destino de al menos MAX_PREGUNTAS_POR_CUESTIONARIO.
     * @param cantPreguntas   Se completa con la cantidad de preguntas cargadas.
     * @return true si el cuestionario existe.
     */
    bool obtenerCompleto(int idCuestionario, Cuestionario & c, PreguntaCompleta * bufferPreguntas, int & cantPreguntas);

    /**
     * @brief Elimina un cuestionario con sus preguntas y opciones.
     * @note Solo el dueño puede eliminarlo, y no si está en progreso o pausado.
     * @param idCuestionario Id del cuestionario.
     * @param idUsuario      Profesor que pide el borrado.
     * @return CuestionarioResult con ok=true si tuvo éxito.
     */
    CuestionarioResult eliminar(int idCuestionario, int idUsuario);

    /**
     * @brief Inicia un cuestionario pendiente (pendiente → en_progreso).
     *
     * En una transacción, pausa el que estuviera en progreso y guarda la
     * fecha de inicio. Después del COMMIT arranca el cronómetro desde 0.
     *
     * @param idCuestionario Id del cuestionario.
     * @param idUsuario      Profesor que lo inicia (debe ser el dueño).
     * @param fechaInicio    Fecha ISO enviada por el front.
     * @return CuestionarioResult con ok=true si tuvo éxito.
     */
    CuestionarioResult iniciar(int idCuestionario, int idUsuario, const String & fechaInicio);

    /**
     * @brief Pausa un cuestionario en progreso (en_progreso → pausado).
     * @details En una transacción, cambia el estado y guarda el tiempo
     * acumulado en la BD. Después del COMMIT detiene el cronómetro.
     * @param idCuestionario Id del cuestionario.
     * @param idUsuario      Profesor que lo pausa (debe ser el dueño).
     * @return CuestionarioResult con ok=true si tuvo éxito.
     */
    CuestionarioResult pausar(int idCuestionario, int idUsuario);

    /**
     * @brief Reanuda un cuestionario pausado (pausado → en_progreso).
     * @details En una transacción, pausa el que estuviera en progreso y
     * cambia el estado. Después del COMMIT retoma el cronómetro desde el
     * tiempo guardado en la BD.
     * @param idCuestionario Id del cuestionario.
     * @param idUsuario      Profesor que lo reanuda (debe ser el dueño).
     * @return CuestionarioResult con ok=true si tuvo éxito.
     */
    CuestionarioResult reanudar(int idCuestionario, int idUsuario);

    /**
     * @brief Finaliza un cuestionario a pedido del profesor
     * (en_progreso | pausado → finalizado).
     * @details Las preguntas sin responder no suman ni restan. Es una sola
     * escritura, que SQLite ya ejecuta como una transacción. Si se guarda
     * bien, detiene el cronómetro.
     * @param idCuestionario Id del cuestionario.
     * @param idUsuario      Profesor que lo finaliza (debe ser el dueño).
     * @return CuestionarioResult con ok=true y el resultado calculado si tuvo éxito.
     */
    CuestionarioResult finalizar(int idCuestionario, int idUsuario);

    /**
     * @brief Finaliza el cuestionario cuando el alumno responde la última pregunta
     * (en_progreso → finalizado).
     * @note A diferencia de finalizar(), no requiere verificación de dueño (el
     *       alumno no es dueño del cuestionario), pero sí exige que estén
     *       todas las preguntas respondidas.
     * @note Se llama dentro de la transacción de RespuestaService::responder(),
     *       junto con la respuesta a la última pregunta. Por eso no detiene el
     *       cronómetro: lo hace quien la llama, después del COMMIT, con
     *       detenerCronometro().
     * @param idCuestionario Id del cuestionario a finalizar.
     * @return CuestionarioResult con ok=true y el resultado calculado si tuvo éxito.
     */
    CuestionarioResult finalizarComoAlumno(int idCuestionario);

    /**
     * @brief Lista una página de los cuestionarios de un profesor.
     * @param idUsuario Id del profesor.
     * @param buffer    Array destino.
     * @param maxSize   Capacidad del array (= tamaño de página).
     * @param offset    Cantidad de cuestionarios a saltear (páginas anteriores).
     * @return Cantidad de cuestionarios cargados en el buffer.
     */
    int listarProfesor(int idUsuario, CuestionarioResumenProfesor * buffer, int maxSize, int offset);

    /**
     * @brief Lista una página de todos los cuestionarios (vista del tutor).
     * @param buffer  Array destino.
     * @param maxSize Capacidad del array (= tamaño de página).
     * @param offset  Cantidad de cuestionarios a saltear (páginas anteriores).
     * @return Cantidad de cuestionarios cargados en el buffer.
     */
    int listarTutor(CuestionarioResumenTutor * buffer, int maxSize, int offset);

    /**
     * @brief Total de cuestionarios de un profesor (para la paginación).
     * @param idUsuario Id del profesor.
     * @return Cantidad de cuestionarios.
     */
    int contarProfesor(int idUsuario);

    /**
     * @brief Total de cuestionarios del sistema (para la paginación del tutor).
     * @return Cantidad de cuestionarios.
     */
    int contarTutor();

    /**
     * @brief Obtiene el detalle de cada pregunta con lo que respondió el alumno.
     * @param idCuestionario Id del cuestionario.
     * @param buffer         Array destino.
     * @param maxSize        Capacidad del array.
     * @return Cantidad de preguntas cargadas, o -1 si el cuestionario no existe.
     */
    int obtenerRevision(int idCuestionario, PreguntaRevision * buffer, int maxSize);

    /**
     * @brief Suma al cronómetro el tiempo real transcurrido desde el heartbeat anterior.
     *
     * La llama RespuestaService::obtenerEstado() cada vez que el alumno pide
     * el estado (el front lo hace cada 2 s solo en la pantalla de examen).
     * Si entre dos llamadas pasaron más de HEARTBEAT_MAX_GAP_MS, ese
     * intervalo no se suma: así no cuenta el tiempo de una desconexión o
     * fuera de la pantalla de examen.
     *
     * Cada GUARDADO_PERIODICO_MS guarda el tiempo acumulado en la BD, para
     * que un reinicio de la placa pierda como mucho ese intervalo.
     *
     * @param idCuestionario Cuestionario en progreso. Si el cronómetro no lo
     *                       está midiendo (por ejemplo, después de un
     *                       reinicio), lo retoma desde el tiempo guardado en
     *                       la BD y empieza a contar desde ahora.
     */
    void procesarHeartbeatCronometro(int idCuestionario);

    /**
     * @brief Detiene el cronómetro en RAM (no escribe en la BD).
     * @note Llamar después de confirmar en la BD el cambio que lo detiene
     *       (pausa o finalización).
     * @param idCuestionario Cuestionario que se deja de medir. Si no es el
     *                       del cronómetro, no hace nada.
     */
    void detenerCronometro(int idCuestionario);

private:
    /// @brief Cuestionario que mide el cronómetro (0 si no hay ninguno).
    int _idCuestionarioTimer = 0;

    /// @brief Tiempo acumulado en segundos (derivado de _tiempoAcumuladoMs).
    int _tiempoAcumuladoSeg = 0;

    /// @brief Tiempo acumulado en ms. Es la fuente de verdad; _tiempoAcumuladoSeg
    /// se deriva de éste en cada heartbeat porque la BD trabaja en segundos.
    unsigned long _tiempoAcumuladoMs = 0;

    /// @brief millis() del último heartbeat contabilizado.
    unsigned long _ultimoHeartbeatMs = 0;

    /// @brief millis() de la última vez que se guardó el tiempo en la BD.
    unsigned long _ultimoGuardadoMs = 0;

    /// @brief Si entre dos heartbeats pasa más que esto, ese intervalo no se
    /// suma. El front pide /estado cada 2 s: 6 s tolera hasta dos pedidos
    /// perdidos o demorados (SD lenta) sin perder tiempo.
    static const unsigned long HEARTBEAT_MAX_GAP_MS = 6000;

    /// @brief Cada cuánto se guarda el tiempo acumulado en la BD mientras el
    /// examen está en progreso (una escritura en la SD por minuto).
    static const unsigned long GUARDADO_PERIODICO_MS = 60000;

    /// @brief Spinlock que protege el estado del cronómetro frente a accesos
    /// desde distintas tareas de FreeRTOS. Mientras está tomado se
    /// deshabilitan las interrupciones del core, por eso adentro no se
    /// accede a la BD.
    portMUX_TYPE _cronometroMux = portMUX_INITIALIZER_UNLOCKED;

    /**
     * @brief Arranca el cronómetro desde 0 para un cuestionario.
     * @param idCuestionario Cuestionario que se empieza a medir.
     */
    void _iniciarCronometro(int idCuestionario);

    /**
     * @brief Guarda en la BD el tiempo acumulado del cronómetro, sin detenerlo.
     * @param idCuestionario Cuestionario que se pausa. Si no es el que se
     *                       está midiendo, no escribe nada.
     * @return DbResult con ok=true si se guardó o si no había nada que guardar.
     */
    DbResult _guardarTiempoCronometro(int idCuestionario);

    /**
     * @brief Retoma el cronómetro desde el tiempo guardado en la BD.
     * @param idCuestionario Cuestionario que se reanuda.
     */
    void _reanudarCronometro(int idCuestionario);

    /**
     * @brief Devuelve el tiempo de examen de un cuestionario.
     * @param idCuestionario Cuestionario consultado.
     * @param tiempoGuardado Tiempo guardado en la BD al pausarlo; se usa si
     *                       el cronómetro no lo está midiendo (por ejemplo,
     *                       al finalizar un examen pausado).
     * @return Segundos acumulados por el cronómetro, o tiempoGuardado si el
     *         cronómetro no mide ese cuestionario.
     */
    int _tiempoTranscurridoSeg(int idCuestionario, int tiempoGuardado);

    CuestionarioService() {
    }
    CuestionarioService(const CuestionarioService &) = delete;
    CuestionarioService & operator=(const CuestionarioService &) = delete;

    /**
     * @brief Calcula el puntaje obtenido por el alumno.
     *
     * Suma puntajeCorrecta por cada acierto y resta puntajeIncorrecta por
     * cada error. Las preguntas sin responder no suman ni restan.
     *
     * @param idCuestionario Id del cuestionario.
     * @return Puntaje obtenido (puede ser negativo).
     */
    float _calcularPuntaje(int idCuestionario);

    /**
     * @brief Suma el puntajeCorrecta de TODAS las preguntas del cuestionario,
     * sin importar si están respondidas (es el máximo teórico posible).
     * @param idCuestionario Id del cuestionario.
     * @return Puntaje máximo posible.
     */
    float _calcularPuntajeMaximo(int idCuestionario);

    /**
     * @brief Lógica común de finalización, usada por finalizar() y
     * finalizarComoAlumno(). Calcula puntaje, puntaje máximo y tiempo,
     * persiste el resultado en BD y llena result.resultado. No detiene el
     * cronómetro (ver detenerCronometro()).
     * @note Asume que ya se validó estado y permisos antes de llamarla.
     * @param idCuestionario Id del cuestionario a finalizar.
     * @param result         Struct a llenar (ok/mensaje/resultado).
     */
    void _finalizarInterno(int idCuestionario, CuestionarioResult & result);

    /**
     * @brief Indica si un usuario es el dueño de un cuestionario.
     * @param idCuestionario Id del cuestionario.
     * @param idUsuario      Id del profesor.
     * @return true si el cuestionario existe y es de ese usuario.
     */
    bool _esDuenio(int idCuestionario, int idUsuario);

    /**
     * @brief Valida las preguntas de un cuestionario antes de crearlo o editarlo:
     * al menos una pregunta, pregunta no vacía, entre 2 y 4 opciones y
     * exactamente una correcta.
     * @param preguntas Preguntas a validar.
     * @param cant      Cantidad de preguntas.
     * @param error     Se llena con el motivo si alguna pregunta es inválida.
     * @return true si todas las preguntas son válidas.
     */
    bool _validarPreguntas(const PreguntaCompleta * preguntas, int cant, String & error);

    /**
     * @brief Valida los datos del cuestionario antes de crearlo o editarlo:
     * título no vacío y que el usuario no tenga otro cuestionario con el
     * mismo título. Si c.idCuestionario != 0 (edición), se excluye a sí mismo.
     * @param c     Cuestionario a validar.
     * @param error Se llena con el motivo si es inválido.
     * @return true si es válido.
     */
    bool _validarCuestionario(const Cuestionario & c, String & error);

    /**
     * @brief Inserta las preguntas, sus opciones y la opción correcta de cada una.
     * @note No abre ni cierra transacción: la maneja quien la llama
     *       (crear / editar), que debe hacer rollback si devuelve false.
     * @param idCuestionario Cuestionario al que pertenecen las preguntas.
     * @param preguntas      Preguntas a insertar (ya validadas).
     * @param cant           Cantidad de preguntas.
     * @param error          Se llena con el motivo si falla algún INSERT/UPDATE.
     * @return true si se insertó todo.
     */
    bool _insertarPreguntas(int idCuestionario, const PreguntaCompleta * preguntas, int cant, String & error);
};

#endif // CUESTIONARIO_SERVICE_H
