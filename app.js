// ==============================================================================
// TERMOSTATO DIGITAL WEB - JAVASCRIPT FRONTEND
// ==============================================================================

// Configurações Padrão
const DEFAULT_SUPABASE_URL = "https://dejascgqmovkdbytujde.supabase.co";
const DEFAULT_SUPABASE_KEY = "sb_publishable_hoi9CeVeffstIg814TiUFw_9knsJkoN";

let supabaseClient = null;
let currentConfig = {
    target_temp: 25.0,
    hysteresis: 1.0,
    mode: 'AUTO_COOL',
    relay_state: false,
    current_temp: null,
    current_humidity: null
};

let telemetryChart = null;

// ==============================================================================
// INICIALIZAÇÃO
// ==============================================================================
document.addEventListener('DOMContentLoaded', () => {
    initSupabase();
    initChart();
    
    // Atualizar a cada 15 segundos se o status do dispositivo continua ativo
    setInterval(checkDeviceOnlineStatus, 15000);
});

function initSupabase() {
    const savedUrl = localStorage.getItem('SUPABASE_URL') || DEFAULT_SUPABASE_URL;
    const savedKey = localStorage.getItem('SUPABASE_KEY') || DEFAULT_SUPABASE_KEY;

    if (!savedUrl || !savedKey) {
        document.getElementById('deviceStatusText').innerText = 'Credenciais Pendentes';
        document.getElementById('deviceIndicator').className = 'w-2.5 h-2.5 rounded-full bg-amber-500';
        openConfigModal();
        return;
    }

    try {
        supabaseClient = supabase.createClient(savedUrl, savedKey);
        fetchConfig();
        fetchTelemetryHistory();
        setupRealtimeSubscription();
    } catch (err) {
        console.error("Erro ao inicializar Supabase:", err);
        alert("Erro ao conectar com o Supabase. Verifique a URL e Chave inseridas.");
    }
}

// ==============================================================================
// BUSCA E ATUALIZAÇÃO DE DADOS
// ==============================================================================
async function fetchConfig() {
    if (!supabaseClient) return;

    const { data, error } = await supabaseClient
        .from('thermostat_config')
        .select('*')
        .eq('id', 'main_thermostat')
        .single();

    if (error) {
        console.error("Erro ao buscar configuração:", error);
        return;
    }

    if (data) {
        currentConfig = data;
        updateUI();
    }
}

function updateUI() {
    // 1. Atualizar Temperatura e Umidade Atual
    if (currentConfig.current_temp !== null && currentConfig.current_temp !== undefined) {
        document.getElementById('liveTemp').innerText = Number(currentConfig.current_temp).toFixed(1);
    }
    if (currentConfig.current_humidity !== null && currentConfig.current_humidity !== undefined) {
        document.getElementById('liveHumidity').innerText = Number(currentConfig.current_humidity).toFixed(1);
    }

    // 2. Atualizar Diferença de Temperatura Alvo
    if (currentConfig.current_temp !== null && currentConfig.target_temp !== null) {
        const diff = (currentConfig.current_temp - currentConfig.target_temp).toFixed(1);
        const diffSymbol = diff > 0 ? `+${diff}` : `${diff}`;
        document.getElementById('tempDiffText').innerHTML = `<span>Diferen&ccedil;a do Alvo: <strong>${diffSymbol} &deg;C</strong></span>`;
    }

    // 3. Atualizar Relé
    const relayText = document.getElementById('relayStatusText');
    const relayIcon = document.getElementById('relayIconContainer');
    const relayDesc = document.getElementById('relayActionDesc');
    const relayGlow = document.getElementById('relayGlow');

    if (currentConfig.relay_state) {
        relayText.innerText = 'LIGADO';
        relayText.className = 'text-3xl font-extrabold text-emerald-400';
        relayIcon.className = 'p-2 bg-emerald-500/20 text-emerald-400 rounded-lg group-hover:scale-110 transition';
        relayDesc.innerText = 'Carga acionada e energizada';
        relayGlow.className = 'absolute -right-8 -bottom-8 w-32 h-32 bg-emerald-500/15 rounded-full blur-2xl transition';
    } else {
        relayText.innerText = 'DESLIGADO';
        relayText.className = 'text-3xl font-extrabold text-slate-400';
        relayIcon.className = 'p-2 bg-slate-800 text-slate-400 rounded-lg group-hover:scale-110 transition';
        relayDesc.innerText = 'Carga desativada';
        relayGlow.className = 'absolute -right-8 -bottom-8 w-32 h-32 bg-slate-700/5 rounded-full blur-2xl transition';
    }

    // 4. Atualizar Ajustes
    document.getElementById('targetTempDisplay').innerText = Number(currentConfig.target_temp).toFixed(1);
    document.getElementById('hysteresisDisplay').innerHTML = `${Number(currentConfig.hysteresis).toFixed(1)} &deg;C`;
    document.getElementById('hysteresisSlider').value = currentConfig.hysteresis;

    // 5. Botões de Modo
    document.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('active'));
    const activeBtn = document.getElementById(`btn-${currentConfig.mode}`);
    if (activeBtn) activeBtn.classList.add('active');

    // 6. Verificar status online
    checkDeviceOnlineStatus();
}

