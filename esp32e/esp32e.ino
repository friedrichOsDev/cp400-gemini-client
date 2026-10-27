#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h" // provides SSID, PASSWD, GEMINI_API_KEY

// WLAN & API Zugangsdaten
const char* ssid = SSID;
const char* password = PASSWD;
const char* gemini_api_key = GEMINI_API_KEY;

// UART2: Verbindung zum Casio
#define RXD2 16
#define TXD2 17
#define UART_BAUD 115200

#define LED_PIN 2

// Netzwerk-Timeouts
#define WIFI_TIMEOUT_MS 30000
#define HTTP_CONNECT_TIMEOUT_MS 15000
#define HTTP_READ_TIMEOUT_MS 60000

static const char* GEMINI_SYSTEM_PROMPT =
    "You are an AI chatbot running INSIDE a Casio fx-CP400. "
    "Role: Chat, entertain, and answer general questions. "
    "CRITICAL: Do NOT explain how to use the calculator. "
    "CRITICAL: Output ONLY plain ASCII (32-126). NO Unicode, NO Markdown, NO emojis. "
    "Convert German umlauts/sharp-s: ae, oe, ue, ss (e.g., Erklaerung). "
    "MAX LIMIT: 1024 chars. Be brief. Use short, single-line sentences. "
    "Compact ASCII-only code if requested.";

void ledBlink(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_PIN, LOW);
        delay(delayMs);
    }
}

String sanitizeAscii(const String& input) {
    String output;
    output.reserve(input.length());

    for (size_t i = 0; i < input.length(); ) {
        const uint8_t c = static_cast<uint8_t>(input[i]);

        if (c >= 32 && c <= 126) {
            output += static_cast<char>(c);
            ++i;
            continue;
        }

        if (i + 1 < input.length()) {
            const uint8_t c2 = static_cast<uint8_t>(input[i + 1]);
            if (c == 0xC3) {
                switch (c2) {
                    case 0xA4: output += "ae"; i += 2; continue; 
                    case 0x84: output += "Ae"; i += 2; continue; 
                    case 0xB6: output += "oe"; i += 2; continue; 
                    case 0x96: output += "Oe"; i += 2; continue; 
                    case 0xBC: output += "ue"; i += 2; continue; 
                    case 0x9C: output += "Ue"; i += 2; continue; 
                    case 0x9F: output += "ss"; i += 2; continue; 
                    case 0xA9: output += "e";  i += 2; continue; 
                    case 0xA0: output += "a";  i += 2; continue; 
                    case 0xA8: output += "e";  i += 2; continue; 
                    case 0xB1: output += "n";  i += 2; continue; 
                    default: break;
                }
            }
            if (c == 0xE2 && i + 2 < input.length()) {
                const uint8_t c3 = static_cast<uint8_t>(input[i + 2]);
                if (c2 == 0x80 && (c3 == 0x93 || c3 == 0x94)) {
                    output += '-'; i += 3; continue; 
                }
                if (c2 == 0x80 && c3 == 0x99) {
                    output += '\''; i += 3; continue; 
                }
                if (c2 == 0x80 && c3 == 0x9C) {
                    output += '"'; i += 3; continue; 
                }
                if (c2 == 0x80 && c3 == 0x9D) {
                    output += '"'; i += 3; continue; 
                }
                if (c2 == 0x80 && c3 == 0xA6) {
                    output += "..."; i += 3; continue; 
                }
            }
        }

        if (c == '\n' || c == '\r' || c == '\t') {
            output += ' ';
        }
        ++i;
    }

    output.trim();
    return output;
}

void connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("[ESP32] Verbinde mit WLAN");

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_TIMEOUT_MS) {
        digitalWrite(LED_PIN, HIGH);
        delay(250);
        digitalWrite(LED_PIN, LOW);
        delay(250);
        Serial.print(".");
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[ESP32] WLAN verbunden!");
        Serial.print("[ESP32] IP-Adresse: ");
        Serial.println(WiFi.localIP());
        
        digitalWrite(LED_PIN, HIGH);
        delay(2000);
        digitalWrite(LED_PIN, LOW);
    } else {
        Serial.println("[ESP32] WLAN-Verbindung fehlgeschlagen.");
        ledBlink(10, 100);
    }
}

