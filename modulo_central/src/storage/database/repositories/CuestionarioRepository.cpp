#include "storage/database/repositories/CuestionarioRepository.h"

Cuestionario CuestionarioRepository::_filaACuestionario(sqlite3_stmt * stmt) {
    Cuestionario c;
    c.idCuestionario = sqlite3_column_int(stmt, 0);
    c.idUsuario = sqlite3_column_int(stmt, 1);
    c.titulo = String((const char *)sqlite3_column_text(stmt, 2));
    c.puntajeParaAprobar = (float)sqlite3_column_double(stmt, 3);
    c.estado = String((const char *)sqlite3_column_text(stmt, 4));

    c.puntajeObtenido = sqlite3_column_type(stmt, 5) != SQLITE_NULL ? (float)sqlite3_column_double(stmt, 5) : 0.0f;

    const char * fecha = (const char *)sqlite3_column_text(stmt, 6);
    c.fechaInicio = fecha ? String(fecha) : "";

    c.tiempoSegundos = sqlite3_column_type(stmt, 7) != SQLITE_NULL ? sqlite3_column_int(stmt, 7) : 0;
    return c;
}

DbResult CuestionarioRepository::crear(const Cuestionario & c) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        INSERT INTO cuestionarios (idUsuario, titulo, puntajeParaAprobar, estado)
        VALUES (?, ?, ?, 'pendiente');
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, c.idUsuario);
    sqlite3_bind_text(stmt, 2, c.titulo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, c.puntajeParaAprobar);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result.ok = true;
        result.id = (int)sqlite3_last_insert_rowid(db);
    } else {
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

Cuestionario CuestionarioRepository::buscarPorId(int idCuestionario) {
    Cuestionario c;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        SELECT idCuestionario, idUsuario, titulo, puntajeParaAprobar,
               estado, puntajeObtenido, fechaInicio, tiempoSegundos
        FROM cuestionarios WHERE idCuestionario = ?;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return c;

    sqlite3_bind_int(stmt, 1, idCuestionario);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        c = _filaACuestionario(stmt);

    sqlite3_finalize(stmt);
    return c;
}

Cuestionario CuestionarioRepository::obtenerActivo() {
    Cuestionario c;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        SELECT idCuestionario, idUsuario, titulo, puntajeParaAprobar,
               estado, puntajeObtenido, fechaInicio, tiempoSegundos
        FROM cuestionarios WHERE estado = 'en_progreso' LIMIT 1;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return c;

    if (sqlite3_step(stmt) == SQLITE_ROW)
        c = _filaACuestionario(stmt);

    sqlite3_finalize(stmt);
    return c;
}

DbResult CuestionarioRepository::actualizar(const Cuestionario & c) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        UPDATE cuestionarios SET titulo = ?, puntajeParaAprobar = ?
        WHERE idCuestionario = ?;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_text(stmt, 1, c.titulo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, c.puntajeParaAprobar);
    sqlite3_bind_int(stmt, 3, c.idCuestionario);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

DbResult CuestionarioRepository::cambiarEstado(int idCuestionario, const String & nuevoEstado, const String & fecha) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = (fecha.length() > 0)
                           ? "UPDATE cuestionarios SET estado = ?, fechaInicio = ? WHERE idCuestionario = ?;"
                           : "UPDATE cuestionarios SET estado = ? WHERE idCuestionario = ?;";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_text(stmt, 1, nuevoEstado.c_str(), -1, SQLITE_TRANSIENT);
    if (fecha.length() > 0) {
        sqlite3_bind_text(stmt, 2, fecha.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, idCuestionario);
    } else {
        sqlite3_bind_int(stmt, 2, idCuestionario);
    }

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);

    return result;
}

DbResult CuestionarioRepository::guardarResultado(int idCuestionario, float puntajeObtenido, int tiempoSegundos) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    const char * sql = R"(
        UPDATE cuestionarios
        SET puntajeObtenido = ?,  tiempoSegundos = ?,
            estado = 'finalizado'
        WHERE idCuestionario = ?;
    )";

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_double(stmt, 1, puntajeObtenido);
    sqlite3_bind_int(stmt, 2, tiempoSegundos);
    sqlite3_bind_int(stmt, 3, idCuestionario);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

DbResult CuestionarioRepository::actualizarTiempoParcial(int idCuestionario, int tiempoSegundos) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "UPDATE cuestionarios SET tiempoSegundos = ? WHERE idCuestionario = ?;", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, tiempoSegundos);
    sqlite3_bind_int(stmt, 2, idCuestionario);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

DbResult CuestionarioRepository::eliminar(int idCuestionario) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "DELETE FROM cuestionarios WHERE idCuestionario = ?;", -1, &stmt, nullptr) !=
        SQLITE_OK) {
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

bool CuestionarioRepository::existeTitulo(int idUsuario, const String & titulo) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM cuestionarios WHERE idUsuario = ? AND titulo = ? LIMIT 1;", -1, &stmt,
                           nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, idUsuario);
    sqlite3_bind_text(stmt, 2, titulo.c_str(), -1, SQLITE_TRANSIENT);

    bool existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

