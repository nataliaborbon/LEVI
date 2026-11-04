#include "ui/screens.h"

lv_obj_t * ta_alumno;   ///< Campo del nombre del alumno.
lv_obj_t * ta_clave1;   ///< Campo de la clave de profesor.
lv_obj_t * ta_clave2;   ///< Campo de la clave de tutor.
lv_obj_t * kb;          ///< Teclado en pantalla (oculto hasta enfocar un campo).
lv_obj_t * btn_guardar; ///< Botón Guardar.
lv_obj_t * cont_form;   ///< Contenedor del formulario (se desplaza para que el teclado no tape el campo).

/**
 * @brief Callback de animación: mueve el formulario en Y.
 * @param obj Formulario (cont_form).
 * @param v   Nueva posición Y.
 */
static void anim_y_form_cb(void * obj, int32_t v) {
    lv_obj_set_y((lv_obj_t *)obj, v);
}

/**
 * @brief Anima el formulario hasta una posición Y en 200 ms.
 * @param y_destino Posición Y final (0 = posición original).
 */
static void desplazar_formulario(lv_coord_t y_destino) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, cont_form);
    lv_anim_set_exec_cb(&a, anim_y_form_cb);
    lv_anim_set_values(&a, lv_obj_get_y(cont_form), y_destino);
    lv_anim_set_time(&a, 200);
    lv_anim_start(&a);
}

/**
 * @brief Eventos de los campos de texto.
 *
 * Al enfocar un campo muestra el teclado y sube el formulario si el campo
 * quedaría tapado. Al desenfocar, o con OK / cancelar del teclado, lo
 * oculta y vuelve el formulario a su lugar.
 *
 * @param e Evento de LVGL.
 */
static void ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_move_foreground(kb);
        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(kb);
        lv_coord_t alto_teclado = lv_obj_get_height(kb);
        lv_coord_t limite_visible = lv_display_get_vertical_resolution(NULL) - alto_teclado;
        lv_coord_t y2_campo = lv_obj_get_y(ta) + lv_obj_get_height(ta);

        lv_coord_t y_destino = 0;
        if (y2_campo > limite_visible) {
            y_destino = -(y2_campo - limite_visible + 10);
        }
        desplazar_formulario(y_destino);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        desplazar_formulario(0);
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_remove_state(ta, LV_STATE_FOCUSED);
        lv_indev_reset(NULL, ta);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        desplazar_formulario(0);
    }
}

/**
 * @brief Botón Guardar: guarda nombre del alumno y claves en NVS y reinicia.
 * @note Si algún campo está vacío o falla la escritura, solo lo informa por
 *       Serial y no reinicia.
 * @param e Evento de LVGL.
 */
static void btn_guardar_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        String alumno = lv_textarea_get_text(ta_alumno);
        String clave1 = lv_textarea_get_text(ta_clave1);
        String clave2 = lv_textarea_get_text(ta_clave2);

        if (alumno.length() == 0 || clave1.length() == 0 || clave2.length() == 0) {
            return;
        }

        bool ok0 = ConfigManager::getInstance().guardarNombreAlumno(alumno);
        bool ok1 = ConfigManager::getInstance().guardarClaveProfesor(clave1);
        bool ok2 = ConfigManager::getInstance().guardarClaveTutor(clave2);

        if (!ok0 || !ok1 || !ok2) {
            Serial.println("[Setup] Error al guardar datos en NVS.");
            return;
        }

        Serial.println("[Setup] Guardado exitoso. Reiniciando el sistema...");
        delay(500);
        ESP.restart();
    }
}

/**
 * @brief Botón del ojo: muestra u oculta la clave del campo asociado.
 * @param e Evento de LVGL (user_data = campo de texto).
 */
static void toggle_pw_visibility_cb(lv_event_t * e) {
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_user_data(e);
    bool ahora_oculta = !lv_textarea_get_password_mode(ta);
    lv_textarea_set_password_mode(ta, ahora_oculta);
    lv_obj_t * icono = lv_obj_get_child(btn, 0);
    lv_label_set_text(icono, ahora_oculta ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}

/**
 * @brief Agrega el botón del ojo a la derecha de un campo de clave.
 * @param parent      Contenedor del botón.
 * @param ta_asociada Campo de clave que controla.
 */
static void agregar_boton_ojo(lv_obj_t * parent, lv_obj_t * ta_asociada) {
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 30, 30);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t * icono = lv_label_create(btn);
    lv_obj_set_style_text_color(icono, lv_color_hex(0x555555), 0);
    lv_label_set_text(icono, LV_SYMBOL_EYE_CLOSE);
    lv_obj_center(icono);
    lv_obj_align_to(btn, ta_asociada, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_add_event_cb(btn, toggle_pw_visibility_cb, LV_EVENT_CLICKED, ta_asociada);
}

/**
 * @brief Hace el campo más alto y con letra más grande para la pantalla de 320x480.
 * @param ta Campo de texto.
 */
static void estilo_campo(lv_obj_t * ta) {
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_18, 0);
    lv_obj_set_style_pad_ver(ta, 14, 0);
}

