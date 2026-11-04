#include "storage/database/repositories/OpcionRepository.h"

Opcion OpcionRepository::_filaAOpcion(sqlite3_stmt * stmt) {
    Opcion o;
    o.idOpcion = sqlite3_column_int(stmt, 0);
    o.idPregunta = sqlite3_column_int(stmt, 1);
    o.opcion = String((const char *)sqlite3_column_text(stmt, 2));
    o.esCorrecta = false;
    return o;
}

DbResult OpcionRepository::crear(int idPregunta, const String & textoOpcion) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "INSERT INTO opciones (idPregunta, opcion) VALUES (?, ?);", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, idPregunta);
    sqlite3_bind_text(stmt, 2, textoOpcion.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result.ok = true;
        result.id = (int)sqlite3_last_insert_rowid(db);
    } else {
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);
    }

    sqlite3_finalize(stmt);
    return result;
}

Opcion OpcionRepository::buscarPorId(int idOpcion) {
    Opcion o;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT idOpcion, idPregunta, opcion FROM opciones WHERE idOpcion = ?;", -1, &stmt,
                           nullptr) != SQLITE_OK)
        return o;

    sqlite3_bind_int(stmt, 1, idOpcion);

    if (sqlite3_step(stmt) == SQLITE_ROW)
        o = _filaAOpcion(stmt);

    sqlite3_finalize(stmt);
    return o;
}

int OpcionRepository::listarParaAlumno(int idPregunta, OpcionAlumno * buffer, int maxSize) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();
    int count = 0;

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "SELECT idOpcion, opcion FROM opciones WHERE idPregunta = ? ORDER BY idOpcion ASC;", -1,
                           &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idPregunta);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < maxSize) {
        buffer[count].idOpcion = sqlite3_column_int(stmt, 0);
        buffer[count].opcion = String((const char *)sqlite3_column_text(stmt, 1));
        count++;
    }

    sqlite3_finalize(stmt);
    return count;
}

int OpcionRepository::listarPorPregunta(int idPregunta, Opcion * buffer, int maxSize) {
    sqlite3 * db = DatabaseManager::getInstance().getDB();
    int count = 0;

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(
            db, "SELECT idOpcion, idPregunta, opcion FROM opciones WHERE idPregunta = ? ORDER BY idOpcion ASC;", -1,
            &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, idPregunta);

    while (sqlite3_step(stmt) == SQLITE_ROW && count < maxSize) {
        buffer[count++] = _filaAOpcion(stmt);
    }

    sqlite3_finalize(stmt);
    return count;
}

DbResult OpcionRepository::actualizar(const Opcion & o) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "UPDATE opciones SET opcion = ? WHERE idOpcion = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_text(stmt, 1, o.opcion.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, o.idOpcion);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}

DbResult OpcionRepository::eliminar(int idOpcion) {
    DbResult result;
    sqlite3 * db = DatabaseManager::getInstance().getDB();

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(db, "DELETE FROM opciones WHERE idOpcion = ?;", -1, &stmt, nullptr) != SQLITE_OK) {
        result.mensaje = String("prepare error: ") + sqlite3_errmsg(db);
        return result;
    }

    sqlite3_bind_int(stmt, 1, idOpcion);

    result.ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!result.ok)
        result.mensaje = String("step error: ") + sqlite3_errmsg(db);

    sqlite3_finalize(stmt);
    return result;
}