bool CuestionarioRepository::existeTituloExcluyendo(int idUsuario, const String & titulo, int excluirId) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(
            db, "SELECT 1 FROM cuestionarios WHERE idUsuario = ? AND titulo = ? AND idCuestionario != ? LIMIT 1;", -1,
            &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, idUsuario);
    sqlite3_bind_text(stmt, 2, titulo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, excluirId);

    bool existe = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return existe;
}

bool CuestionarioRepository::hayUnoEnProgreso() {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM cuestionarios WHERE estado = 'en_progreso' LIMIT 1;", -1, &stmt,
                           nullptr) != SQLITE_OK)
        return false;

    bool hay = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return hay;
}

/**
 * @brief Carga en un resumen las columnas comunes de los listados (0..6).
 *
 * Columnas, en este orden: idCuestionario, titulo, estado, puntajeObtenido,
 * puntajeParaAprobar, fechaInicio, cantPreguntas. También calcula aprobado.
 *
 * @param stmt Statement con una fila lista para leer.
 * @param r    Resumen a completar (sirve para profesor y tutor).
 */
static void _filaAResumen(sqlite3_stmt * stmt, CuestionarioResumenProfesor & r) {
    r.idCuestionario = sqlite3_column_int(stmt, 0);
    r.titulo = String((const char *)sqlite3_column_text(stmt, 1));
    r.estado = String((const char *)sqlite3_column_text(stmt, 2));
    r.puntajeObtenido = sqlite3_column_type(stmt, 3) != SQLITE_NULL ? (float)sqlite3_column_double(stmt, 3) : 0.0f;
    r.puntajeParaAprobar = (float)sqlite3_column_double(stmt, 4);
    r.aprobado = (r.puntajeObtenido >= r.puntajeParaAprobar) && r.estado == "finalizado";

    const char * fecha = (const char *)sqlite3_column_text(stmt, 5);
    r.fechaInicio = fecha ? String(fecha) : "";

    r.cantPreguntas = sqlite3_column_int(stmt, 6);
}

int CuestionarioRepository::listarResumenProfesor(int idUsuario, CuestionarioResumenProfesor * buffer, int maxSize,
                                                  int offset) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();
    int count = 0;

    const char * sql = R"(
        SELECT c.idCuestionario, c.titulo, c.estado, c.puntajeObtenido,
               c.puntajeParaAprobar, c.fechaInicio,
               (SELECT COUNT(*) FROM preguntas p WHERE p.idCuestionario = c.idCuestionario)
        FROM cuestionarios c
        WHERE c.idUsuario = ?
        ORDER BY c.idCuestionario DESC
        LIMIT ? OFFSET ?;
    )";

    sqlite3_stmt * stmt;
    int rcPrep = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rcPrep != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idUsuario);
    sqlite3_bind_int(stmt, 2, maxSize);
    sqlite3_bind_int(stmt, 3, offset);

    int rcStep;
    while ((rcStep = sqlite3_step(stmt)) == SQLITE_ROW && count < maxSize) {
        _filaAResumen(stmt, buffer[count++]);
    }

    sqlite3_finalize(stmt);
    return count;
}

int CuestionarioRepository::listarResumenTutor(CuestionarioResumenTutor * buffer, int maxSize, int offset) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();
    int count = 0;

    const char * sql = R"(
        SELECT c.idCuestionario, c.titulo, c.estado, c.puntajeObtenido,
               c.puntajeParaAprobar, c.fechaInicio,
               (SELECT COUNT(*) FROM preguntas p WHERE p.idCuestionario = c.idCuestionario),
               u.referencia
        FROM cuestionarios c
        JOIN usuarios u ON u.idUsuario = c.idUsuario
        ORDER BY c.idCuestionario DESC
        LIMIT ? OFFSET ?;
    )";

    sqlite3_stmt * stmt;
    int rcPrep = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rcPrep != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, maxSize);
    sqlite3_bind_int(stmt, 2, offset);

    int rcStep;
    while ((rcStep = sqlite3_step(stmt)) == SQLITE_ROW && count < maxSize) {
        CuestionarioResumenTutor & r = buffer[count++];
        _filaAResumen(stmt, r);

        const char * materia = (const char *)sqlite3_column_text(stmt, 7);
        r.materia = materia ? String(materia) : "";
    }

    sqlite3_finalize(stmt);
    return count;
}

int CuestionarioRepository::contarPorUsuario(int idUsuario) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM cuestionarios WHERE idUsuario = ?;", -1, &stmt, nullptr) !=
        SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idUsuario);

    int cant = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;

    sqlite3_finalize(stmt);
    return cant;
}

int CuestionarioRepository::contarTodos() {
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM cuestionarios;", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int cant = (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int(stmt, 0) : 0;

    sqlite3_finalize(stmt);
    return cant;
}
