#ifndef MODELS_H
#define MODELS_H

#include <Arduino.h>

/**
 * @file Models.h
 * @brief Structs que representan las tablas de la BD y DTOs para consultas con JOIN.
 */

// ===========================================================================
// ENTIDADES BASE
// ===========================================================================

/**
 * @brief Representa un registro de la tabla usuarios.
 * @note Ni el alumno ni los invitados tienen registro en esta tabla.
 */
struct Usuario {
    int idUsuario = 0;
    String usuario = ""; ///< Nombre de login (único).
    String nombre = "";
    String apellido = "";
    String rol = "";        ///< "profesor" | "tutor"
    String referencia = ""; ///< Materia (profesor) o vínculo con el alumno (tutor).
    String contacto = "";
    String hashPassword = ""; ///< SHA256 en hex de (password + salt).
    String salt = "";         ///< 32 caracteres hexadecimales aleatorios.
};

/**
 * @brief Representa un registro de la tabla cuestionarios.
 *
 * tiempoSegundos se guarda en la BD cuando el examen se pausa (a mano, por
 * desconexión del alumno o porque se inició otro), una vez por minuto
 * mientras está en progreso y cuando finaliza. Entre esas escrituras, el
 * tiempo corre en el cronómetro en RAM de CuestionarioService.
 * puntajeObtenido queda en 0 hasta que el examen finaliza.
 */
struct Cuestionario {
    int idCuestionario = 0;
    int idUsuario = 0; ///< Profesor dueño del cuestionario.
    String titulo = "";
    float puntajeParaAprobar = 0.0f;
    String estado = "pendiente"; ///< "pendiente" | "en_progreso" | "pausado" | "finalizado"
    float puntajeObtenido = 0.0f;
    String fechaInicio = ""; ///< Fecha ISO que manda el front al iniciar.
    int tiempoSegundos = 0;
};

/**
 * @brief Representa un registro de la tabla opciones.
 *
 * @note esCorrecta no existe en la BD. Se usa al crear o editar una pregunta
 * para saber qué opción pasar a asignarOpcionCorrecta(), y al leer un
 * cuestionario completo se calcula comparando con idOpcionCorrecta.
 */
struct Opcion {
    int idOpcion = 0;
    int idPregunta = 0;
    String opcion = "";
    bool esCorrecta = false;
};

/**
 * @brief Representa un registro de la tabla preguntas.
 *
 * idOpcionCorrecta: 0 (NULL en la BD) hasta que se asigna tras insertar las opciones.
 * idOpcionElegida:  0 (NULL en la BD) hasta que el alumno responde.
 * puntajeCorrecta se suma si acierta; puntajeIncorrecta se resta si se equivoca.
 */
struct Pregunta {
    int idPregunta = 0;
    int idCuestionario = 0;
    int idOpcionCorrecta = 0;
    int idOpcionElegida = 0;
    String pregunta = "";
    float puntajeCorrecta = 0.0f;
    float puntajeIncorrecta = 0.0f;
};

/**
 * @brief Resumen de un usuario para los listados del panel.
 * @note Profesores y tutores pueden ver tanto la lista de profesores (con su
 * materia y contacto) como la de tutores (con su vínculo y contacto).
 */
struct UsuarioResumen {
    int idUsuario = 0;
    String nombre = "";
    String apellido = "";
    String referencia = "";
    String contacto = "";
};

// ===========================================================================
// DTOs DE CREACIÓN
// ===========================================================================

/**
 * @brief DTO que agrupa una pregunta con sus opciones.
 * @note Se usa al crear y editar un cuestionario, y al devolverlo completo
 * para editarlo. cantOpciones indica cuántas opciones están cargadas (2 a 4).
 */
struct PreguntaCompleta {
    Pregunta pregunta;
    Opcion opciones[4];
    int cantOpciones = 0;
};

// ===========================================================================
// DTOs DE LECTURA PARA EL PANEL DEL PROFESOR
// ===========================================================================

