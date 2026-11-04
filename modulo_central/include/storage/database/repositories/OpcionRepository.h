#ifndef OPCION_REPOSITORY_H
#define OPCION_REPOSITORY_H

#include <Arduino.h>
#include "../../models/Models.h"
#include "../DatabaseManager.h"

/**
 * @file OpcionRepository.h
 * @brief Acceso a datos de la tabla opciones.
 *
 * Cada pregunta puede tener entre 2 y 4 opciones.
 * El campo esCorrecta no existe en la BD: se determina comparando
 * idOpcion con el idOpcionCorrecta de la pregunta correspondiente.
 */
class OpcionRepository {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al OpcionRepository.
     */
    static OpcionRepository & getInstance() {
        static OpcionRepository instance;
        return instance;
    }

    /**
     * @brief Inserta una opción para una pregunta.
     * @param idPregunta  Id de la pregunta a la que pertenece.
     * @param textoOpcion Texto de la opción.
     * @return DbResult con ok=true e id generado si tuvo éxito.
     */
    DbResult crear(int idPregunta, const String & textoOpcion);

    /**
     * @brief Busca una opción por su id.
     * @param idOpcion Id a buscar.
     * @return Opcion encontrada, o struct con idOpcion=0 si no existe.
     */
    Opcion buscarPorId(int idOpcion);

    /**
     * @brief Lista todas las opciones de una pregunta en formato para el alumno,
     * ordenadas por id.
     * @details No incluye cuál es la correcta.
     * @param idPregunta Id de la pregunta.
     * @param buffer     Array destino.
     * @param maxSize    Capacidad máxima del array.
     * @return Cantidad de opciones encontradas.
     */
    int listarParaAlumno(int idPregunta, OpcionAlumno * buffer, int maxSize);

    /**
     * @brief Lista todas las opciones de una pregunta con todos sus datos,
     * ordenadas por id.
     * @note esCorrecta queda en false: la determina quien llama comparando
     *       con el idOpcionCorrecta de la pregunta.
     * @param idPregunta Id de la pregunta.
     * @param buffer     Array destino.
     * @param maxSize    Capacidad máxima del array.
     * @return Cantidad de opciones encontradas.
     */
    int listarPorPregunta(int idPregunta, Opcion * buffer, int maxSize);

    /**
     * @brief Actualiza el texto de una opción.
     * @param o Opcion con el texto nuevo (idOpcion debe ser válido).
     * @return DbResult con ok=true si tuvo éxito.
     */
    DbResult actualizar(const Opcion & o);

    /**
     * @brief Elimina una opción por su id.
     * @param idOpcion Id de la opción a eliminar.
     * @return DbResult con ok=true si tuvo éxito.
     */
    DbResult eliminar(int idOpcion);

private:
    OpcionRepository() {
    }
    OpcionRepository(const OpcionRepository &) = delete;
    OpcionRepository & operator=(const OpcionRepository &) = delete;

    /**
     * @brief Convierte la fila actual de un stmt en un struct Opcion.
     * @param stmt Statement con una fila lista para leer
     *             (columnas: idOpcion, idPregunta, opcion).
     * @return Opcion con los datos de la fila.
     */
    Opcion _filaAOpcion(sqlite3_stmt * stmt);
};

#endif // OPCION_REPOSITORY_H
