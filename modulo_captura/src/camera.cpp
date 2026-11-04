#include "camera.h"

// ------------------------------------------------------------------
// Configuración de la Cámara
// ------------------------------------------------------------------
/// @brief Configuración pasada a esp_camera_init().
static camera_config_t cfg;

bool startCamera() {
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer = LEDC_TIMER_0;
    cfg.pin_d0 = Y2_GPIO_NUM;
    cfg.pin_d1 = Y3_GPIO_NUM;
    cfg.pin_d2 = Y4_GPIO_NUM;
    cfg.pin_d3 = Y5_GPIO_NUM;
    cfg.pin_d4 = Y6_GPIO_NUM;
    cfg.pin_d5 = Y7_GPIO_NUM;
    cfg.pin_d6 = Y8_GPIO_NUM;
    cfg.pin_d7 = Y9_GPIO_NUM;
    cfg.pin_xclk = XCLK_GPIO_NUM;
    cfg.pin_pclk = PCLK_GPIO_NUM;
    cfg.pin_vsync = VSYNC_GPIO_NUM;
    cfg.pin_href = HREF_GPIO_NUM;
    cfg.pin_sccb_sda = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl = SIOC_GPIO_NUM;
    cfg.pin_pwdn = PWDN_GPIO_NUM;
    cfg.pin_reset = RESET_GPIO_NUM;

    cfg.xclk_freq_hz = 16000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = FRAMESIZE_QVGA;
    cfg.jpeg_quality = 16;
    cfg.fb_count = 2;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&cfg);

    if (err != ESP_OK) {
        Serial.printf("Inicialización de cámara falló (0x%x)\n", err);
        return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s) {
        s->set_gain_ctrl(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_whitebal(s, 1);
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_ae_level(s, 0);
        s->set_denoise(s, 0);
        s->set_special_effect(s, 0);

        Serial.println("Camera init OK.");
    }

    sleepCamera();
    return true;
}

// ------------------------------------------------------------------
// LED infrarrojo
// ------------------------------------------------------------------
void wakeCamera() {
    digitalWrite(IR_LED_PIN, HIGH);
}

void sleepCamera() {
    digitalWrite(IR_LED_PIN, LOW);
}