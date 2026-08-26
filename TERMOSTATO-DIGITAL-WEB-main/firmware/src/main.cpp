#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include "config.h"

// Instâncias
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);
Preferences preferences;

// Variáveis de Estado do Sistema
float currentTemp = 0.0;
float currentHumidity = 0.0;
bool isSensorValid = false;

float targetTemp = 25.0;
float hysteresis = 1.0;
String systemMode = "AUTO_COOL"; // AUTO_COOL, AUTO_HEAT, MANUAL_ON, MANUAL_OFF
bool relayState = false;

// Timers de controle
unsigned long lastSensorRead = 0;
unsigned long lastSupabaseSync = 0;
unsigned long lastSupabaseLog = 0;

// Protótipos de Funções
void setupWiFi();
void setupLocalWebServer();
void readSensor();
void processThermostatLogic();
void setRelay(bool state);
void loadSavedPreferences();
void savePreferences();
void syncWithSupabase();
void logTelemetryToSupabase();

// ==============================================================================
// SETUP INICIAL
// ==============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- [ TERMOSTATO DIGITAL ESP32 INICIANDO ] ---");

    // Configurar pino do Relé
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(false);

    // Inicializar Sensor DHT22
    dht.begin();
    Serial.println("[DHT22] Sensor inicializado no GPIO " + String(DHT_PIN));

    // Carregar configurações da memória Flash (NVS)
    loadSavedPreferences();

    // Iniciar Wi-Fi
    setupWiFi();

    // Iniciar Servidor Web Local (Controle Offline/Local)
    setupLocalWebServer();

    Serial.println("[SISTEMA] Inicialização concluída com sucesso.");
}

// ==============================================================================
// LOOP PRINCIPAL
// ==============================================================================
void loop() {
    // 1. Processar requisições do servidor web local
    server.handleClient();

    unsigned long currentMillis = millis();

    // 2. Leitura periódica do sensor DHT22
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
        lastSensorRead = currentMillis;
        readSensor();
        processThermostatLogic();
    }

    // 3. Comunicação Online com Supabase (Se conectado ao Wi-Fi)
    if (WiFi.status() == WL_CONNECTED) {
        if (currentMillis - lastSupabaseSync >= SUPABASE_SYNC_INTERVAL_MS) {
            lastSupabaseSync = currentMillis;
            syncWithSupabase();
        }

        if (currentMillis - lastSupabaseLog >= SUPABASE_LOG_INTERVAL_MS) {
            lastSupabaseLog = currentMillis;
            logTelemetryToSupabase();
        }
    }
}

// ==============================================================================
// LEITURA DO DHT22
// ==============================================================================
void readSensor() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
        Serial.println("[DHT22 ERRO] Falha ao ler dados do sensor DHT22!");
        isSensorValid = false;
    } else {
        currentTemp = t;
        currentHumidity = h;
        isSensorValid = true;
        Serial.printf("[SENSOR] Temp: %.1f C | Umid: %.1f %% | Relé: %s\n", 
                      currentTemp, currentHumidity, relayState ? "LIGADO" : "DESLIGADO");
    }
}

// ==============================================================================
// LÓGICA DE CONTROLE DO TERMOSTATO (OFFLINE FIRST)
// ==============================================================================
void processThermostatLogic() {
    if (systemMode == "MANUAL_ON") {
        setRelay(true);
        return;
    }
    if (systemMode == "MANUAL_OFF") {
        setRelay(false);
        return;
    }

    // Se o sensor falhar em modo automático, desliga relé por segurança
    if (!isSensorValid) {
        setRelay(false);
        return;
    }

    if (systemMode == "AUTO_COOL") {
        // Modo Resfriamento (Ex: Ar-condicionado / Geladeira / Cooler)
        if (currentTemp >= (targetTemp + hysteresis)) {
            setRelay(true);
        } else if (currentTemp <= (targetTemp - hysteresis)) {
            setRelay(false);
        }
    } else if (systemMode == "AUTO_HEAT") {
        // Modo Aquecimento (Ex: Chocadeira / Estufa / Aquecedor)
        if (currentTemp <= (targetTemp - hysteresis)) {
            setRelay(true);
        } else if (currentTemp >= (targetTemp + hysteresis)) {
            setRelay(false);
        }
    }
}

