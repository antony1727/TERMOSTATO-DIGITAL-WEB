-- ==============================================================================
-- TERMOSTATO DIGITAL WEB - SUPABASE DATABASE SCHEMA
-- ==============================================================================

-- 1. TABELA DE CONFIGURAÇÃO DO TERMOSTATO
CREATE TABLE IF NOT EXISTS public.thermostat_config (
    id TEXT PRIMARY KEY DEFAULT 'main_thermostat',
    target_temp NUMERIC(4, 1) NOT NULL DEFAULT 25.0,
    hysteresis NUMERIC(3, 1) NOT NULL DEFAULT 1.0,
    mode TEXT NOT NULL DEFAULT 'AUTO_COOL' CHECK (mode IN ('AUTO_COOL', 'AUTO_HEAT', 'MANUAL_ON', 'MANUAL_OFF')),
    relay_state BOOLEAN NOT NULL DEFAULT FALSE,
    current_temp NUMERIC(4, 1),
    current_humidity NUMERIC(4, 1),
    last_seen TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Insere o registro padrão inicial caso não exista
INSERT INTO public.thermostat_config (id, target_temp, hysteresis, mode, relay_state)
VALUES ('main_thermostat', 25.0, 1.0, 'AUTO_COOL', FALSE)
ON CONFLICT (id) DO NOTHING;

-- 2. TABELA DE HISTÓRICO DE LEITURAS (TELEMETRIA DHT22)
CREATE TABLE IF NOT EXISTS public.thermostat_logs (
    id BIGSERIAL PRIMARY KEY,
    temperature NUMERIC(4, 1) NOT NULL,
    humidity NUMERIC(4, 1) NOT NULL,
    relay_state BOOLEAN NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Criar índice para consultas rápidas por data no histórico/gráficos
CREATE INDEX IF NOT EXISTS idx_thermostat_logs_created_at ON public.thermostat_logs(created_at DESC);

-- 3. HABILITAR ROW LEVEL SECURITY (RLS) E CRIAR POLÍTICAS PÚBLICAS (ANON KEY)
ALTER TABLE public.thermostat_config ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.thermostat_logs ENABLE ROW LEVEL SECURITY;

-- Permitir leitura e escrita anônima para controle do IoT e Painel Web
CREATE POLICY "Permitir SELECT publico em thermostat_config"
    ON public.thermostat_config FOR SELECT
    TO anon, authenticated
    USING (true);

CREATE POLICY "Permitir UPDATE publico em thermostat_config"
    ON public.thermostat_config FOR UPDATE
    TO anon, authenticated
    USING (true)
    WITH CHECK (true);

CREATE POLICY "Permitir SELECT publico em thermostat_logs"
    ON public.thermostat_logs FOR SELECT
    TO anon, authenticated
    USING (true);

CREATE POLICY "Permitir INSERT publico em thermostat_logs"
    ON public.thermostat_logs FOR INSERT
    TO anon, authenticated
    WITH CHECK (true);

-- Habilitar Realtime para atualizações instantâneas no Frontend
ALTER PUBLICATION supabase_realtime ADD TABLE public.thermostat_config;
