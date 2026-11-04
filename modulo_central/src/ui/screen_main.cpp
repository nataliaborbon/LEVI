#include "ui/screens.h"

lv_obj_t * tv_main;              ///< Tileview con las 3 pantallas.
lv_obj_t * lbl_alumno_main;      ///< Pantalla 2: nombre del alumno.
lv_obj_t * lbl_usuario_main;     ///< Pantalla 2: usuario de la sesión del panel.
lv_obj_t * lbl_camara_main;      ///< Pantalla 2: estado de la cámara.
lv_obj_t * lbl_disp_main;        ///< Pantalla 2: cantidad de dispositivos conectados.
lv_obj_t * lbl_esperando_examen; ///< Pantalla 3, vista 1: "Esperando cuestionario".
lv_obj_t * lbl_titulo_examen;    ///< Pantalla 3, vista 2: título del examen activo.
lv_obj_t * lbl_progreso_titulo;  ///< Pantalla 3, vista 2: texto "Progreso:".
lv_obj_t * lbl_progreso_main;    ///< Pantalla 3, vista 2: "N/total".

// --- VISTA 3: EXAMEN FINALIZADO (solo se usan dentro de este archivo) ---
static lv_obj_t * lbl_finalizado_titulo; ///< "EXAMEN FINALIZADO".
static lv_obj_t * lbl_finalizado_datos;  ///< Puntajes, tiempo y si aprobó.
static lv_obj_t * lbl_finalizado_cuenta; ///< Cuenta regresiva para volver.

/// @brief Timer que hace volver a la vista "esperando" 10 s después de finalizar.
static lv_timer_t * timer_regreso_examen = nullptr;
/// @brief Segundos que faltan para volver a "esperando".
static int segundos_restantes_examen = 0;

/**
 * @brief Formatea segundos como "MM:SS" (o "H:MM:SS" si superó la hora),
 * misma lógica que el formatearTiempo() del front web.
 * @param totalSegundos Segundos a formatear (se redondean; negativos = 0).
 * @return Tiempo formateado.
 */
static String formatearTiempoLv(float totalSegundos) {
    long s = (long)(totalSegundos > 0 ? totalSegundos + 0.5f : 0);
    long horas = s / 3600;
    long minutos = (s % 3600) / 60;
    long segundos = s % 60;
    char buf[16];
    if (horas > 0) {
        snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", horas, minutos, segundos);
    } else {
        snprintf(buf, sizeof(buf), "%02ld:%02ld", minutos, segundos);
    }
    return String(buf);
}

/**
 * @brief Oculta la vista de "finalizado" y vuelve a mostrar "esperando cuestionario".
 */
