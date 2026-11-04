#include "ui/ui_manager.h"

// Pantalla ST7796 de la ONX3248G035 en vertical (orientación nativa 320x480)
#define SCREEN_WIDTH           320 ///< Ancho de la pantalla en px.
#define SCREEN_HEIGHT          480 ///< Alto de la pantalla en px.
#define TFT_ROTATION           0   ///< Vertical; si queda cabeza abajo probar 2.

// Bus I2C del táctil y del expansor (según BoardConfig.h del ejemplo oficial)
#define NEXTION_IIC_SDA_PIN    8    ///< SDA del bus I2C.
#define NEXTION_IIC_SCL_PIN    7    ///< SCL del bus I2C.
#define CST826_ADDR            0x15 ///< Dirección I2C del táctil CST826.
#define PCF8574_ADDR           0x38 ///< Dirección I2C del expansor PCF8574.
#define PCF8574_PIN_LCD_RST    6    ///< Pin del expansor conectado al reset del LCD.

#define TOUCH_SWAP_XY          false
#define TOUCH_INVERT_X         false
#define TOUCH_INVERT_Y         false

// Configuración de la Suspensión ---
#define BACKLIGHT_PIN          6     ///< Pin del backlight (el mismo que TFT_BL).
#define TIMEOUT_INACTIVIDAD_MS 60000 ///< Sin toques durante 60 s se apaga la pantalla.

/// @brief millis() del último toque (para la suspensión por inactividad).
static uint32_t last_touch_time = 0;
/// @brief false mientras el backlight está apagado por inactividad.
static bool screen_awake = true;

TFT_eSPI tft = TFT_eSPI();          ///< Driver de la pantalla ST7796.
static Adafruit_CST8XX touch;       ///< Driver del táctil CST826.
static Adafruit_PCF8574 ioExpander; ///< Expansor de I/O (reset del LCD y CS de la SD).
/// @brief true si se intenta leer el táctil (siempre, aunque la librería no reconozca el chip).
static bool touchReady = false;

/// @brief Buffer parcial de LVGL: 32 líneas de SCREEN_WIDTH px a 16 bits (~20 KB en RAM interna).
__attribute__((aligned(16))) static uint8_t draw_buf[SCREEN_WIDTH * 32 * 2];

// =========================================================
// RESET DEL LCD POR EXPANSOR PCF8574 (como el ejemplo oficial)
// =========================================================
/**
 * @brief Resetea el LCD con un pulso bajo en P6 del expansor PCF8574.
 *
 * P6 es el reset del LCD y P7 el CS de la SD. Todas las salidas se dejan en
 * alto salvo el pulso de reset, para no afectar periféricos compartidos.
 * Si el expansor no responde, lo informa por Serial y no resetea.
 */
static void lcdResetByExpander() {
    if (!ioExpander.begin(PCF8574_ADDR, &Wire)) {
        Serial.println("[UI] PCF8574 no responde, se omite el reset del LCD.");
        return;
    }

    ioExpander.digitalWriteByte(0xFF);
    delay(20);
    ioExpander.digitalWriteByte(0xFF & ~(1 << PCF8574_PIN_LCD_RST));
    delay(120);
    ioExpander.digitalWriteByte(0xFF);
    delay(120);
}

// =========================================================
// DRIVER TÁCTIL CST826 (I2C)
// =========================================================

/**
 * @brief Callback de lectura del táctil para LVGL.
 *
 * Si hay un toque renueva el tiempo de inactividad. Si la pantalla estaba
 * suspendida, la despierta (backlight y ping a la cámara) y ese toque se
 * informa como soltado para que no active nada. Si no, convierte las
 * coordenadas del CST826 a las de la pantalla.
 *
 * @param indev Dispositivo de entrada de LVGL.
 * @param data  Estado y coordenadas a completar.
 */
