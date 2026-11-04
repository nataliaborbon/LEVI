#include "stream_server.h"

#define PART_BOUNDARY "123456789000000000000987654321"
static const char * _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char * _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char * _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n";

/// @brief FPS máximos enviados (el front procesa a 15 FPS; se manda un poco más
/// para que siempre tenga un frame nuevo disponible)
#define STREAM_FPS         20
#define FRAME_INTERVAL_MS  (1000 / STREAM_FPS)

/// @brief Capturas fallidas seguidas antes de cortar el stream (~1 s)
#define MAX_FALLOS_CAPTURA 10

/// @brief Frames que se descartan al iniciar el stream (viejos o con exposición sin ajustar)
#define FRAMES_DESCARTE    3

/// @brief Handle del servidor HTTP del stream (NULL hasta que arranca).
httpd_handle_t stream_httpd = NULL;

/**
 * @brief Handler de GET /stream: transmite frames JPEG hasta que el cliente
 * se desconecta.
 *
 * Enciende el LED IR, descarta los primeros FRAMES_DESCARTE frames y manda
 * cada frame como una parte del multipart, esperando lo necesario para no
 * superar STREAM_FPS. Corta si falla el envío o si fallan
 * MAX_FALLOS_CAPTURA capturas seguidas. Al salir apaga el LED IR.
 *
 * @param req Request HTTP.
 * @return El error que cortó el stream: el de envío (cliente desconectado),
 *         ESP_FAIL si fallaron las capturas, o el de httpd_resp_set_type().
 */
static esp_err_t stream_handler(httpd_req_t * req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[128];
    int fallos = 0;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    wakeCamera();

    for (int i = 0; i < FRAMES_DESCARTE; i++) {
        fb = esp_camera_fb_get();
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
    }

    while (true) {
        uint32_t inicio = millis();

        fb = esp_camera_fb_get();
        if (!fb) {
            if (++fallos >= MAX_FALLOS_CAPTURA) {
                res = ESP_FAIL;
                break;
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }
        fallos = 0;

        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));

        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, 128, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }

        if (res != ESP_OK) {
            break;
        }

        uint32_t transcurrido = millis() - inicio;
        if (transcurrido < FRAME_INTERVAL_MS) {
            vTaskDelay((FRAME_INTERVAL_MS - transcurrido) / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(1);
        }
    }

    sleepCamera();
    return res;
}

void startStreamServer() {
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = 80;
    server_config.recv_wait_timeout = 5;
    server_config.send_wait_timeout = 2;
    server_config.lru_purge_enable = true;

    httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};

    if (httpd_start(&stream_httpd, &server_config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.println("Servidor HTTP del módulo de captura iniciado: http://192.168.4.50/stream");
    }
}
