#ifndef USUARIO_REPOSITORY_H
#define USUARIO_REPOSITORY_H

#include <Arduino.h>
#include "../../models/Models.h"
#include "../DatabaseManager.h"

/**
 * @file UsuarioRepository.h
 * @brief Acceso a datos de la tabla usuarios.
 *
 * Responsabilidades:
 *   - CRUD sobre la tabla usuarios.
 *   - Búsqueda por id y por nombre de login.
 *   - Listados resumidos para los paneles de profesor y tutor.
 *
 * No valida reglas de negocio ni conoce nada de HTTP.
 * Toda operación devuelve DbResult o el struct correspondiente.
 */
class UsuarioRepository {
public:
    /**
     * @brief Devuelve la instancia única.
     * @return Referencia al UsuarioRepository.
     */
    static UsuarioRepository & getInstance() {
        static UsuarioRepository instance;
        return instance;
    }

    /**
     * @brief Inserta un usuario nuevo en la BD.
     * @param u Usuario a insertar (idUsuario se ignora, lo asigna la BD).
     * @return DbResult con ok=true e id=idUsuario generado si tuvo éxito.
     */
    DbResult crear(const Usuario & u);

    /**
     * @brief Busca un usuario por su nombre de login.
     * @param usuario Nombre de login a buscar.
     * @return Usuario encontrado, o struct con idUsuario=0 si no existe.
     */
    Usuario buscarPorUsuario(const String & usuario);

    /**
     * @brief Busca un usuario por su id.
     * @param idUsuario Id a buscar.
     * @return Usuario encontrado, o struct con idUsuario=0 si no existe.
     */
    Usuario buscarPorId(int idUsuario);

    /**
     * @brief Actualiza usuario, nombre, apellido, referencia y contacto.
     * @note No actualiza rol, hashPassword ni salt.
     * @param u Usuario con los datos nuevos (idUsuario debe ser válido).
     * @return DbResult con ok=true si tuvo éxito.
     */
    DbResult actualizar(const Usuario & u);

    /**
     * @brief Actualiza el hash y salt de la contraseña de un usuario.
     * @param idUsuario  Id del usuario a actualizar.
     * @param nuevoHash  Nuevo hash de la contraseña.
     * @param nuevoSalt  Nuevo salt.
     * @return DbResult con ok=true si tuvo éxito.
     */
    DbResult actualizarPassword(int idUsuario, const String & nuevoHash, const String & nuevoSalt);

    /**
     * @brief Elimina un usuario por su id.
     * @details ON DELETE CASCADE borra también sus cuestionarios, y con ellos
     * sus preguntas y opciones.
     * @param idUsuario Id del usuario a eliminar.
     * @return DbResult con ok=true si tuvo éxito (también si el id no existía).
     */
    DbResult eliminar(int idUsuario);

    /**
     * @brief Verifica si ya existe un usuario con ese nombre de login.
     * @param usuario Nombre de login a verificar.
     * @return true si ya existe.
     */
    bool existeUsuario(const String & usuario);

    /**
     * @brief Verifica si ya existe un usuario con ese nombre de login
     * excluyendo un id específico (para validar al editar el propio perfil).
     * @param usuario   Nombre de login a verificar.
     * @param excluirId Id del usuario a excluir de la búsqueda.
     * @return true si ya existe otro usuario con ese nombre.
     */
    bool existeUsuarioExcluyendo(const String & usuario, int excluirId);

    /**
     * @brief Lista una página de los usuarios de un rol en formato resumido,
     * ordenados por apellido, nombre e id.
     * @param rol     "profesor" o "tutor".
     * @param buffer  Array destino.
     * @param maxSize Capacidad del array (= tamaño de página).
     * @param offset  Cantidad de usuarios a saltear (páginas anteriores).
     * @return Cantidad de usuarios cargados en el buffer.
     */
    int listarPorRol(const String & rol, UsuarioResumen * buffer, int maxSize, int offset);

    /**
     * @brief Cuenta los usuarios de un rol.
     * @param rol "profesor" o "tutor".
     * @return Cantidad de usuarios con ese rol.
     */
    int contarPorRol(const String & rol);

private:
    UsuarioRepository() {
    }
    UsuarioRepository(const UsuarioRepository &) = delete;
    UsuarioRepository & operator=(const UsuarioRepository &) = delete;

    /**
     * @brief Convierte la fila actual de un stmt en un struct Usuario.
     * @param stmt Statement con una fila lista para leer.
     * @return Usuario con los datos de la fila.
     */
    Usuario _filaAUsuario(sqlite3_stmt * stmt);
};

#endif // USUARIO_REPOSITORY_H
