#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <cstring>
#include <time.h>

#include "NetworkManager.h"
#include "ProjectConfig.h"
#include "WebDashboard.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

namespace
{
    enum class State
    {
        kDisabled,
        kWaiting,
        kConnecting,
        kConnected
    };

    State state = State::kDisabled;
    uint32_t stateStartMs = 0;
    uint32_t retryDelayMs = 0;
    uint32_t nextRetryDelayMs = WIFI_RETRY_INITIAL_MS;
    std::atomic<uint16_t> disconnectReason{0};

    const char* statusName(wl_status_t status)
    {
        switch (status)
        {
            case WL_IDLE_STATUS: return "idle";
            case WL_NO_SSID_AVAIL: return "network not found (check 2.4 GHz coverage)";
            case WL_SCAN_COMPLETED: return "scan complete";
            case WL_CONNECTED: return "connected";
            case WL_CONNECT_FAILED: return "connection failed (check credentials/router security)";
            case WL_CONNECTION_LOST: return "connection lost";
            case WL_DISCONNECTED: return "disconnected";
            case WL_NO_SHIELD: return "Wi-Fi unavailable";
            default: return "unknown";
        }
    }

    void scheduleRetry()
    {
        // Stop an unfinished attempt so it cannot continue through the backoff.
        WebDashboard::stop();
        if (!WiFi.mode(WIFI_OFF))
        {
            Serial.println("[WiFi] Failed to stop radio before retry.");
        }
        state = State::kWaiting;
        stateStartMs = millis();
        retryDelayMs = nextRetryDelayMs;
        nextRetryDelayMs = nextRetryDelayMs >= WIFI_RETRY_MAX_MS / 2U
            ? WIFI_RETRY_MAX_MS : nextRetryDelayMs * 2U;
        Serial.printf("[WiFi] Retrying in %lu ms; effects and button remain available.\n", retryDelayMs);
    }

    void startAttempt()
    {
        disconnectReason.store(0);
        Serial.printf("[WiFi] Connecting as %s (timeout %lu ms)...\n",
                      WIFI_HOSTNAME, WIFI_CONNECT_TIMEOUT_MS);
        if (!WiFi.setHostname(WIFI_HOSTNAME) || !WiFi.mode(WIFI_STA))
        {
            Serial.println("[WiFi] Cannot initialize station/hostname.");
            scheduleRetry();
            return;
        }

        state = State::kConnecting;
        stateStartMs = millis();
        const wl_status_t status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        if (status == WL_CONNECT_FAILED || status == WL_NO_SHIELD)
        {
            Serial.printf("[WiFi] Cannot start connection: %s.\n", statusName(status));
            scheduleRetry();
        }
    }
}

void NetworkManager::begin()
{
    const size_t ssidLength = std::strlen(WIFI_SSID);
    const size_t passwordLength = std::strlen(WIFI_PASSWORD);
    if (ssidLength == 0 || ssidLength > 32 || passwordLength > 64 ||
        std::strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 ||
        std::strcmp(WIFI_PASSWORD, "YOUR_WIFI_PASSWORD") == 0)
    {
        Serial.println("[WiFi] Invalid/missing credentials. Edit ignored include/secrets.h "
                       "(SSID 1-32 bytes, password up to 64 bytes). Effects continue offline.");
        return;
    }

    WiFi.persistent(false);
    if (!WiFi.setAutoReconnect(false))
    {
        Serial.println("[WiFi] Cannot disable automatic reconnect; networking disabled.");
        return;
    }
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info)
    {
        // Arduino dispatches events on another task; log only from update().
        disconnectReason.store(info.wifi_sta_disconnected.reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    startAttempt();
}

void NetworkManager::update()
{
    if (state == State::kDisabled)
    {
        return;
    }
    const uint32_t nowMs = millis();
    if (state == State::kWaiting)
    {
        if ((nowMs - stateStartMs) >= retryDelayMs)
        {
            startAttempt();
        }
        return;
    }

    const uint16_t reason = disconnectReason.exchange(0);
    if (reason != 0)
    {
        Serial.printf("[WiFi] Station disconnected, ESP-IDF reason %u "
                      "(check signal, credentials, and router settings).\n", reason);
    }

    const wl_status_t status = WiFi.status();
    const bool hasAddress = status == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
    if (hasAddress)
    {
        if (state != State::kConnected)
        {
            state = State::kConnected;
            nextRetryDelayMs = WIFI_RETRY_INITIAL_MS;
            configTzTime(CLOCK_TIMEZONE, "pool.ntp.org", "time.cloudflare.com");
            Serial.println("[Time] NTP synchronization started.");
            Serial.printf("[WiFi] Connected | hostname: %s | IP: %s | RSSI: %d dBm | MAC: %s\n",
                          WIFI_HOSTNAME, WiFi.localIP().toString().c_str(),
                          static_cast<int>(WiFi.RSSI()), WiFi.macAddress().c_str());
            Serial.println("[WiFi] Use this station MAC for a router DHCP reservation.");
        }
    }
    else if (state == State::kConnected)
    {
        Serial.printf("[WiFi] LAN connection lost: %s.\n", statusName(status));
        scheduleRetry();
    }
    else if ((nowMs - stateStartMs) >= WIFI_CONNECT_TIMEOUT_MS)
    {
        Serial.printf("[WiFi] Connection/IP timeout: %s. Check credentials, 2.4 GHz network, and DHCP.\n",
                      statusName(status));
        scheduleRetry();
    }
}

void NetworkManager::stop()
{
    state = State::kDisabled;
    if (WiFi.getMode() != WIFI_OFF)
    {
        Serial.println("[WiFi] Stopping radio for deep sleep.");
        if (!WiFi.mode(WIFI_OFF))
        {
            Serial.println("[WiFi] Radio stop failed; deep sleep will power it down.");
        }
    }
}
