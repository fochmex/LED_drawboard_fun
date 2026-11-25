#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

#include "DashboardPage.h"
#include "EffectControl.h"
#include "ClockControl.h"
#include "DisplayControl.h"
#include "ProjectConfig.h"
#include "WebDashboard.h"
#include "TetrisControl.h"
#include "ScrollingTextControl.h"

namespace
{
    WebServer server(80);
    WebSocketsServer webSocket(81);
    bool running = false;
    bool webSocketRunning = false;
    String lastStatus;
    String lastFrame;


    void sendJson(int code, const String& body)
    {
        server.sendHeader("Cache-Control", "no-store");
        server.sendHeader("X-Content-Type-Options", "nosniff");
        server.send(code, "application/json", body);
    }

    String buildStatus()
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
        body += ",\"clockAvailable\":";
        body += ClockControl::available() ? "true" : "false";
        body += ",\"clockMode\":";
        body += ClockControl::active() ? "true" : "false";
        body += ",\"clockStyle\":\"";
        body += ClockControl::style() == ClockControl::Style::kBinary ? "binary" : "analog";
        body += '"';
        body += ",\"clockTime\":\"";
        struct tm localTime;
        if (ClockControl::readLocalTime(localTime))
        {
            char timeText[9];
            strftime(timeText, sizeof(timeText), "%H:%M:%S", &localTime);
            body += timeText;
        }
        body += '"';
        body += ",\"tetrisActive\":";
        body += TetrisControl::active() ? "true" : "false";
        body += ",\"tetrisPaused\":";
        body += TetrisControl::paused() ? "true" : "false";
        body += ",\"tetrisGameOver\":";
        body += TetrisControl::gameOver() ? "true" : "false";
        body += ",\"tetrisScore\":" + String(TetrisControl::score());
        body += ",\"tetrisLines\":" + String(TetrisControl::lines());
        body += ",\"tetrisLevel\":" + String(TetrisControl::level());
        body += ",\"tetrisNext\":" + String(TetrisControl::nextPieceType());
        body += ",\"textActive\":";
        body += ScrollingTextControl::active() ? "true" : "false";
        body += ",\"text\":";
        body += "\"";
        for (const char* p = ScrollingTextControl::text(); *p != '\0'; ++p)
        {
            if (*p == '"' || *p == '\\') body += '\\';
            body += *p;
        }
        body += '"';
        body += ",\"textColor\":";
        char colorHex[7]; snprintf(colorHex, sizeof(colorHex), "%06x", (unsigned)(ScrollingTextControl::textColor() & 0xffffff));
        body += "\"#" + String(colorHex) + '"';
        snprintf(colorHex, sizeof(colorHex), "%06x", (unsigned)(ScrollingTextControl::backgroundColor() & 0xffffff));
        body += ",\"textBackground\":" + String("\"#") + String(colorHex) + '"';
        body += ",\"textFont\":" + String(ScrollingTextControl::font());
        body += ",\"textDirection\":";
        body += ScrollingTextControl::scrollLeft() ? "\"left\"" : "\"right\"";
        body += ",\"textSpeed\":" + String(ScrollingTextControl::speed());
        body += ",\"textBefore\":" + String(ScrollingTextControl::beforePause());
        body += ",\"textAfter\":" + String(ScrollingTextControl::afterPause());
        body += ",\"textRepeat\":";
        if (ScrollingTextControl::repeatMode() == ScrollingTextControl::RepeatMode::kOnce) body += "\"once\"";
        else if (ScrollingTextControl::repeatMode() == ScrollingTextControl::RepeatMode::kCount) body += "\"count\"";
        else body += "\"loop\"";
        body += ",\"textRepeatCount\":" + String(ScrollingTextControl::repeatCount());
        body += ",\"textDurationMs\":" + String(ScrollingTextControl::estimatedDurationMs());
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
        return body;
    }

    void sendStatus()
    {
        sendJson(200, buildStatus());
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

    void broadcastStatusNow()
    {
        if (!webSocketRunning || webSocket.connectedClients() == 0) return;
        String status = "S" + buildStatus();
        if (status == lastStatus) return;
        lastStatus = status;
        webSocket.broadcastTXT(status.c_str(), status.length());
    }

    void broadcastFrameNow()
    {
        if (!webSocketRunning || webSocket.connectedClients() == 0) return;
        char pixels[static_cast<size_t>(PIXEL_COUNT) * 6U + 1U];
        if (!DisplayControl::frameHex(pixels, sizeof(pixels))) return;
        String frame = "F";
        frame += pixels;
        if (frame == lastFrame) return;
        lastFrame = frame;
        webSocket.broadcastTXT(frame.c_str(), frame.length());
    }

    void handleWebSocketEvent(uint8_t client, WStype_t type,
                             uint8_t* payload, size_t length)
    {
        if (type == WStype_CONNECTED)
        {
            String status = "S" + buildStatus();
            webSocket.sendTXT(client, status.c_str(), status.length());
            char pixels[static_cast<size_t>(PIXEL_COUNT) * 6U + 1U];
            if (DisplayControl::frameHex(pixels, sizeof(pixels)))
            {
                String frame = "F";
                frame += pixels;
                webSocket.sendTXT(client, frame.c_str(), frame.length());
            }
            Serial.printf("[WS] Client %u connected.\n", client);
        }
        else if (type == WStype_DISCONNECTED)
        {
            Serial.printf("[WS] Client %u disconnected.\n", client);
        }
        else if (type == WStype_TEXT && payload != nullptr)
        {
            char text[length + 1U];
            memcpy(text, payload, length);
            text[length] = '\0';
            String message(text);

            if (message.startsWith("ping:"))
            {
                String pong = "P";
                pong += message.substring(5);
                webSocket.sendTXT(client, pong.c_str(), pong.length());
            }
            else if (message.startsWith("effect:"))
            {
                const String value = message.substring(7);
                for (uint8_t i = 0; i < EffectControl::count(); ++i)
                {
                    if (value == String(i))
                    {
                        EffectControl::select(i);
                        broadcastStatusNow();
                        broadcastFrameNow();
                        break;
                    }
                }
            }
            else if (message == "playlist:true" || message == "playlist:false")
            {
                EffectControl::setPlaylistEnabled(message.endsWith("true"));
            }
            else if (message.startsWith("brightness:"))
            {
                const String value = message.substring(11);
                char* end = nullptr;
                const long brightness = strtol(value.c_str(), &end, 10);
                if (end != value.c_str() && *end == '\0' && brightness >= 0 &&
                    brightness <= MAX_ALLOWED_BRIGHTNESS)
                {
                    DisplayControl::setBrightness(static_cast<uint16_t>(brightness));
                }
            }
            else if (message.startsWith("tetris:"))
            {
                const String command = message.substring(7);
                if (command == "start" || command == "restart")
                {
                    if (ClockControl::active()) ClockControl::setActive(false);
                    DisplayControl::leaveDrawingMode();
                    if (command == "restart") TetrisControl::restart();
                    else TetrisControl::start();
                    EffectControl::setPlaylistEnabled(false);
                }
                else if (command == "pause") TetrisControl::togglePause();
                else if (command == "left") TetrisControl::moveLeft();
                else if (command == "right") TetrisControl::moveRight();
                else if (command == "rotate") TetrisControl::rotate();
                else if (command == "soft") TetrisControl::softDrop();
                else if (command == "hard") TetrisControl::hardDrop();
                broadcastStatusNow();
                broadcastFrameNow();
            }
            else if (message.startsWith("text:"))
            {
                const String command = message.substring(5);
                if (command == "start")
                {
                    TetrisControl::stop();
                    ClockControl::setActive(false);
                    DisplayControl::leaveDrawingMode();
                    EffectControl::setPlaylistEnabled(false);
                    ScrollingTextControl::start();
                }
                else if (command == "stop") ScrollingTextControl::stop();
                else if (command == "clear") ScrollingTextControl::clear();
                broadcastStatusNow();
                broadcastFrameNow();
            }
            else if (message.startsWith("frame:"))
            {
                const String pixels = message.substring(6);
                if (!DisplayControl::applyFrame(pixels.c_str(), pixels.length()))
                {
                    webSocket.sendTXT(client, "EInvalid frame payload", 21);
                }
            }
        }
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
    server.on("/api/clock", HTTP_POST, []()
    {
        if (!validateControl("enabled")) return;
        const String enabled = server.arg("enabled");
        if (enabled != "true" && enabled != "false")
        {
            sendJson(400, "{\"error\":\"enabled must be true or false\"}");
            return;
        }
        if (enabled == "true" && !ClockControl::available())
        {
            sendJson(503, "{\"error\":\"Clock time is not synchronized yet\"}");
            return;
        }
        if (!ClockControl::setActive(enabled == "true"))
        {
            sendJson(503, "{\"error\":\"Clock mode is unavailable until time synchronization completes\"}");
            return;
        }
        sendStatus();
    });
    server.on("/api/clock-style", HTTP_POST, []()
    {
        if (!validateControl("style")) return;
        const String style = server.arg("style");
        if (style != "analog" && style != "binary")
        {
            sendJson(400, "{\"error\":\"style must be analog or binary\"}");
            return;
        }
        if (!ClockControl::available())
        {
            sendJson(503, "{\"error\":\"Clock time is not synchronized yet\"}");
            return;
        }
        const ClockControl::Style selectedStyle = style == "binary"
            ? ClockControl::Style::kBinary : ClockControl::Style::kAnalog;
        if (!ClockControl::setStyle(selectedStyle))
        {
            sendJson(503, "{\"error\":\"Clock style could not be changed\"}");
            return;
        }
        sendStatus();
    });
    server.on("/api/tetris", HTTP_POST, []()
    {
        if (!validateControl("command")) return;
        const String command = server.arg("command");
        if (command == "start" || command == "restart")
        {
            if (ClockControl::active()) ClockControl::setActive(false);
            DisplayControl::leaveDrawingMode();
            if (command == "restart") TetrisControl::restart();
            else TetrisControl::start();
            EffectControl::setPlaylistEnabled(false);
        }
        else if (command == "pause") TetrisControl::togglePause();
        else if (command == "left") TetrisControl::moveLeft();
        else if (command == "right") TetrisControl::moveRight();
        else if (command == "rotate") TetrisControl::rotate();
        else if (command == "soft") TetrisControl::softDrop();
        else if (command == "hard") TetrisControl::hardDrop();
        else
        {
            sendJson(400, "{\"error\":\"Unknown Tetris command\"}");
            return;
        }
        sendStatus();
    });
    server.on("/api/text", HTTP_POST, []()
    {
        if (server.header("X-Drawboard-Control") != "1")
        {
            sendJson(403, "{\"error\":\"Missing X-Drawboard-Control: 1 header\"}");
            return;
        }
        if (server.hasArg("command"))
        {
            const String command = server.arg("command");
            if (command == "start")
            {
                TetrisControl::stop();
                if (ClockControl::active()) ClockControl::setActive(false);
                DisplayControl::leaveDrawingMode();
                EffectControl::setPlaylistEnabled(false);
                ScrollingTextControl::start();
            }
            else if (command == "stop")
            {
                ScrollingTextControl::stop();
            }
            else if (command == "clear")
            {
                ScrollingTextControl::clear();
            }
            else
            {
                sendJson(400, "{\"error\":\"Unknown text command\"}");
                return;
            }
            sendStatus();
            return;
        }
        if (!server.hasArg("text") || server.arg("text").length() > 64)
        {
            sendJson(400, "{\"error\":\"text is required and limited to 64 characters\"}");
            return;
        }
        const auto hexColor = [](const String& value, uint32_t& output) -> bool
        {
            if (value.length() != 7 || value[0] != '#') return false;
            output = 0;
            for (uint8_t i = 1; i < 7; ++i)
            {
                char c = value[i];
                uint8_t digit;
                if (c >= '0' && c <= '9') digit = c - '0';
                else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
                else return false;
                output = (output << 4) | digit;
            }
            return true;
        };
        uint32_t textColor = 0x75e4ca;
        uint32_t backgroundColor = 0x000000;
        if (server.hasArg("textColor") && !hexColor(server.arg("textColor"), textColor))
        {
            sendJson(400, "{\"error\":\"textColor must be #RRGGBB\"}");
            return;
        }
        if (server.hasArg("backgroundColor") && !hexColor(server.arg("backgroundColor"), backgroundColor))
        {
            sendJson(400, "{\"error\":\"backgroundColor must be #RRGGBB\"}");
            return;
        }
        uint8_t font = 1;
        if (server.hasArg("font"))
        {
            const String value = server.arg("font");
            if (value == "compact") font = 0;
            else if (value == "normal") font = 1;
            else if (value == "large") font = 2;
            else
            {
                sendJson(400, "{\"error\":\"font must be compact, normal, or large\"}");
                return;
            }
        }
        bool left = true;
        if (server.hasArg("direction"))
        {
            const String value = server.arg("direction");
            if (value == "right") left = false;
            else if (value != "left")
            {
                sendJson(400, "{\"error\":\"direction must be left or right\"}");
                return;
            }
        }
        char* end = nullptr;
        uint8_t speed = server.hasArg("speed") ? static_cast<uint8_t>(strtol(server.arg("speed").c_str(), &end, 10)) : 5;
        if (server.hasArg("speed") && (*end != '\0' || speed < 1 || speed > 10))
        {
            sendJson(400, "{\"error\":\"speed must be 1 through 10\"}");
            return;
        }
        uint16_t beforePause = server.hasArg("before") ? static_cast<uint16_t>(strtoul(server.arg("before").c_str(), &end, 10)) : 500;
        if (server.hasArg("before") && (*end != '\0' || beforePause > 5000))
        {
            sendJson(400, "{\"error\":\"before must be 0 through 5000 ms\"}");
            return;
        }
        uint16_t afterPause = server.hasArg("after") ? static_cast<uint16_t>(strtoul(server.arg("after").c_str(), &end, 10)) : 500;
        if (server.hasArg("after") && (*end != '\0' || afterPause > 5000))
        {
            sendJson(400, "{\"error\":\"after must be 0 through 5000 ms\"}");
            return;
        }
        ScrollingTextControl::RepeatMode repeat = ScrollingTextControl::RepeatMode::kLoop;
        uint8_t repeatCount = 1;
        if (server.hasArg("repeat"))
        {
            const String value = server.arg("repeat");
            if (value == "once") repeat = ScrollingTextControl::RepeatMode::kOnce;
            else if (value == "count") repeat = ScrollingTextControl::RepeatMode::kCount;
            else if (value == "loop") repeat = ScrollingTextControl::RepeatMode::kLoop;
            else
            {
                sendJson(400, "{\"error\":\"repeat must be once, count, or loop\"}");
                return;
            }
        }
        if (server.hasArg("repeatCount"))
        {
            repeatCount = static_cast<uint8_t>(strtol(server.arg("repeatCount").c_str(), &end, 10));
            if (*end != '\0' || repeatCount < 1 || repeatCount > 20)
            {
                sendJson(400, "{\"error\":\"repeatCount must be 1 through 20\"}");
                return;
            }
        }
        ScrollingTextControl::setText(server.arg("text").c_str());
        ScrollingTextControl::setColors(textColor, backgroundColor);
        ScrollingTextControl::setFont(font);
        ScrollingTextControl::setDirection(left);
        ScrollingTextControl::setSpeed(speed);
        ScrollingTextControl::setPause(beforePause, afterPause);
        ScrollingTextControl::setRepeat(repeat, repeatCount);
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
    if (!webSocketRunning)
    {
        webSocket.begin();
        webSocket.onEvent(handleWebSocketEvent);
        webSocketRunning = true;
        Serial.printf("[WS] Live endpoint: ws://%s:81/\n", WiFi.localIP().toString().c_str());
    }
    webSocket.loop();
    server.handleClient();

    static uint32_t lastSyncMs = 0;
    const uint32_t nowMs = millis();
    if ((nowMs - lastSyncMs) >= 500U)
    {
        lastSyncMs = nowMs;
        broadcastStatusNow();
        broadcastFrameNow();
    }
}

void WebDashboard::stop()
{
    if (running)
    {
        server.stop();
        running = false;
        Serial.println("[HTTP] Dashboard stopped.");
    }
    if (webSocketRunning)
    {
        webSocket.close();
        webSocketRunning = false;
        Serial.println("[WS] Live communication stopped.");
    }
}

void WebDashboard::broadcastStatus()
{
    broadcastStatusNow();
}

void WebDashboard::broadcastFrame()
{
    broadcastFrameNow();
}