function checkDeviceOnlineStatus() {
    if (!currentConfig.last_seen) {
        setDeviceStatus('Offline', false);
        return;
    }

    const lastSeenTime = new Date(currentConfig.last_seen).getTime();
    const now = Date.now();
    const diffSeconds = Math.floor((now - lastSeenTime) / 1000);

    if (diffSeconds < 35) {
        setDeviceStatus('ESP32 Online', true);
    } else {
        setDeviceStatus(`Offline há ${Math.floor(diffSeconds / 60)}m`, false);
    }
}

function setDeviceStatus(text, isOnline) {
    const statusText = document.getElementById('deviceStatusText');
    const indicator = document.getElementById('deviceIndicator');

    statusText.innerText = text;
    if (isOnline) {
        indicator.className = 'w-2.5 h-2.5 rounded-full bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.8)]';
    } else {
        indicator.className = 'w-2.5 h-2.5 rounded-full bg-rose-500';
    }
}

// ==============================================================================
// REALTIME SUBSCRIPTION (SUPABASE)
// ==============================================================================
function setupRealtimeSubscription() {
    if (!supabaseClient) return;

    supabaseClient
        .channel('thermostat_realtime_channel')
        .on('postgres_changes', {
            event: 'UPDATE',
            schema: 'public',
            table: 'thermostat_config',
            filter: 'id=eq.main_thermostat'
        }, (payload) => {
            console.log('[Realtime Update]', payload.new);
            currentConfig = payload.new;
            updateUI();
        })
        .subscribe();
}

// ==============================================================================
// AJUSTES DO USUÁRIO
// ==============================================================================
function adjustTarget(amount) {
    let newTarget = parseFloat(currentConfig.target_temp) + amount;
    newTarget = Math.round(newTarget * 10) / 10;
    if (newTarget < 0) newTarget = 0;
    if (newTarget > 80) newTarget = 80;

    currentConfig.target_temp = newTarget;
    document.getElementById('targetTempDisplay').innerText = newTarget.toFixed(1);
}

function onHysteresisChange(val) {
    currentConfig.hysteresis = parseFloat(val);
    document.getElementById('hysteresisDisplay').innerHTML = `${parseFloat(val).toFixed(1)} &deg;C`;
}

function setMode(newMode) {
    currentConfig.mode = newMode;
    document.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('active'));
    const btn = document.getElementById(`btn-${newMode}`);
    if (btn) btn.classList.add('active');
}

