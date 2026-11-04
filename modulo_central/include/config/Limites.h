#pragma once

/**
 * @file Limites.h
 * @brief Límites y tiempos fijos del sistema.
 */

/// @brief Máximo de preguntas por cuestionario (tamaño de los buffers de preguntas).
constexpr int MAX_PREGUNTAS_POR_CUESTIONARIO = 20;

/// @brief Máximo de opciones por pregunta.
constexpr int MAX_OPCIONES_POR_PREGUNTA = 4;

/// @brief Cantidad de elementos por página en los listados paginados
/// (cuestionarios, profesores y tutores).
constexpr int ELEMENTOS_POR_PAGINA = 10;

// ---------------------------------------------------------------------------
// Sesiones (las aplica SessionManager::tick())
//   - Vida: tiempo máximo sin ningún request (pestaña cerrada, sin WiFi).
//   - Inactividad: tiempo máximo sin que el usuario use la pantalla.
// ---------------------------------------------------------------------------

/// @brief Vida de la sesión de profesor o tutor: 2 minutos.
constexpr unsigned long SESION_PANEL_VIDA_MS = 2UL * 60UL * 1000UL;

/// @brief Inactividad máxima de la sesión de profesor o tutor: 10 minutos.
constexpr unsigned long SESION_PANEL_INACTIVIDAD_MS = 10UL * 60UL * 1000UL;

/// @brief Vida de la sesión del invitado: 2 minutos.
constexpr unsigned long SESION_INVITADO_VIDA_MS = 2UL * 60UL * 1000UL;

/// @brief Inactividad máxima de la sesión del invitado: 5 minutos.
constexpr unsigned long SESION_INVITADO_INACTIVIDAD_MS = 5UL * 60UL * 1000UL;

/// @brief Vida de la sesión del alumno: 15 segundos (no tiene reloj de inactividad).
constexpr unsigned long SESION_ALUMNO_VIDA_MS = 15UL * 1000UL;