/**
 * @brief Resumen de un cuestionario para el panel del profesor.
 *
 * El profesor solo ve sus propios cuestionarios y ya conoce su materia,
 * por eso no se incluye información del profesor ni materia.
 */
struct CuestionarioResumenProfesor {
    int idCuestionario = 0;
    String titulo = "";
    String estado = "";
    float puntajeObtenido = 0.0f;
    float puntajeParaAprobar = 0.0f;
    bool aprobado = false; ///< true si está finalizado y puntajeObtenido >= puntajeParaAprobar.
    int cantPreguntas = 0;
    String fechaInicio = "";
};

// ===========================================================================
// DTOs DE LECTURA PARA EL PANEL DEL TUTOR
// ===========================================================================

/**
 * @brief Resumen de un cuestionario para el panel del tutor.
 *
 * El tutor ve todos los cuestionarios de todos los profesores.
 * Tiene los mismos campos que el resumen del profesor, más la materia
 * para identificar a qué asignatura pertenece.
 */
struct CuestionarioResumenTutor : CuestionarioResumenProfesor {
    String materia = ""; ///< referencia del profesor dueño del cuestionario.
};

// ===========================================================================
// DTOs DE REVISIÓN DE CUESTIONARIO FINALIZADO
// Usados por profesor y tutor para ver el detalle del resultado.
// ===========================================================================

/**
 * @brief Detalle de una pregunta respondida para la vista de revisión.
 *
 * Muestra el texto de la opción elegida y la correcta para que
 * el profesor o tutor pueda ver qué respondió el alumno.
 */
struct PreguntaRevision {
    int idPregunta = 0;
    String textoPregunta = "";
    float puntajeCorrecta = 0.0f;
    float puntajeIncorrecta = 0.0f;
    String opcionElegida = ""; ///< "" si no se respondió.
    String opcionCorrecta = "";
    bool fueCorrecto = false; ///< false también si no se respondió.
};

// ===========================================================================
// DTO DE RESULTADO DE FINALIZACIÓN
// ===========================================================================

/**
 * @brief Datos de puntaje y tiempo tras finalizar un cuestionario.
 */
struct ResultadoFinalizacion {
    float puntajeObtenido = 0.0f;
    float puntajeMaximo = 0.0f; ///< Suma de puntajeCorrecta de todas las preguntas.
    float puntajeParaAprobar = 0.0f;
    int tiempoSegundos = 0;
};

// ===========================================================================
// DTOs PARA EL PANEL DEL ALUMNO
// ===========================================================================

/**
 * @brief Opción simplificada para mostrar al alumno.
 * @note No incluye si es correcta o no.
 */
struct OpcionAlumno {
    int idOpcion = 0;
    String opcion = "";
};

/**
 * @brief Pregunta activa para mostrar al alumno durante el examen.
 *
 * numeroPregunta es la cantidad de preguntas ya respondidas + 1, y
 * totalPreguntas la cantidad total de preguntas del cuestionario.
 * En modo invitado idPregunta es 0 y los idOpcion son los índices 0 a 3.
 */
struct PreguntaAlumno {
    int idPregunta = 0;
    String textoPregunta = "";
    int numeroPregunta = 0;
    int totalPreguntas = 0;
    OpcionAlumno opciones[4];
    int cantOpciones = 0;
};

// ===========================================================================
// RESULTADO DE OPERACIONES DE BD
// Lo devuelven las funciones de repository que escriben (INSERT, UPDATE,
// DELETE). Las de lectura devuelven el struct, la cantidad o el bool.
// ===========================================================================

/**
 * @brief Resultado estándar de una escritura sobre la base de datos.
 */
struct DbResult {
    bool ok = false;     ///< true si la operación fue exitosa.
    String mensaje = ""; ///< Descripción del error en caso de fallo.
    int id = 0;          ///< ID generado por un INSERT (last_insert_rowid). 0 si no aplica.
};

#endif // MODELS_H