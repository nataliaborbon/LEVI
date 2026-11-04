#include <Arduino.h>
#include "data.h"
#include "wifi_sta.h"
#include "camera.h"
#include "stream_server.h"

/**
 * @file main.cpp
 * @brief Punto de entrada del firmware de la ESP32-CAM.
 *
 * Módulo de captura: se conecta a la red LEVI con IP fija, inicia la
 * cámara y sirve el video en http://192.168.4.50/stream. Todo el trabajo
 * lo hace el servidor HTTP en su propia tarea; loop() no hace nada.
 */

/// @brief Pin del LED de flash (queda siempre apagado).
#define FLASH_LED_PIN             4

/// @brief Espera antes de reiniciar la placa si falla la cámara.
#define ESPERA_REINICIO_CAMARA_MS 3000

/**
 * @brief Apaga el flash y el LED IR, y arranca WiFi, cámara y servidor del
 * stream, en ese orden.
 *
 * Si la cámara no se inicializa, reinicia la placa en vez de levantar un
 * servidor que no puede entregar video.
 */
void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);

    pinMode(IR_LED_PIN, OUTPUT);
    digitalWrite(IR_LED_PIN, LOW);

    startWifi();

    if (!startCamera()) {
        Serial.println("[CAM] No se pudo iniciar la cámara, reiniciando...");
        delay(ESPERA_REINICIO_CAMARA_MS);
        ESP.restart();
    }

    startStreamServer();
}

/// @brief Sin trabajo: solo cede el procesador.
void loop() {
    delay(100);
}