#include "storage/database/repositories/PreguntaRepository.h"

Pregunta PreguntaRepository::_filaAPregunta(sqlite3_stmt * stmt) {
    Pregunta p;
    p.idPregunta = sqlite3_column_int(stmt, 0);
    p.idCuestionario = sqlite3_column_int(stmt, 1);

    p.idOpcionCorrecta = sqlite3_column_type(stmt, 2) != SQLITE_NULL ? sqlite3_column_int(stmt, 2) : 0;

    p.idOpcionElegida = sqlite3_column_type(stmt, 3) != SQLITE_NULL ? sqlite3_column_int(stmt, 3) : 0;

    p.pregunta = String((const char *)sqlite3_column_text(stmt, 4));
    p.puntajeCorrecta = (float)sqlite3_column_double(stmt, 5);
    p.puntajeIncorrecta = (float)sqlite3_column_double(stmt, 6);
    return p;
}

DbResult PreguntaRepository::crear(const Pregunta & p) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        INSERT INTO preguntas (idCuestionario, pregunta, puntajeCorrecta, puntajeIncorrecta)
        VALUES (?, ?, ?, ?);
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, p.idCuestionario);
    sqlite3_bind_text(stmt, 2, p.pregunta.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, p.puntajeCorrecta);
    sqlite3_bind_double(stmt, 4, p.puntajeIncorrecta);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result.ok = true;
        result.id = (int)sqlite3_last_insert_rowid(db);
    } else {
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

DbResult PreguntaRepository::asignarOpcionCorrecta(int idPregunta, int idOpcionCorrecta) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt = nullptr;

    int rc = sqlite3_prepare_v2(db, "UPDATE preguntas SET idOpcionCorrecta=? WHERE idPregunta=?;", -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        result.mensaje = sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, idOpcionCorrecta);
    sqlite3_bind_int(stmt, 2, idPregunta);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_DONE)
        result.ok = true;
    else
        result.mensaje = sqlite3_errmsg(db);

    sqlite3_finalize(stmt);

    return result;
}

DbResult PreguntaRepository::guardarRespuesta(int idPregunta, int idOpcionElegida) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "UPDATE preguntas SET idOpcionElegida = ? WHERE idPregunta = ?;", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, idOpcionElegida);
    sqlite3_bind_int(stmt, 2, idPregunta);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

Pregunta PreguntaRepository::buscarPorId(int idPregunta) {
    Pregunta p;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        SELECT idPregunta, idCuestionario, idOpcionCorrecta, idOpcionElegida,
               pregunta, puntajeCorrecta, puntajeIncorrecta
        FROM preguntas WHERE idPregunta = ?;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return p;

    sqlite3_bind_int(stmt, 1, idPregunta);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        p = _filaAPregunta(stmt);

    sqlite3_finalize(stmt);
    return p;
}

int PreguntaRepository::listarPorCuestionario(int idCuestionario, Pregunta * buffer, int maxSize) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();
    int count = 0;

    const char * sql = R"(
        SELECT idPregunta, idCuestionario, idOpcionCorrecta, idOpcionElegida,
               pregunta, puntajeCorrecta, puntajeIncorrecta
        FROM preguntas WHERE idCuestionario = ?
        ORDER BY idPregunta ASC;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < maxSize) {
        buffer[count++] = _filaAPregunta(stmt);
    }

    sqlite3_finalize(stmt);
    return count;
}

DbResult PreguntaRepository::actualizar(const Pregunta & p) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        UPDATE preguntas
        SET pregunta = ?, puntajeCorrecta = ?, puntajeIncorrecta = ?
        WHERE idPregunta = ?;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_text(stmt, 1, p.pregunta.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, p.puntajeCorrecta);
    sqlite3_bind_double(stmt, 3, p.puntajeIncorrecta);
    sqlite3_bind_int(stmt, 4, p.idPregunta);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

DbResult PreguntaRepository::eliminarPorCuestionario(int idCuestionario) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "DELETE FROM preguntas WHERE idCuestionario = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, idCuestionario);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

DbResult PreguntaRepository::limpiarRespuestas(int idCuestionario) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "UPDATE preguntas SET idOpcionElegida = NULL WHERE idCuestionario = ?;", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, idCuestionario);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

Pregunta PreguntaRepository::obtenerPrimeraSinResponder(int idCuestionario) {
    Pregunta p;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        SELECT idPregunta, idCuestionario, idOpcionCorrecta, idOpcionElegida,
               pregunta, puntajeCorrecta, puntajeIncorrecta
        FROM preguntas
        WHERE idCuestionario = ? AND idOpcionElegida IS NULL
        ORDER BY idPregunta ASC LIMIT 1;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return p;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        p = _filaAPregunta(stmt);

    sqlite3_finalize(stmt);
    return p;
}

int PreguntaRepository::contarTotal(int idCuestionario) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM preguntas WHERE idCuestionario = ?;", -1, &stmt, nullptr) !=
        SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        total = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return total;
}

int PreguntaRepository::contarRespondidas(int idCuestionario) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db,
                           "SELECT COUNT(*) FROM preguntas WHERE idCuestionario = ? AND idOpcionElegida IS NOT NULL;",
                           -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    int total = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        total = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return total;
}

int PreguntaRepository::listarRevision(int idCuestionario, PreguntaRevision * buffer, int maxSize) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();
    int count = 0;

    const char * sql = R"(
        SELECT p.idPregunta,
               p.pregunta,
               p.puntajeCorrecta,
               p.puntajeIncorrecta,
               oe.opcion  AS opcionElegida,
               oc.opcion  AS opcionCorrecta,
               p.idOpcionElegida = p.idOpcionCorrecta AS fueCorrecto
        FROM preguntas p
        LEFT JOIN opciones oe ON oe.idOpcion = p.idOpcionElegida
        LEFT JOIN opciones oc ON oc.idOpcion = p.idOpcionCorrecta
        WHERE p.idCuestionario = ?
        ORDER BY p.idPregunta ASC;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < maxSize) {
        PreguntaRevision & r = buffer[count++];
        r.idPregunta = sqlite3_column_int(stmt, 0);
        r.textoPregunta = String((const char *)sqlite3_column_text(stmt, 1));
        r.puntajeCorrecta = (float)sqlite3_column_double(stmt, 2);
        r.puntajeIncorrecta = (float)sqlite3_column_double(stmt, 3);

        const char * elegida = (const char *)sqlite3_column_text(stmt, 4);
        r.opcionElegida = elegida ? String(elegida) : "";

        const char * correcta = (const char *)sqlite3_column_text(stmt, 5);
        r.opcionCorrecta = correcta ? String(correcta) : "";
        r.fueCorrecto = sqlite3_column_int(stmt, 6) == 1;
    }

    sqlite3_finalize(stmt);
    return count;
}

bool PreguntaRepository::obtenerSiguienteParaAlumno(int idCuestionario, PreguntaAlumno & result) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        SELECT idPregunta, pregunta
        FROM preguntas
        WHERE idCuestionario = ? AND idOpcionElegida IS NULL
        ORDER BY idPregunta ASC LIMIT 1;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    bool hayPregunta = (sqlite3_step(stmt) == SQLITE_ROW);

    if (hayPregunta) {
        result.idPregunta = sqlite3_column_int(stmt, 0);
        result.textoPregunta = String((const char *)sqlite3_column_text(stmt, 1));
        result.totalPreguntas = contarTotal(idCuestionario);
        result.numeroPregunta = contarRespondidas(idCuestionario) + 1;
    }

    sqlite3_finalize(stmt);
    return hayPregunta;
}
