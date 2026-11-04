#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

/**
 * @file SDManager.h
 * @brief Inicialización y operaciones básicas sobre la tarjeta SD.
 *
 * La SD aloja dos recursos:
 *   - opencv.js : librería de visión artificial servida con caché.
 *   - levi.db : base de datos SQLite del sistema.
 */

/// @brief Pines SD_MMC de la ONX3248G035 (según BoardConfig.h del ejemplo oficial).
#define SD_CLK_PIN     11
#define SD_CMD_PIN     10
#define SD_D0_PIN      9

/// @brief Punto de montaje de la SD (se mantiene "/sd" para conservar "/sd/levi.db").
#define SD_MOUNT_POINT "/sd"

/**
 * @brief Monta la tarjeta SD usando el bus SD_MMC en modo 1-bit.
 * @return true si la SD se montó correctamente.
 */
bool initSD();

/**
 * @brief Escribe o sobreescribe un archivo de texto.
 * @param fs    Sistema de archivos destino (por ejemplo SD_MMC).
 * @param path  Ruta absoluta del archivo (ej: "/opencv.js").
 * @param data  Contenido a escribir.
 * @return true si la escritura fue exitosa.
 */
bool writeFile(fs::FS & fs, const char * path, const char * data);

#endif // SD_MANAGER_H