void setRelay(bool state) {
    relayState = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

// ==============================================================================
// MEMÓRIA NÃO-VOLÁTIL (PREFERENCES / FLASH NVS)
// ==============================================================================
void loadSavedPreferences() {
    preferences.begin("thermostat", true);
    targetTemp = preferences.getFloat("target_temp", 25.0);
    hysteresis = preferences.getFloat("hysteresis", 1.0);
    systemMode = preferences.getString("mode", "AUTO_COOL");
    preferences.end();

    Serial.printf("[NVS] Carregado: Alvo: %.1f C | Histerese: %.1f C | Modo: %s\n", 
                  targetTemp, hysteresis, systemMode.c_str());
}

void savePreferences() {
    preferences.begin("thermostat", false);
    preferences.putFloat("target_temp", targetTemp);
    preferences.putFloat("hysteresis", hysteresis);
    preferences.putString("mode", systemMode);
    preferences.end();
    Serial.println("[NVS] Configurações salvas na memória interna.");
}

// ==============================================================================
// WI-FI & SERVIDOR WEB LOCAL (PAINEL OFFLINE)
// ==============================================================================
void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WIFI] Conectando a " + String(WIFI_SSID));

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Conectado! Endereço IP Local: " + WiFi.localIP().toString());
    } else {
        Serial.println("\n[WIFI AVISO] Não foi possível conectar ao Wi-Fi. Operando 100% Offline.");
    }
}