static void ui_examen_volver_a_esperando() {
    lv_obj_add_flag(lbl_finalizado_titulo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_finalizado_datos, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_finalizado_cuenta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(lbl_esperando_examen, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief Tick de la cuenta regresiva de la vista "finalizado" (1 vez por
 * segundo). Al llegar a 0 vuelve a "esperando" y borra el timer.
 * @param timer Timer de LVGL que la llama.
 */
static void cb_cuenta_regresiva_examen(lv_timer_t * timer) {
    segundos_restantes_examen--;
    if (segundos_restantes_examen <= 0) {
        ui_examen_volver_a_esperando();
        lv_timer_delete(timer);
        timer_regreso_examen = nullptr;
        return;
    }
    lv_label_set_text_fmt(lbl_finalizado_cuenta, "Volviendo en %d s...", segundos_restantes_examen);
}

void ui_update_usuario(const char * nombre) {
    if (lbl_usuario_main) {
        if (strlen(nombre) == 0) {
            lv_label_set_text(lbl_usuario_main, "Sin conexion");
        } else {
            lv_label_set_text(lbl_usuario_main, nombre);
        }
    }
}

void ui_update_dispositivos(int cantidad) {
    if (lbl_disp_main) {
        lv_label_set_text_fmt(lbl_disp_main, "%d", cantidad);
    }
}

void ui_update_camara(bool lista) {
    if (lbl_camara_main) {
        if (lista) {
            lv_label_set_text(lbl_camara_main, "Transmitiendo");
            lv_obj_set_style_text_color(lbl_camara_main, lv_color_hex(0xd1338e), 0);
        } else {
            lv_label_set_text(lbl_camara_main, "Desconectada");
            lv_obj_set_style_text_color(lbl_camara_main, lv_color_hex(0x18b3c3), 0);
        }
    }
}

void ui_update_examen(const char * estado, const char * titulo, int numeroPregunta, int totalPreguntas,
                      float puntajeObtenido, float puntajeParaAprobar, float puntajeMaximo, float tiempoSegundos,
                      bool aprobado) {
    if (!lbl_esperando_examen)
        return;

    String estadoStr = estado;

    static String ultimoEstado = "";

    if (estadoStr == "finalizado" && ultimoEstado == "finalizado") {
        return;
    }

    if (estadoStr != ultimoEstado && timer_regreso_examen) {
        lv_timer_delete(timer_regreso_examen);
        timer_regreso_examen = nullptr;
    }

    if (estadoStr == "esperando" || estadoStr == "sin_sesion") {
        lv_obj_remove_flag(lbl_esperando_examen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_titulo_examen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_progreso_titulo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_progreso_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_finalizado_titulo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_finalizado_datos, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_finalizado_cuenta, LV_OBJ_FLAG_HIDDEN);

    } else if (estadoStr == "finalizado") {
        lv_obj_add_flag(lbl_esperando_examen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_titulo_examen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_progreso_titulo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_progreso_main, LV_OBJ_FLAG_HIDDEN);

        lv_obj_remove_flag(lbl_finalizado_titulo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(lbl_finalizado_datos, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(lbl_finalizado_cuenta, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(lbl_finalizado_datos, "Necesario: %g\nObtenido: %g / %g\nTiempo: %s\n\n%s",
                              puntajeParaAprobar, puntajeObtenido, puntajeMaximo,
                              formatearTiempoLv(tiempoSegundos).c_str(), aprobado ? "APROBADO" : "DESAPROBADO");
        lv_obj_set_style_text_color(lbl_finalizado_datos, aprobado ? lv_color_hex(0xd1338e) : lv_color_hex(0x18b3c3),
                                    0);

        segundos_restantes_examen = 10;
        lv_label_set_text_fmt(lbl_finalizado_cuenta, "Volviendo en %d s...", segundos_restantes_examen);
        timer_regreso_examen = lv_timer_create(cb_cuenta_regresiva_examen, 1000, nullptr);

    } else { // "en_progreso"
        lv_obj_add_flag(lbl_esperando_examen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(lbl_titulo_examen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(lbl_progreso_titulo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(lbl_progreso_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_finalizado_titulo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_finalizado_datos, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_finalizado_cuenta, LV_OBJ_FLAG_HIDDEN);
        if (strlen(titulo) > 0) {
            lv_label_set_text_fmt(lbl_titulo_examen, "Examen:\n\"%s\"", titulo);
        } else {
            lv_label_set_text(lbl_titulo_examen, "Examen:\n\"-\"");
        }
        lv_label_set_text_fmt(lbl_progreso_main, "%d/%d", numeroPregunta, totalPreguntas);
    }

    ultimoEstado = estadoStr;
}

void ui_screen_main_init() {
    tv_main = lv_tileview_create(lv_screen_active());
    lv_obj_set_scrollbar_mode(tv_main, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tv_main, lv_color_hex(0x0F0F0F), 0);
    lv_obj_set_style_bg_opa(tv_main, LV_OPA_COVER, 0);

    // ==========================================
    // PANTALLA 1: BIENVENIDA (Columna 0, Fila 0)
    // ==========================================
    lv_obj_t * tile1 = lv_tileview_add_tile(tv_main, 0, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(tile1, lv_color_hex(0x0F0F0F), 0);
    lv_obj_set_style_text_color(tile1, lv_color_hex(0xDEDEDE), 0);

    lv_obj_t * etiqueta_bienvenida = lv_label_create(tile1);
    lv_label_set_text(etiqueta_bienvenida, "Bienvenido\nIngresa a \"LEVI\"");
    lv_obj_set_style_text_align(etiqueta_bienvenida, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(etiqueta_bienvenida, &lv_font_montserrat_18, 0);
    lv_obj_align(etiqueta_bienvenida, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t * qr_img = lv_image_create(tile1);
    lv_image_set_src(qr_img, &qr_levi_local_x2);
    lv_obj_align_to(qr_img, etiqueta_bienvenida, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    // ==========================================
    // PANTALLA 2: ESTADO SISTEMA (Columna 1, Fila 0)
    // ==========================================
    lv_obj_t * tile2 = lv_tileview_add_tile(tv_main, 1, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(tile2, lv_color_hex(0x0F0F0F), 0);
    lv_obj_set_style_text_color(tile2, lv_color_hex(0xDEDEDE), 0);

    lv_obj_t * titulo_estado = lv_label_create(tile2);
    lv_label_set_text(titulo_estado, "Estado");
    lv_obj_set_style_text_font(titulo_estado, &lv_font_montserrat_20, 0);
    lv_obj_align(titulo_estado, LV_ALIGN_TOP_MID, 0, 15);

    const lv_font_t * fuente_datos = &lv_font_montserrat_16;

    // --- ALUMNO ---
    lv_obj_t * lbl_alumno_titulo = lv_label_create(tile2);
    lv_label_set_text(lbl_alumno_titulo, "Alumno: ");
    lv_obj_set_style_text_font(lbl_alumno_titulo, fuente_datos, 0);
    lv_obj_align(lbl_alumno_titulo, LV_ALIGN_TOP_LEFT, 20, 60);

    lbl_alumno_main = lv_label_create(tile2);

    String nombreGuardado = ConfigManager::getInstance().leerNombreAlumno();
    if (nombreGuardado.length() > 0) {
        lv_label_set_text(lbl_alumno_main, nombreGuardado.c_str());
    } else {
        lv_label_set_text(lbl_alumno_main, "No registrado");
    }

    lv_obj_set_style_text_font(lbl_alumno_main, fuente_datos, 0);
    lv_obj_align_to(lbl_alumno_main, lbl_alumno_titulo, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    // --- USUARIO ---
    lv_obj_t * lbl_usuario_titulo = lv_label_create(tile2);
    lv_label_set_text(lbl_usuario_titulo, "Usuario: ");
    lv_obj_set_style_text_font(lbl_usuario_titulo, fuente_datos, 0);
    lv_obj_align(lbl_usuario_titulo, LV_ALIGN_TOP_LEFT, 20, 100);

    lbl_usuario_main = lv_label_create(tile2);
    lv_label_set_text(lbl_usuario_main, "Sin conexion");
    lv_obj_set_style_text_font(lbl_usuario_main, fuente_datos, 0);
    lv_obj_align_to(lbl_usuario_main, lbl_usuario_titulo, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    // --- CAMARA ---
    lv_obj_t * lbl_camara_titulo = lv_label_create(tile2);
    lv_label_set_text(lbl_camara_titulo, "Camara: ");
    lv_obj_set_style_text_font(lbl_camara_titulo, fuente_datos, 0);
    lv_obj_align(lbl_camara_titulo, LV_ALIGN_TOP_LEFT, 20, 140);

    lbl_camara_main = lv_label_create(tile2);
    lv_label_set_text(lbl_camara_main, "Cargando...");
    lv_obj_set_style_text_font(lbl_camara_main, fuente_datos, 0);
    lv_obj_align_to(lbl_camara_main, lbl_camara_titulo, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    // --- DISPOSITIVOS ---
    lv_obj_t * lbl_disp_titulo = lv_label_create(tile2);
    lv_label_set_text(lbl_disp_titulo, "Dispositivos conectados: ");
    lv_obj_set_style_text_font(lbl_disp_titulo, fuente_datos, 0);
    lv_obj_align(lbl_disp_titulo, LV_ALIGN_TOP_LEFT, 20, 180);

    lbl_disp_main = lv_label_create(tile2);
    lv_label_set_text(lbl_disp_main, "0");
    lv_obj_set_style_text_font(lbl_disp_main, fuente_datos, 0);
    lv_obj_align_to(lbl_disp_main, lbl_disp_titulo, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

    // ==========================================
    // PANTALLA 3: EXAMEN (Columna 2, Fila 0)
    // ==========================================
    lv_obj_t * tile3 = lv_tileview_add_tile(tv_main, 2, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(tile3, lv_color_hex(0x0F0F0F), 0);
    lv_obj_set_style_text_color(tile3, lv_color_hex(0xDEDEDE), 0);

    // --- VISTA 1: ESPERANDO ---
    lbl_esperando_examen = lv_label_create(tile3);
    lv_label_set_text(lbl_esperando_examen, "Esperando\ncuestionario");
    lv_obj_set_style_text_align(lbl_esperando_examen, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_esperando_examen, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_esperando_examen, lv_color_hex(0x18b3c3), 0);
    lv_obj_align(lbl_esperando_examen, LV_ALIGN_CENTER, 0, 0);

    // --- VISTA 2: EXAMEN ACTIVO ---
    lbl_titulo_examen = lv_label_create(tile3);
    lv_label_set_text(lbl_titulo_examen, "Examen:\n\"-\"");
    lv_obj_set_style_text_align(lbl_titulo_examen, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_titulo_examen, &lv_font_montserrat_18, 0);
    lv_obj_set_width(lbl_titulo_examen, 180);
    lv_label_set_long_mode(lbl_titulo_examen, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_titulo_examen, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_add_flag(lbl_titulo_examen, LV_OBJ_FLAG_HIDDEN);

    lbl_progreso_titulo = lv_label_create(tile3);
    lv_label_set_text(lbl_progreso_titulo, "Progreso: ");
    lv_obj_set_style_text_font(lbl_progreso_titulo, fuente_datos, 0);
    lv_obj_align(lbl_progreso_titulo, LV_ALIGN_LEFT_MID, 20, 30);
    lv_obj_add_flag(lbl_progreso_titulo, LV_OBJ_FLAG_HIDDEN);

    lbl_progreso_main = lv_label_create(tile3);
    lv_label_set_text(lbl_progreso_main, "0/0");
    lv_obj_set_style_text_font(lbl_progreso_main, fuente_datos, 0);
    lv_obj_align_to(lbl_progreso_main, lbl_progreso_titulo, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_add_flag(lbl_progreso_main, LV_OBJ_FLAG_HIDDEN);

    // --- VISTA 3: EXAMEN FINALIZADO ---
    lbl_finalizado_titulo = lv_label_create(tile3);
    lv_label_set_text(lbl_finalizado_titulo, "EXAMEN\nFINALIZADO");
    lv_obj_set_style_text_align(lbl_finalizado_titulo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_finalizado_titulo, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_finalizado_titulo, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_flag(lbl_finalizado_titulo, LV_OBJ_FLAG_HIDDEN);

    lbl_finalizado_datos = lv_label_create(tile3);
    lv_label_set_text(lbl_finalizado_datos, "");
    lv_obj_set_style_text_align(lbl_finalizado_datos, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lbl_finalizado_datos, fuente_datos, 0);
    lv_obj_align(lbl_finalizado_datos, LV_ALIGN_CENTER, 0, 15);
    lv_obj_add_flag(lbl_finalizado_datos, LV_OBJ_FLAG_HIDDEN);

    lbl_finalizado_cuenta = lv_label_create(tile3);
    lv_label_set_text(lbl_finalizado_cuenta, "");
    lv_obj_set_style_text_font(lbl_finalizado_cuenta, fuente_datos, 0);
    lv_obj_set_style_text_color(lbl_finalizado_cuenta, lv_color_hex(0x777777), 0);
    lv_obj_align(lbl_finalizado_cuenta, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_add_flag(lbl_finalizado_cuenta, LV_OBJ_FLAG_HIDDEN);
}