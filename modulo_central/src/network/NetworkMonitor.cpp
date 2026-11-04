#include "network/NetworkMonitor.h"

/// @brief Sesión de ping hacia la cámara (nullptr si está detenida).
static esp_ping_handle_t _pingCamara = nullptr;
/// @brief Resultado del último ping a la cámara.
static bool _camaraConectada = false;
/// @brief Última cantidad leída de dispositivos conectados al AP.
static int _cantidadDispositivos = 0;

/**
 * @brief Callback de ping respondido: marca la cámara como conectada.
 * @param hdl  Sesión de ping que respondió.
 * @param args Sin uso.
 */
static void onPingSuccess(esp_ping_handle_t hdl, void * args) {
    _camaraConectada = true;
}

/**
 * @brief Callback de ping sin respuesta: marca la cámara como desconectada.
 * @param hdl  Sesión de ping que no respondió.
 * @param args Sin uso.
 */
static void onPingTimeout(esp_ping_handle_t hdl, void * args) {
    _camaraConectada = false;
}

/**
 * @brief Crea la sesión de ping hacia IP_modulo_captura y la arranca.
 *
 * Se separa de networkMonitor_init() para poder volver a llamarla cuando
 * la pantalla se despierta, después de haber sido destruida por
 * networkMonitor_detenerPingCamara().
 */
static void crearYArrancarPing() {
    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr.u_addr.ip4.addr = static_cast<uint32_t>(IP_modulo_captura);
    cfg.target_addr.type = IPADDR_TYPE_V4;
    cfg.count = ESP_PING_COUNT_INFINITE;
    cfg.interval_ms = 2000;
    cfg.timeout_ms = 500;

    esp_ping_callbacks_t cbs = {};
    cbs.on_ping_success = onPingSuccess;
    cbs.on_ping_timeout = onPingTimeout;

    esp_ping_new_session(&cfg, &cbs, &_pingCamara);
    esp_ping_start(_pingCamara);
}

void networkMonitor_init() {
    crearYArrancarPing();
}

void networkMonitor_loop() {
    _cantidadDispositivos = WiFi.softAPgetStationNum();
}

void networkMonitor_detenerPingCamara() {
    if (_pingCamara == nullptr)
        return; // ya estaba detenido

    esp_ping_stop(_pingCamara);
    esp_ping_delete_session(_pingCamara);
    _pingCamara = nullptr;

    _camaraConectada = false;
}

void networkMonitor_reanudarPingCamara() {
    if (_pingCamara != nullptr)
        return; // ya está corriendo

    crearYArrancarPing();
}

int networkMonitor_getCantidadDispositivos() {
    return _cantidadDispositivos;
}
bool networkMonitor_isCamaraConectada() {
    return _camaraConectada;
}