const char LOCAL_HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Termostato Local (Offline)</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; display: flex; justify-content: center; }
        .card { background: #1e293b; border-radius: 16px; padding: 24px; max-width: 420px; width: 100%; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
        h1 { font-size: 20px; text-align: center; margin-bottom: 20px; color: #38bdf8; }
        .metrics { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 20px; }
        .metric-box { background: #334155; padding: 16px; border-radius: 12px; text-align: center; }
        .metric-val { font-size: 26px; font-weight: bold; }
        .metric-label { font-size: 12px; color: #94a3b8; text-transform: uppercase; margin-top: 4px; }
        .relay-status { text-align: center; padding: 10px; border-radius: 8px; font-weight: bold; margin-bottom: 20px; }
        .relay-on { background: #16a34a; color: white; }
        .relay-off { background: #475569; color: #cbd5e1; }
        .form-group { margin-bottom: 15px; }
        label { display: block; font-size: 13px; color: #cbd5e1; margin-bottom: 6px; }
        input, select { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid #475569; background: #0f172a; color: white; box-sizing: border-box; font-size: 15px; }
        button { width: 100%; padding: 12px; background: #0284c7; color: white; border: none; border-radius: 8px; font-size: 16px; font-weight: bold; cursor: pointer; transition: 0.2s; }
        button:hover { background: #0369a1; }
        .badge { display: inline-block; padding: 4px 8px; border-radius: 6px; font-size: 11px; background: #fbbf24; color: #78350f; font-weight: bold; float: right; }
    </style>
</head>
<body>
    <div class="card">
        <div><span class="badge">PAINEL LOCAL OFFLINE</span></div>
        <h1>Termostato Digital</h1>
        <div class="metrics">
            <div class="metric-box">
                <div class="metric-val" id="temp">-- &deg;C</div>
                <div class="metric-label">Temperatura</div>
            </div>
            <div class="metric-box">
                <div class="metric-val" id="umid">-- %</div>
                <div class="metric-label">Umidade</div>
            </div>
        </div>
        <div id="relayStatus" class="relay-status relay-off">REL&Eacute;: DESLIGADO</div>
        <form action="/set" method="POST">
            <div class="form-group">
                <label>Temperatura Alvo (&deg;C)</label>
                <input type="number" step="0.5" name="target" id="inputTarget" required>
            </div>
            <div class="form-group">
                <label>Histerese (&plusmn;&deg;C)</label>
                <input type="number" step="0.1" name="hysteresis" id="inputHyst" required>
            </div>
            <div class="form-group">
                <label>Modo de Opera&ccedil;&atilde;o</label>
                <select name="mode" id="selectMode">
                    <option value="AUTO_COOL">Autom&aacute;tico: Resfriar (Cool)</option>
                    <option value="AUTO_HEAT">Autom&aacute;tico: Aquecer (Heat)</option>
                    <option value="MANUAL_ON">Manual: Ligado (For&ccedil;ar ON)</option>
                    <option value="MANUAL_OFF">Manual: Desligado (For&ccedil;ar OFF)</option>
                </select>
            </div>
            <button type="submit">Salvar Configura&ccedil;&otilde;es</button>
        </form>
    </div>
    <script>
        function updateData() {
            fetch('/api/status').then(r => r.json()).then(data => {
                document.getElementById('temp').innerHTML = data.temp.toFixed(1) + ' &deg;C';
                document.getElementById('umid').innerHTML = data.umid.toFixed(1) + ' %';
                document.getElementById('inputTarget').value = data.target;
                document.getElementById('inputHyst').value = data.hysteresis;
                document.getElementById('selectMode').value = data.mode;
                const rBox = document.getElementById('relayStatus');
                if(data.relay) {
                    rBox.className = 'relay-status relay-on';
                    rBox.innerText = 'RELÉ: LIGADO (ATIVO)';
                } else {
                    rBox.className = 'relay-status relay-off';
                    rBox.innerText = 'RELÉ: DESLIGADO';
                }
            }).catch(e => console.error(e));
        }
        updateData();
        setInterval(updateData, 3000);
    </script>
</body>
</html>
)rawliteral";

void setupLocalWebServer() {
    // Página principal
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", LOCAL_HTML_PAGE);
    });

    // Endpoint JSON de status
    server.on("/api/status", HTTP_GET, []() {
        JsonDocument doc;
        doc["temp"] = currentTemp;
        doc["umid"] = currentHumidity;
        doc["target"] = targetTemp;
        doc["hysteresis"] = hysteresis;
        doc["mode"] = systemMode;
        doc["relay"] = relayState;

        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });

    // Endpoint POST para salvar configurações
    server.on("/set", HTTP_POST, []() {
        if (server.hasArg("target")) targetTemp = server.arg("target").toFloat();
        if (server.hasArg("hysteresis")) hysteresis = server.arg("hysteresis").toFloat();
        if (server.hasArg("mode")) systemMode = server.arg("mode");

        savePreferences();
        processThermostatLogic();

        server.sendHeader("Location", "/");
        server.send(303);
    });

    server.begin();
    Serial.println("[HTTP] Servidor Web Local iniciado na porta 80");
}

// ==============================================================================
// COMUNICAÇÃO ONLINE VIA SUPABASE REST API
// ==============================================================================
void syncWithSupabase() {
    if (String(SUPABASE_URL) == "https://SEU_PROJETO.supabase.co") {
        return; // Não configurado ainda
    }

    WiFiClientSecure client;
    client.setInsecure(); // Para conexões HTTPS sem necessidade de carregar certificado raiz
    HTTPClient https;

    // 1. GET - Buscar configurações atualizadas do Supabase
    String getUrl = String(SUPABASE_URL) + "/rest/v1/thermostat_config?id=eq.main_thermostat&select=*";
    if (https.begin(client, getUrl)) {
        https.addHeader("apikey", SUPABASE_KEY);
        https.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);

        int httpCode = https.GET();
        if (httpCode == 200) {
            String payload = https.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, payload);
            if (!err && doc.is<JsonArray>() && doc.as<JsonArray>().size() > 0) {
                JsonObject config = doc[0];
                float remoteTarget = config["target_temp"] | targetTemp;
                float remoteHyst = config["hysteresis"] | hysteresis;
                String remoteMode = config["mode"] | systemMode;

                // Se houver alteração remota, atualiza e salva
                if (remoteTarget != targetTemp || remoteHyst != hysteresis || remoteMode != systemMode) {
                    targetTemp = remoteTarget;
                    hysteresis = remoteHyst;
                    systemMode = remoteMode;
                    savePreferences();
                    processThermostatLogic();
                    Serial.println("[SUPABASE] Configurações sincronizadas da nuvem.");
                }
            }
        }
        https.end();
    }

    // 2. PATCH - Atualizar estado atual e telemetria em tempo real
    String patchUrl = String(SUPABASE_URL) + "/rest/v1/thermostat_config?id=eq.main_thermostat";
    if (https.begin(client, patchUrl)) {
        https.addHeader("apikey", SUPABASE_KEY);
        https.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
        https.addHeader("Content-Type", "application/json");

        JsonDocument patchDoc;
        patchDoc["current_temp"] = currentTemp;
        patchDoc["current_humidity"] = currentHumidity;
        patchDoc["relay_state"] = relayState;
        patchDoc["last_seen"] = "now()";

        String jsonPatch;
        serializeJson(patchDoc, jsonPatch);

        https.sendRequest("PATCH", jsonPatch);
        https.end();
    }
}

void logTelemetryToSupabase() {
    if (!isSensorValid || String(SUPABASE_URL) == "https://SEU_PROJETO.supabase.co") {
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    String postUrl = String(SUPABASE_URL) + "/rest/v1/thermostat_logs";
    if (https.begin(client, postUrl)) {
        https.addHeader("apikey", SUPABASE_KEY);
        https.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
        https.addHeader("Content-Type", "application/json");

        JsonDocument logDoc;
        logDoc["temperature"] = currentTemp;
        logDoc["humidity"] = currentHumidity;
        logDoc["relay_state"] = relayState;

        String jsonLog;
        serializeJson(logDoc, jsonLog);

        https.POST(jsonLog);
        https.end();
        Serial.println("[SUPABASE] Histórico de telemetria registrado.");
    }
}
