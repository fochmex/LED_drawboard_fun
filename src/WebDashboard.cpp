#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "DashboardPage.h"
#include "EffectControl.h"
#include "DisplayControl.h"
#include "ProjectConfig.h"
#include "WebDashboard.h"

namespace
{
    WebServer server(80);
    bool running = false;

    void sendJson(int code, const String& body)
    {
        server.sendHeader("Cache-Control", "no-store");
        server.sendHeader("X-Content-Type-Options", "nosniff");
        server.send(code, "application/json", body);
    }

    void sendStatus()
    {
        String body;
        body.reserve(1000);
        body = "{\"hostname\":\"" WIFI_HOSTNAME "\",\"ip\":\"";
        body += WiFi.localIP().toString();
        body += "\",\"mac\":\"";
        body += WiFi.macAddress();
        body += "\",\"rssi\":";
        body += String(WiFi.RSSI());
        body += ",\"width\":" + String(MATRIX_WIDTH);
        body += ",\"height\":" + String(MATRIX_HEIGHT);
        body += ",\"pixels\":" + String(PIXEL_COUNT);
        body += ",\"brightness\":" + String(DisplayControl::brightness());
        body += ",\"drawing\":";
        body += DisplayControl::drawing() ? "true" : "false";
        body += ",\"maxBrightness\":" + String(MAX_ALLOWED_BRIGHTNESS);
        body += ",\"uptimeMs\":" + String(millis());
        body += ",\"freeHeap\":" + String(ESP.getFreeHeap());
        body += ",\"effect\":" + String(EffectControl::selected());
        body += ",\"effectName\":\"";
        body += EffectControl::name(EffectControl::selected());
        body += "\",\"playlist\":";
        body += EffectControl::playlistEnabled() ? "true" : "false";
        body += ",\"effects\":[";
        for (uint8_t i = 0; i < EffectControl::count(); ++i)
        {
            if (i != 0) body += ',';
            body += "{\"id\":" + String(i) + ",\"name\":\"";
            body += EffectControl::name(i);
            body += "\"}";
        }
        body += "]}";
        sendJson(200, body);
    }

    bool validateControl(const char* argument)
    {
        // Cross-origin browser forms cannot supply this header; no CORS is enabled.
        if (server.header("X-Drawboard-Control") != "1")
        {
            Serial.println("[HTTP] Rejected control request: missing control header.");
            sendJson(403, "{\"error\":\"Missing X-Drawboard-Control: 1 header\"}");
            return false;
        }
        if (server.args() != 1 || !server.hasArg(argument))
        {
            Serial.println("[HTTP] Rejected control request: invalid arguments.");
            sendJson(400, "{\"error\":\"Exactly one supported form argument is required\"}");
            return false;
        }
        return true;
    }
}

void WebDashboard::begin()
{
    const char* headers[] = {"X-Drawboard-Control"};
    server.collectHeaders(headers, 1);
    server.on("/", HTTP_GET, []()
    {
        server.sendHeader("Cache-Control", "no-store");
        server.sendHeader("X-Frame-Options", "DENY");
        server.sendHeader("X-Content-Type-Options", "nosniff");
        server.send_P(200, "text/html; charset=utf-8", kDashboardPage);
    });
    server.on("/api/status", HTTP_GET, sendStatus);
    server.on("/api/effect", HTTP_POST, []()
    {
        if (!validateControl("id")) return;
        const String id = server.arg("id");
        for (uint8_t i = 0; i < EffectControl::count(); ++i)
        {
            if (id == String(i))
            {
                EffectControl::select(i);
                sendStatus();
                return;
            }
        }
        Serial.println("[HTTP] Rejected effect index.");
        sendJson(400, "{\"error\":\"Unknown effect id\"}");
    });
    server.on("/api/playlist", HTTP_POST, []()
    {
        if (!validateControl("enabled")) return;
        const String enabled = server.arg("enabled");
        if (enabled != "true" && enabled != "false")
        {
            Serial.println("[HTTP] Rejected playlist value.");
            sendJson(400, "{\"error\":\"enabled must be true or false\"}");
            return;
        }
        EffectControl::setPlaylistEnabled(enabled == "true");
        sendStatus();
    });
    server.on("/api/frame", HTTP_POST, []()
    {
        if (!validateControl("pixels")) return;
        const String pixels = server.arg("pixels");
        if (!DisplayControl::applyFrame(pixels.c_str(), pixels.length()))
        {
            sendJson(400, "{\"error\":\"pixels must contain exactly 256 RGB hex colors (1536 hex digits)\"}");
            return;
        }
        sendStatus();
    });
    server.on("/api/brightness", HTTP_POST, []()
    {
        if (!validateControl("value")) return;
        const String value = server.arg("value");
        for (uint16_t i = 0; i <= MAX_ALLOWED_BRIGHTNESS; ++i)
        {
            if (value == String(i))
            {
                DisplayControl::setBrightness(i);
                sendStatus();
                return;
            }
        }
        Serial.println("[HTTP] Rejected brightness value.");
        sendJson(400, "{\"error\":\"value must be a canonical integer from 0 to the safe brightness ceiling\"}");
    });
    server.onNotFound([]()
    {
        sendJson(404, "{\"error\":\"Route or method not supported\"}");
    });
}

void WebDashboard::update()
{
    const bool connected = WiFi.status() == WL_CONNECTED &&
                           WiFi.localIP() != IPAddress(0, 0, 0, 0);
    if (!connected)
    {
        if (running) stop();
        return;
    }
    if (!running)
    {
        server.begin();
        running = true;
        Serial.printf("[HTTP] Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
    }
    server.handleClient();
}

void WebDashboard::stop()
{
    if (running)
    {
        server.stop();
        running = false;
        Serial.println("[HTTP] Dashboard stopped.");
    }
}
