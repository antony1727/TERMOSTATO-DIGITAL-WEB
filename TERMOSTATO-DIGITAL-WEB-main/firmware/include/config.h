#pragma once
#include <Arduino.h>

// ==============================================================================
// CONFIGURAÇÃO DE REDE WI-FI
// ==============================================================================
#define WIFI_SSID       "SEU_WIFI_NOME"      // Coloque aqui o nome da sua rede Wi-Fi 2.4GHz
#define WIFI_PASSWORD   "SUA_SENHA_WIFI"     // Coloque aqui a senha da sua rede Wi-Fi

// ==============================================================================
// CONFIGURAÇÃO DA API SUPABASE (ONLINE REST)
// ==============================================================================
#define SUPABASE_URL    "https://dejascgqmovkdbytujde.supabase.co"
#define SUPABASE_KEY    "sb_publishable_hoi9CeVeffstIg814TiUFw_9knsJkoN"

// ==============================================================================
// CONFIGURAÇÃO DE HARDWARE (ESP32 RELAY X1)
// ==============================================================================
// Pino de acionamento do Relé da placa (GPIO 16)
#define RELAY_PIN       16

// Pino de dados do Sensor DHT22 (Pino OUT/DATA no GPIO 4)
#define DHT_PIN         4
#define DHT_TYPE        DHT22

// Lógica de acionamento do relé
#define RELAY_ON_LEVEL  HIGH
#define RELAY_OFF_LEVEL LOW

// Intervalos de tempo (em milissegundos)
#define SENSOR_READ_INTERVAL_MS 2500      // Leitura do DHT a cada 2.5 segundos
#define SUPABASE_SYNC_INTERVAL_MS 5000    // Sincronização com nuvem a cada 5 segundos
#define SUPABASE_LOG_INTERVAL_MS 60000    // Gravação de histórico a cada 1 minuto
