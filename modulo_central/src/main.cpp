#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "esp_task_wdt.h"

#include "config/NetworkConfig.h"
#include "config/ConfigManager.h"
#include "storage/SDManager.h"
#include "storage/database/DatabaseManager.h"
#include "network/WiFiAP.h"
#include "network/WebServer.h"
#include "network/NetworkMonitor.h"
#include "session/SessionManager.h"
#include "services/RespuestaService.h"
#include "ui/screens.h"
#include "ui/ui_manager.h"

/**
 * @file main.cpp
 * @brief Punto de entrada del firmware del módulo central (placa ONX3248G035).
 *
 * setup() monta LittleFS, inicia la pantalla (setup o pantallas normales
 * según la configuración inicial), la SD, la base de datos, el AP WiFi, el
 * monitor de red y el servidor web. loop() aplica los timeouts de sesión
 * cada 1 s y mantiene viva la interfaz.
 */

/// @brief Cada cuánto se llama a SessionManager::tick().
static constexpr unsigned long TICK_INTERVAL_MS = 1000UL;
/// @brief millis() del último tick.
static unsigned long _ultimoTick = 0;

static constexpr uint32_t WATCHDOG_TIMEOUT_S = 10;

// LEDs RGB de la placa anterior: en la ONX3248G035 estos pines no son LEDs.
// const int LED_ROJO = 4;
// const int LED_VERDE = 16;
// const int LED_AZUL = 17;

/**
 * @brief Inicializa todo el sistema, en orden. Si no monta LittleFS, se
 * detiene ahí; los errores de SD y BD solo se informan.
 */
void setup() {
    Serial.begin(115200);
    delay(1000);

    esp_err_t rcWdt = esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
    if (rcWdt != ESP_OK) {
        Serial.printf("[Main] Error al configurar el watchdog: %s\n", esp_err_to_name(rcWdt));
    }

    // LEDs RGB de la placa anterior
    // pinMode(LED_ROJO, OUTPUT);
    // pinMode(LED_VERDE, OUTPUT);
    // pinMode(LED_AZUL, OUTPUT);
    //
    // digitalWrite(LED_ROJO, HIGH);
    // digitalWrite(LED_VERDE, HIGH);
    // digitalWrite(LED_AZUL, HIGH);

    // 1. LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("[Main] Error: no se pudo montar LittleFS.");
        return;
    }
    Serial.println("[Main] LittleFS montado.");

    // 2. Claves maestras (LÓGICA PARA LA INTERFAZ)
    bool esPrimerInicio = false;
    if (!ConfigManager::getInstance().clavesConfiguradas()) {
        Serial.println("[Main] Claves maestras NO configuradas. Se requiere configuracion inicial.");
        esPrimerInicio = true;
    } else {
        Serial.println("[Main] Claves maestras encontradas en memoria.");
    }

    // 3. INICIALIZAR LA PANTALLA TÁCTIL (ONX3248G035)
    ui_init(esPrimerInicio);
    Serial.println("[Main] Pantalla inicializada.");

    // 4. SD
    if (!initSD()) {
        Serial.println("[Main] Error: no se pudo montar la SD.");
    }

    // 5. Base de datos
    if (!DatabaseManager::getInstance().begin("/sd/levi.db")) {
        Serial.println("[Main] Error: no se pudo inicializar la base de datos.");
    }

    // 6. WiFi Access Point
    initWiFiAP();

    // 6.1 Monitor de red (ping a la camara)
    networkMonitor_init();

    // 7. Servidor web
    initWebServer();

    Serial.println("[Main] Sistema completamente listo.");
}

/**
 * @brief Cada 1 s aplica los timeouts de sesión; en cada vuelta actualiza la
 * interfaz con el último resumen del examen.
 */
void loop() {
    unsigned long ahora = millis();

    if (ahora - _ultimoTick >= TICK_INTERVAL_MS) {
        _ultimoTick = ahora;
        SessionManager::getInstance().tick();
    }

    EstadoExamenResumen resumenExamen = RespuestaService::getInstance().obtenerResumenCacheado();
    ui_loop(resumenExamen);
}