void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    if (touchReady && touch.touched()) {
        last_touch_time = millis();

        if (!screen_awake) {
            screen_awake = true;
            digitalWrite(BACKLIGHT_PIN, HIGH);
            networkMonitor_reanudarPingCamara();

            data->state = LV_INDEV_STATE_RELEASED;
            return;
        }

        CST_TS_Point p = touch.getPoint();
        int32_t x = p.x;
        int32_t y = p.y;

        if (TOUCH_SWAP_XY) {
            int32_t t = x;
            x = y;
            y = t;
        }
        if (TOUCH_INVERT_X)
            x = (SCREEN_WIDTH - 1) - x;
        if (TOUCH_INVERT_Y)
            y = (SCREEN_HEIGHT - 1) - y;

        x = constrain(x, 0, SCREEN_WIDTH - 1);
        y = constrain(y, 0, SCREEN_HEIGHT - 1);

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// =========================================================
// DRIVER DE PANTALLA LVGL
// =========================================================

/**
 * @brief Callback de LVGL que envía un área ya dibujada a la pantalla por SPI.
 * @param disp   Display de LVGL.
 * @param area   Área a actualizar.
 * @param px_map Píxeles RGB565 del área.
 */
void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

/**
 * @brief Fuente de tiempo de LVGL.
 * @return millis().
 */
static uint32_t my_tick_get_cb(void) {
    return millis();
}

// --- FUNCIONES PÚBLICAS ---

void ui_init(bool isFirstBoot) {
    // 0. Bus I2C compartido por el táctil y el expansor, y reset del LCD
    Wire.begin(NEXTION_IIC_SDA_PIN, NEXTION_IIC_SCL_PIN, 400000);
    lcdResetByExpander();

    // 1. Inicializar Pantalla (Usa Hardware SPI)
    tft.begin();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);

    // Configurar Pin de Iluminación
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);
    last_touch_time = millis();

    // 2. Inicializar Táctil CST826 (I2C)
    touchReady = touch.begin(&Wire, CST826_ADDR);
    if (!touchReady) {
        Serial.println("[UI] Aviso: CST826 no reconocido por la libreria, se intenta leer igual.");
        touchReady = true;
    }

    // 3. Inicializar LVGL
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // 4. DECIDIR QUÉ INTERFAZ CARGAR
    if (isFirstBoot) {
        ui_screen_setup_init();
    } else {
        ui_screen_main_init();
    }
}

void ui_loop(const EstadoExamenResumen & resumenExamen) {
    static uint32_t t0;
    static uint32_t espera;
    const uint32_t t = millis();

    if (t - t0 > espera) {
        espera = lv_timer_handler();
        t0 = t;
    }

    if (screen_awake) {
        static String ultimo_usuario_ui = "@@@";
        String usuario_actual = SessionManager::getInstance().nombrePanelActivo();

        if (usuario_actual != ultimo_usuario_ui) {
            ui_update_usuario(usuario_actual.c_str());
            ultimo_usuario_ui = usuario_actual;
        }

        networkMonitor_loop();

        static bool ultima_camara_ui = false;
        static bool primera_vez_camara = true;
        bool camara_actual = networkMonitor_isCamaraConectada();

        if (camara_actual != ultima_camara_ui || primera_vez_camara) {
            ui_update_camara(camara_actual);
            ultima_camara_ui = camara_actual;
            primera_vez_camara = false;
        }

        ui_update_dispositivos(networkMonitor_getCantidadDispositivos());
        ui_update_examen(resumenExamen.estado, resumenExamen.tituloCuestionario, resumenExamen.numeroPregunta,
                         resumenExamen.totalPreguntas, resumenExamen.puntajeObtenido, resumenExamen.puntajeParaAprobar,
                         resumenExamen.puntajeMaximo, resumenExamen.tiempoSegundos, resumenExamen.aprobado);
    }

    if (screen_awake && (millis() - last_touch_time > TIMEOUT_INACTIVIDAD_MS)) {
        screen_awake = false;
        digitalWrite(BACKLIGHT_PIN, LOW);
        networkMonitor_detenerPingCamara();
    }
}