String callGemini(const String& prompt) {
    if (WiFi.status() != WL_CONNECTED) {
        return "Fehler: WLAN getrennt";
    }

    if (gemini_api_key[0] == '\0') {
        return "Fehler: API-Key fehlt";
    }

    digitalWrite(LED_PIN, HIGH);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    String url =
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-3.5-flash-lite:generateContent?key=" +
        String(gemini_api_key);

    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
    http.setTimeout(HTTP_READ_TIMEOUT_MS);
    http.setReuse(false);

    if (!http.begin(client, url)) {
        digitalWrite(LED_PIN, LOW);
        return "Fehler: HTTP Initialisierung";
    }

    http.addHeader("Content-Type", "application/json");

    JsonDocument requestDoc;

    requestDoc["system_instruction"]["parts"][0]["text"] =
        GEMINI_SYSTEM_PROMPT;
    requestDoc["contents"][0]["parts"][0]["text"] = prompt;

    // requestDoc["generationConfig"]["thinkingConfig"]["thinkingBudget"] = 0;
    requestDoc["generationConfig"]["maxOutputTokens"] = 256;

    String payload;
    serializeJson(requestDoc, payload);

    Serial.println("[Gemini] Sende Anfrage...");

    int status = http.POST(payload);

    Serial.printf("[HTTP Status]: %d\n", status);

    String response;

    if (status > 0) {
        response = http.getString();
        Serial.println("[API Rohantwort]:");
        Serial.println(response);
    } else {
        String error = HTTPClient::errorToString(status);
        Serial.printf("[HTTP Fehler]: %s\n", error.c_str());
        http.end();
        digitalWrite(LED_PIN, LOW);
        return "HTTP Fehler: " + String(status);
    }

    http.end();

    JsonDocument responseDoc;

    DeserializationError jsonError =
        deserializeJson(responseDoc, response);

    if (jsonError) {
        digitalWrite(LED_PIN, LOW);
        return "JSON Fehler: " + String(jsonError.c_str());
    }

    if (status < 200 || status >= 300) {
        const char* message =
            responseDoc["error"]["message"] | "Unbekannter API-Fehler";

        Serial.printf("[Gemini API Fehler]: %s\n", message);
        digitalWrite(LED_PIN, LOW);
        return "API " + String(status) + ": " + String(message);
    }

    JsonArray candidates =
        responseDoc["candidates"].as<JsonArray>();

    if (candidates.isNull() || candidates.size() == 0) {
        const char* reason =
            responseDoc["promptFeedback"]["blockReason"];

        digitalWrite(LED_PIN, LOW);
        if (reason != nullptr) {
            return "Anfrage blockiert: " + String(reason);
        }

        return "Keine Gemini-Antwort";
    }

    JsonArray parts =
        candidates[0]["content"]["parts"].as<JsonArray>();

    String answer;

    for (JsonVariant part : parts) {
        const char* text = part["text"];

        if (text != nullptr) {
            if (!answer.isEmpty()) {
                answer += " ";
            }

            answer += text;
        }
    }

    if (answer.isEmpty()) {
        const char* finishReason =
            candidates[0]["finishReason"] | "UNKNOWN";

        digitalWrite(LED_PIN, LOW);
        return "Keine Textantwort: " + String(finishReason);
    }

    answer.replace("\r", " ");
    answer.replace("\n", " ");
    answer.replace("\t", " ");
    answer = sanitizeAscii(answer);

    if (answer.length() > 450) {
        answer = answer.substring(0, 447) + "...";
    }

    digitalWrite(LED_PIN, LOW);
    return answer;
}

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Serial.begin(115200);
    delay(1000);

    Serial2.begin(
        UART_BAUD,
        SERIAL_8N1,
        RXD2,
        TXD2
    );

    Serial2.setTimeout(30000);

    connectWiFi();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[ESP32] WLAN getrennt. Verbinde erneut...");
        WiFi.disconnect();
        connectWiFi();

        if (WiFi.status() != WL_CONNECTED) {
            delay(3000);
            return;
        }
    }

    if (!Serial2.available()) {
        delay(10);
        return;
    }

    String input = Serial2.readStringUntil('\n');
    input.trim();

    if (input.isEmpty()) {
        return;
    }

    Serial.println("[Casio Anfrage]: " + input);

    ledBlink(2, 50);

    String answer = callGemini(input);

    Serial.println("[Gemini Antwort]: " + answer);

    Serial.println("[UART TX]: Sende Antwort an Casio");
    Serial2.println(answer);
    Serial2.flush();

    ledBlink(2, 150);
}