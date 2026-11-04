#include "storage/database/DatabaseManager.h"

bool DatabaseManager::begin(const char * rutaArchivo) {
    int rc = sqlite3_open(rutaArchivo, &_db);

    if (rc != SQLITE_OK) {
        Serial.printf("[DB] Error al abrir BD: %s\n", sqlite3_errmsg(_db));
        _db = nullptr;
        return false;
    }

    if (!_configurarPragmas()) {
        end();
        return false;
    }

    _verificarIndices();

    Serial.printf("[DB] Base de datos lista: %s\n", rutaArchivo);
    return true;
}

void DatabaseManager::end() {
    if (_db) {
        sqlite3_close(_db);
        _db = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Configuración SQLite
// ---------------------------------------------------------------------------

bool DatabaseManager::_configurarPragmas() {
    if (!_ejecutar("PRAGMA foreign_keys = ON;", "foreign_keys"))
        return false;

    if (!_ejecutar("PRAGMA journal_mode = PERSIST;", "journal_mode"))
        return false;

    if (!_ejecutar("PRAGMA synchronous = FULL;", "synchronous"))
        return false;

    if (!_ejecutar("PRAGMA cache_size = -512;", "cache_size"))
        return false;

    if (!_ejecutar("PRAGMA temp_store = MEMORY;", "temp_store"))
        return false;

    return true;
}

void DatabaseManager::_verificarIndices() {
    static const char * esperados[] = {
        "idx_opciones_idPregunta",        "idx_preguntas_idCuestionario",
        "idx_preguntas_idOpcionCorrecta", "idx_preguntas_idOpcionElegida",
        "idx_cuestionarios_en_progreso", // parcial: WHERE estado = 'en_progreso'
    };
    const int cantEsperados = sizeof(esperados) / sizeof(esperados[0]);

    sqlite3_stmt * stmt;
    if (sqlite3_prepare_v2(_db, "SELECT 1 FROM sqlite_master WHERE type = 'index' AND name = ?;", -1, &stmt, nullptr) !=
        SQLITE_OK)
        return;

    for (int i = 0; i < cantEsperados; i++) {
        sqlite3_bind_text(stmt, 1, esperados[i], -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_ROW)
            Serial.printf("[DB] ATENCION: falta el índice %s.\n", esperados[i]);

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }
    sqlite3_finalize(stmt);
}

// ---------------------------------------------------------------------------
// Ejecución simple de SQL
// ---------------------------------------------------------------------------

bool DatabaseManager::_ejecutar(const char * sql, const char * descripcion) {
    char * errMsg = nullptr;

    int rc = sqlite3_exec(_db, sql, nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        Serial.printf("[DB] Error en '%s': %s\n", descripcion, errMsg ? errMsg : "desconocido");

        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Transacciones
// ---------------------------------------------------------------------------

bool DatabaseManager::beginTransaction() {
    return _ejecutar("BEGIN TRANSACTION;", "begin_transaction");
}

bool DatabaseManager::commit() {
    return _ejecutar("COMMIT;", "commit");
}

bool DatabaseManager::rollback() {
    char * errMsg = nullptr;

    int rc = sqlite3_exec(_db, "ROLLBACK;", nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}