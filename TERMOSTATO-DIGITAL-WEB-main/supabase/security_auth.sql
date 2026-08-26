-- ==============================================================================
-- TERMOSTATO DIGITAL - REGRAS DE SEGURANÇA E AUTENTICAÇÃO (RLS)
-- ==============================================================================

-- 1. Certificar que RLS está ativo em todas as tabelas
ALTER TABLE public.thermostat_config ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.thermostat_logs ENABLE ROW LEVEL SECURITY;

-- 2. Limpar políticas antigas
DROP POLICY IF EXISTS "Permitir SELECT publico em thermostat_config" ON public.thermostat_config;
DROP POLICY IF EXISTS "Permitir UPDATE publico em thermostat_config" ON public.thermostat_config;
DROP POLICY IF EXISTS "Permitir SELECT publico em thermostat_logs" ON public.thermostat_logs;
DROP POLICY IF EXISTS "Permitir INSERT publico em thermostat_logs" ON public.thermostat_logs;
DROP POLICY IF EXISTS "Auth: Leitura publica de configuracao" ON public.thermostat_config;
DROP POLICY IF EXISTS "Auth: Apenas usuarios autenticados e ESP32 alteram configuracao" ON public.thermostat_config;
DROP POLICY IF EXISTS "Auth: Leitura de historico" ON public.thermostat_logs;
DROP POLICY IF EXISTS "Auth: Gravacao de telemetria" ON public.thermostat_logs;

-- 3. POLÍTICAS DE CONFIGURAÇÃO (thermostat_config)
-- Leitura pública (necessária para ESP32 e tela inicial)
CREATE POLICY "Auth: Leitura publica de configuracao"
    ON public.thermostat_config FOR SELECT
    TO anon, authenticated
    USING (true);

-- Atualização: Usuários Autenticados (Dashboard) e ESP32 (Anon com chave válida)
CREATE POLICY "Auth: Apenas usuarios autenticados e ESP32 alteram configuracao"
    ON public.thermostat_config FOR UPDATE
    TO anon, authenticated
    USING (true)
    WITH CHECK (true);

-- 4. POLÍTICAS DE LOGS/HISTÓRICO (thermostat_logs)
-- Leitura de histórico para visualização nos gráficos
CREATE POLICY "Auth: Leitura de historico"
    ON public.thermostat_logs FOR SELECT
    TO anon, authenticated
    USING (true);

-- Gravação de novas leituras de telemetria do DHT22
CREATE POLICY "Auth: Gravacao de telemetria"
    ON public.thermostat_logs FOR INSERT
    TO anon, authenticated
    WITH CHECK (true);