async function saveConfigToSupabase() {
    if (!supabaseClient) {
        alert("Supabase não configurado. Clique no ícone de engrenagem para configurar.");
        return;
    }

    const saveBtn = document.getElementById('btnSaveConfig');
    const originalText = saveBtn.innerHTML;
    saveBtn.innerHTML = `<span>Salvando...</span>`;
    saveBtn.disabled = true;

    try {
        const { error } = await supabaseClient
            .from('thermostat_config')
            .update({
                target_temp: currentConfig.target_temp,
                hysteresis: currentConfig.hysteresis,
                mode: currentConfig.mode,
                updated_at: new Date().toISOString()
            })
            .eq('id', 'main_thermostat');

        if (error) throw error;

        saveBtn.innerHTML = `<i data-lucide="check" class="w-4 h-4"></i> Salvo com Sucesso!`;
        saveBtn.classList.replace('from-sky-500', 'from-emerald-500');
        saveBtn.classList.replace('to-indigo-600', 'to-emerald-600');
        lucide.createIcons();

        setTimeout(() => {
            saveBtn.innerHTML = originalText;
            saveBtn.classList.replace('from-emerald-500', 'from-sky-500');
            saveBtn.classList.replace('to-emerald-600', 'to-indigo-600');
            saveBtn.disabled = false;
            lucide.createIcons();
        }, 2000);

    } catch (err) {
        console.error("Erro ao salvar no Supabase:", err);
        alert("Erro ao salvar configurações no Supabase: " + err.message);
        saveBtn.innerHTML = originalText;
        saveBtn.disabled = false;
        lucide.createIcons();
    }
}

// ==============================================================================
// GRÁFICO HISTÓRICO (CHART.JS)
// ==============================================================================
function initChart() {
    const ctx = document.getElementById('telemetryChart').getContext('2d');
    telemetryChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                {
                    label: 'Temperatura (°C)',
                    borderColor: '#38bdf8',
                    backgroundColor: 'rgba(56, 189, 248, 0.1)',
                    borderWidth: 2.5,
                    fill: true,
                    tension: 0.35,
                    data: []
                },
                {
                    label: 'Umidade (%)',
                    borderColor: '#22d3ee',
                    backgroundColor: 'transparent',
                    borderWidth: 2,
                    borderDash: [4, 4],
                    fill: false,
                    tension: 0.35,
                    data: []
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: {
                mode: 'index',
                intersect: false
            },
            plugins: {
                legend: {
                    labels: { color: '#94a3b8', font: { size: 12 } }
                },
                tooltip: {
                    backgroundColor: '#1e293b',
                    titleColor: '#f8fafc',
                    bodyColor: '#cbd5e1',
                    borderColor: '#334155',
                    borderWidth: 1
                }
            },
            scales: {
                x: {
                    grid: { color: '#1e293b' },
                    ticks: { color: '#64748b', maxTicksLimit: 8 }
                },
                y: {
                    grid: { color: '#1e293b' },
                    ticks: { color: '#64748b' }
                }
            }
        }
    });
}

async function fetchTelemetryHistory() {
    if (!supabaseClient) return;

    const { data, error } = await supabaseClient
        .from('thermostat_logs')
        .select('*')
        .order('created_at', { ascending: false })
        .limit(30);

    if (error) {
        console.error("Erro ao buscar histórico:", error);
        return;
    }

    if (data && data.length > 0) {
        const sorted = data.reverse();
        const labels = sorted.map(item => {
            const date = new Date(item.created_at);
            return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        });
        const temps = sorted.map(item => item.temperature);
        const humids = sorted.map(item => item.humidity);

        telemetryChart.data.labels = labels;
        telemetryChart.data.datasets[0].data = temps;
        telemetryChart.data.datasets[1].data = humids;
        telemetryChart.update();
    }
}

// ==============================================================================
// MODAL DE CONFIGURAÇÃO DO SUPABASE
// ==============================================================================
function openConfigModal() {
    document.getElementById('supabaseUrlInput').value = localStorage.getItem('SUPABASE_URL') || DEFAULT_SUPABASE_URL;
    document.getElementById('supabaseKeyInput').value = localStorage.getItem('SUPABASE_KEY') || DEFAULT_SUPABASE_KEY;
    document.getElementById('configModal').classList.remove('hidden');
}

function closeConfigModal() {
    document.getElementById('configModal').classList.add('hidden');
}

function saveSupabaseCredentials() {
    const url = document.getElementById('supabaseUrlInput').value.trim();
    const key = document.getElementById('supabaseKeyInput').value.trim();

    if (!url || !key) {
        alert("Por favor, preencha a URL e a Anon Key.");
        return;
    }

    localStorage.setItem('SUPABASE_URL', url);
    localStorage.setItem('SUPABASE_KEY', key);
    closeConfigModal();
    initSupabase();
}
