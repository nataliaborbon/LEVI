#include "network/WebServer.h"

/// @brief Servidor web asíncrono en el puerto 80.
AsyncWebServer server(80);

void initWebServer() {
    // -----------------------------------------------------------------------
    // API
    // -----------------------------------------------------------------------
    registrarAuthController(server);
    registrarUsuarioController(server);
    registrarCuestionarioController(server);
    registrarRespuestaController(server);

    // -----------------------------------------------------------------------
    // opencv.js desde SD
    // -----------------------------------------------------------------------
    server.on("/opencv.js", HTTP_GET, [](AsyncWebServerRequest * request) {
        if (!SD_MMC.exists("/opencv.js") && !SD_MMC.exists("/opencv.js.gz")) {
            request->send(404, "text/plain", "opencv.js no encontrado en SD.");
            return;
        }

        AsyncWebServerResponse * response = request->beginResponse(SD_MMC, "/opencv.js", "text/javascript");

        response->addHeader("Cache-Control", "max-age=31536000");

        request->send(response);
    });

    // -----------------------------------------------------------------------
    // Redirección IP -> levi.local
    // -----------------------------------------------------------------------
    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
        String host = request->host();

        if (host != "levi.local") {
            request->redirect("http://levi.local");
            return;
        }

        request->send(LittleFS, "/index.html", "text/html");
    });

    // -----------------------------------------------------------------------
    // Front (SPA)
    // -----------------------------------------------------------------------
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // -----------------------------------------------------------------------
    // 404 - Catch-All  + CORS
    // -----------------------------------------------------------------------
    server.onNotFound([](AsyncWebServerRequest * request) {
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse * response = request->beginResponse(200);
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Content-Type,Authorization,X-Pestana,X-Tomar-Pestana");
            request->send(response);
            return;
        }

        String url = request->url();
        if (url.startsWith("/api/")) {
            request->send(404, "application/json", "{\"ok\":false,\"mensaje\":\"Endpoint de la API no encontrado.\"}");
            return;
        }

        if (request->host() != "levi.local") {
            request->redirect("http://levi.local" + url);
            return;
        }

        request->send(LittleFS, "/index.html", "text/html");
    });

    server.begin();

    Serial.println("[WebServer] Servidor iniciado en el puerto 80.");
}