void ui_screen_setup_init(void) {
    lv_obj_t * pantalla_actual = lv_screen_active();
    lv_obj_set_style_bg_color(pantalla_actual, lv_color_hex(0x2C3E50), 0);

    kb = lv_keyboard_create(pantalla_actual);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    cont_form = lv_obj_create(pantalla_actual);
    lv_obj_remove_style_all(cont_form);
    lv_obj_set_size(cont_form, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(cont_form, 0, 0);
    lv_obj_remove_flag(cont_form, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * titulo = lv_label_create(cont_form);
    lv_label_set_text(titulo, "L.E.V.I. - Configurar Sistema");
    lv_obj_set_style_text_color(titulo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(titulo, &lv_font_montserrat_18, 0);
    lv_obj_align(titulo, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t * lbl_alumno_titulo = lv_label_create(cont_form);
    lv_label_set_text(lbl_alumno_titulo, "Nombre del Alumno:");
    lv_obj_set_style_text_color(lbl_alumno_titulo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_alumno_titulo, LV_ALIGN_TOP_MID, 0, 40);

    ta_alumno = lv_textarea_create(cont_form);
    lv_textarea_set_one_line(ta_alumno, true);
    lv_textarea_set_placeholder_text(ta_alumno, "Ej: Juan Perez");
    lv_obj_set_width(ta_alumno, 280);
    estilo_campo(ta_alumno);
    lv_obj_align(ta_alumno, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_event_cb(ta_alumno, ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * lbl_clave1_titulo = lv_label_create(cont_form);
    lv_label_set_text(lbl_clave1_titulo, "Clave Profesor:");
    lv_obj_set_style_text_color(lbl_clave1_titulo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_clave1_titulo, LV_ALIGN_TOP_MID, 0, 125);

    ta_clave1 = lv_textarea_create(cont_form);
    lv_textarea_set_one_line(ta_clave1, true);
    lv_textarea_set_password_mode(ta_clave1, true);
    lv_textarea_set_placeholder_text(ta_clave1, "Clave Profesor");
    lv_obj_set_width(ta_clave1, 280);
    estilo_campo(ta_clave1);
    lv_obj_align(ta_clave1, LV_ALIGN_TOP_MID, 0, 145);
    lv_obj_add_event_cb(ta_clave1, ta_event_cb, LV_EVENT_ALL, NULL);
    agregar_boton_ojo(cont_form, ta_clave1);

    lv_obj_t * lbl_clave2_titulo = lv_label_create(cont_form);
    lv_label_set_text(lbl_clave2_titulo, "Clave Tutor:");
    lv_obj_set_style_text_color(lbl_clave2_titulo, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl_clave2_titulo, LV_ALIGN_TOP_MID, 0, 210);

    ta_clave2 = lv_textarea_create(cont_form);
    lv_textarea_set_one_line(ta_clave2, true);
    lv_textarea_set_password_mode(ta_clave2, true);
    lv_textarea_set_placeholder_text(ta_clave2, "Clave Tutor");
    lv_obj_set_width(ta_clave2, 280);
    estilo_campo(ta_clave2);
    lv_obj_align(ta_clave2, LV_ALIGN_TOP_MID, 0, 230);
    lv_obj_add_event_cb(ta_clave2, ta_event_cb, LV_EVENT_ALL, NULL);
    agregar_boton_ojo(cont_form, ta_clave2);

    btn_guardar = lv_button_create(cont_form);
    lv_obj_set_size(btn_guardar, 200, 40);
    lv_obj_align(btn_guardar, LV_ALIGN_TOP_MID, 0, 310);
    lv_obj_add_event_cb(btn_guardar, btn_guardar_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * lbl_btn = lv_label_create(btn_guardar);
    lv_label_set_text(lbl_btn, "Guardar");
    lv_obj_center(lbl_btn);
}