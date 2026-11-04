#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <Arduino.h>
#include <sqlite3.h>

/**
 * @file DatabaseManager.h
 * @brief Gestión de la conexión con la base de datos SQLite.
 *
 * Singleton encargado de abrir la base SQLite, configurar PRAGMAs
 * y proveer la conexión activa a los repositories.
 *
 * Los repositories obtienen la conexión mediante getDB().
 */
class DatabaseManager {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al DatabaseManager.
     */
    static DatabaseManager & getInstance() {
        static DatabaseManager instance;
        return instance;
    }

    /**
     * @brief Abre la BD, configura los PRAGMAs y verifica los índices.
     *
     * Si falla algún PRAGMA cierra la conexión. La falta de índices solo
     * se informa por Serial (ver _verificarIndices()).
     *
     * @param rutaArchivo Ruta VFS del archivo SQLite (ej: "/sd/levi.db").
     * @return true si la conexión se estableció correctamente.
     */
    bool begin(const char * rutaArchivo);

    /**
     * @brief Cierra la conexión con la base de datos.
     */
    void end();

    /**
     * @brief Devuelve el puntero a la conexión activa.
     *
     * @return Puntero sqlite3*, usado por los repositories.
     */
    sqlite3 * getDB() {
        return _db;
    }

    /**
     * @brief Indica si la BD está abierta y lista para usarse.
     *
     * @return true si la conexión está activa.
     */
    bool isReady() {
        return _db != nullptr;
    }

    /**
     * @brief Ejecuta SQL arbitrario (sin filas de resultado) desde fuera del manager.
     * @param sql Sentencia SQL a ejecutar. También se usa como descripción en el log de error.
     * @return true si fue exitoso.
     */
    bool ejecutar(const char * sql) {
        return _ejecutar(sql, sql);
    }

    // -----------------------------------------------------------------------
    // Transacciones SQLite
    // -----------------------------------------------------------------------

    /**
     * @brief Inicia una transacción SQLite.
     *
     * Agrupa múltiples INSERT/UPDATE/DELETE en una única operación
     * de escritura sobre la memoria persistente.
     *
     * @return true si la transacción comenzó correctamente.
     */
    bool beginTransaction();

    /**
     * @brief Confirma una transacción activa.
     *
     * @return true si el COMMIT fue exitoso.
     */
    bool commit();

    /**
     * @brief Cancela una transacción activa.
     *
     * @return true si el ROLLBACK fue exitoso.
     */
    bool rollback();

private:
    DatabaseManager() : _db(nullptr) {
    }
    ~DatabaseManager() {
        end();
    }

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager & operator=(const DatabaseManager &) = delete;

    /// @brief Conexión activa (nullptr si la BD está cerrada).
    sqlite3 * _db;

    /**
     * @brief Configura los PRAGMAs de SQLite.
     *
     * Activa:
     * - foreign_keys ON: necesario para los ON DELETE CASCADE.
     * - journal_mode PERSIST: WAL no está disponible en esta librería y
     *   TRUNCATE no es seguro (su xTruncate no hace nada); PERSIST evita
     *   crear/borrar el journal en cada commit sobre FAT.
     * - synchronous FULL: máxima protección ante cortes de luz; cuesta poco
     *   porque cada operación hace un solo commit.
     * - cache_size -512 (512 KB): las páginas van a PSRAM (> 4096 bytes).
     * - temp_store MEMORY: journals de sentencia y temporales en RAM, no en la SD.
     *
     * @note Requiere la librería de lib/Sqlite3Esp32 con el arreglo de
     *       ESP32FileControl; con la original los PRAGMAs se ignoran.
     *
     * @return true si todos los PRAGMAs se aplicaron correctamente.
     */
    bool _configurarPragmas();

    /**
     * @brief Verifica que la base tenga los índices esperados.
     *
     * Los índices se crean en el propio archivo levi.db (no desde el código).
     * Esta función solo avisa por Serial cuáles de los esperados faltan:
     * - Claves foráneas de preguntas y opciones: evitan recorrer tablas
     *   completas en los borrados en cascada y en los ON DELETE SET NULL.
     * - Índice parcial del cuestionario en progreso: lo usan obtenerActivo()
     *   y hayUnoEnProgreso() en cada consulta de estado del alumno.
     *
     * @note Solo informa: si falta alguno la base abre igual.
     */
    void _verificarIndices();

    /**
     * @brief Ejecuta SQL sin retorno de filas con sqlite3_exec().
     *
     * @param sql Sentencia SQL a ejecutar.
     * @param descripcion Texto que identifica la sentencia en el log de error.
     *
     * @return true si la ejecución fue exitosa.
     */
    bool _ejecutar(const char * sql, const char * descripcion);
};

#endif // DATABASE_MANAGER_H