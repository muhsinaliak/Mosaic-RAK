/**
 * MOSAIC RAK GATEWAY - Frontend JavaScript
 * Single Page Application (SPA) Logic
 */

// ============================================================================
// CONFIGURATION
// ============================================================================

const API_BASE = '/api';
const UPDATE_INTERVAL = 3000;  // 3 seconds
const SCAN_DURATION = 60;      // 60 seconds
const GITHUB_REPO = 'muhsinaliak/Mosaic-RAK';  // Fixed GitHub repository

// ============================================================================
// STATE
// ============================================================================

let state = {
    currentPage: 'dashboard',
    isScanning: false,
    scanTimer: null,
    scanTimeLeft: SCAN_DURATION,
    updateTimer: null,
    lastIP: null,
    devices: [],
    discoveredDevices: []
};

// ============================================================================
// DOM ELEMENTS
// ============================================================================

const elements = {
    // Navigation
    navItems: document.querySelectorAll('.nav-item'),
    pages: document.querySelectorAll('.page'),

    // Connection status
    connectionStatus: document.getElementById('connectionStatus'),

    // Dashboard
    deviceName: document.getElementById('deviceName'),
    macAddress: document.getElementById('macAddress'),
    firmware: document.getElementById('firmware'),
    uptime: document.getElementById('uptime'),
    bootTime: document.getElementById('bootTime'),
    networkStatus: document.getElementById('networkStatus'),
    networkType: document.getElementById('networkType'),
    ipAddress: document.getElementById('ipAddress'),
    rssi: document.getElementById('rssi'),
    mqttStatus: document.getElementById('mqttStatus'),
    mqttServer: document.getElementById('mqttServer'),
    loraScanning: document.getElementById('loraScanning'),
    nodesRegistered: document.getElementById('nodesRegistered'),
    nodesOnline: document.getElementById('nodesOnline'),
    heapFree: document.getElementById('heapFree'),
    heapTotal: document.getElementById('heapTotal'),
    heapProgress: document.getElementById('heapProgress'),
    lastUpdate: document.getElementById('lastUpdate'),

    // Devices
    devicesTable: document.getElementById('devicesTable'),
    deviceCount: document.getElementById('deviceCount'),
    btnScanDevices: document.getElementById('btnScanDevices'),

    // Scan Modal
    scanModal: document.getElementById('scanModal'),
    closeScanModal: document.getElementById('closeScanModal'),
    scanTimeLeft: document.getElementById('scanTimeLeft'),
    scanProgress: document.getElementById('scanProgress'),
    scanStatus: document.getElementById('scanStatus'),
    discoveredDevices: document.getElementById('discoveredDevices'),
    btnStopScan: document.getElementById('btnStopScan'),

    // IP Modal
    ipModal: document.getElementById('ipModal'),
    newIpAddress: document.getElementById('newIpAddress'),
    btnOpenNewIP: document.getElementById('btnOpenNewIP'),
    closeIpModal: document.getElementById('closeIpModal'),

    // Network
    toggleBtns: document.querySelectorAll('.toggle-btn'),
    wifiSettings: document.getElementById('wifiSettings'),
    ethernetSettings: document.getElementById('ethernetSettings'),
    btnScanWifi: document.getElementById('btnScanWifi'),
    wifiNetworks: document.getElementById('wifiNetworks'),
    wifiSSID: document.getElementById('wifiSSID'),
    wifiPassword: document.getElementById('wifiPassword'),
    wifiHidden: document.getElementById('wifiHidden'),
    wifiChannel: document.getElementById('wifiChannel'),
    hiddenNetworkOptions: document.getElementById('hiddenNetworkOptions'),
    wifiDHCP: document.getElementById('wifiDHCP'),
    wifiStaticSettings: document.getElementById('wifiStaticSettings'),
    wifiIP: document.getElementById('wifiIP'),
    wifiSubnet: document.getElementById('wifiSubnet'),
    wifiGateway: document.getElementById('wifiGateway'),
    wifiDNS: document.getElementById('wifiDNS'),
    btnSaveWifi: document.getElementById('btnSaveWifi'),
    ethDHCP: document.getElementById('ethDHCP'),
    staticIpSettings: document.getElementById('staticIpSettings'),
    btnSaveEthernet: document.getElementById('btnSaveEthernet'),

    // Password toggle buttons
    togglePasswordBtns: document.querySelectorAll('.toggle-password'),

    // MQTT
    mqttServerInput: document.getElementById('mqttServerInput'),
    mqttPort: document.getElementById('mqttPort'),
    mqttUser: document.getElementById('mqttUser'),
    mqttPassword: document.getElementById('mqttPassword'),
    btnSaveMQTT: document.getElementById('btnSaveMQTT'),
    mqttTestTopic: document.getElementById('mqttTestTopic'),
    mqttTestMessage: document.getElementById('mqttTestMessage'),
    mqttTestRetained: document.getElementById('mqttTestRetained'),
    btnMqttPublish: document.getElementById('btnMqttPublish'),
    mqttPublishResult: document.getElementById('mqttPublishResult'),

    // System
    deviceNameInput: document.getElementById('deviceNameInput'),
    ledBrightness: document.getElementById('ledBrightness'),
    brightnessValue: document.getElementById('brightnessValue'),
    btnSaveDevice: document.getElementById('btnSaveDevice'),
    btnReboot: document.getElementById('btnReboot'),
    btnFactoryReset: document.getElementById('btnFactoryReset'),

    // Toast
    toastContainer: document.getElementById('toastContainer'),

    // WiFi Result Modal
    wifiResultModal: document.getElementById('wifiResultModal'),
    wifiResultTitle: document.getElementById('wifiResultTitle'),
    wifiResultLoading: document.getElementById('wifiResultLoading'),
    wifiResultSuccess: document.getElementById('wifiResultSuccess'),
    wifiResultError: document.getElementById('wifiResultError'),
    wifiNewIP: document.getElementById('wifiNewIP'),
    wifiErrorMessage: document.getElementById('wifiErrorMessage'),
    btnOpenWifiNewIP: document.getElementById('btnOpenWifiNewIP'),
    btnWifiResultClose: document.getElementById('btnWifiResultClose'),
    closeWifiResultModal: document.getElementById('closeWifiResultModal'),

    // Ethernet Result Modal
    ethResultModal: document.getElementById('ethResultModal'),
    ethResultTitle: document.getElementById('ethResultTitle'),
    ethResultLoading: document.getElementById('ethResultLoading'),
    ethResultSuccess: document.getElementById('ethResultSuccess'),
    ethResultError: document.getElementById('ethResultError'),
    ethNewIP: document.getElementById('ethNewIP'),
    ethErrorMessage: document.getElementById('ethErrorMessage'),
    btnOpenEthNewIP: document.getElementById('btnOpenEthNewIP'),
    btnEthResultClose: document.getElementById('btnEthResultClose'),
    closeEthResultModal: document.getElementById('closeEthResultModal'),

    // MQTT Result Modal
    mqttResultModal: document.getElementById('mqttResultModal'),
    mqttResultTitle: document.getElementById('mqttResultTitle'),
    mqttResultLoading: document.getElementById('mqttResultLoading'),
    mqttResultSuccess: document.getElementById('mqttResultSuccess'),
    mqttResultError: document.getElementById('mqttResultError'),
    mqttServerConnected: document.getElementById('mqttServerConnected'),
    mqttErrorMessage: document.getElementById('mqttErrorMessage'),
    btnMqttResultClose: document.getElementById('btnMqttResultClose'),
    closeMqttResultModal: document.getElementById('closeMqttResultModal'),

    // Software Update
    currentFirmware: document.getElementById('currentFirmware'),
    buildDate: document.getElementById('buildDate'),
    githubReleaseInfo: document.getElementById('githubReleaseInfo'),
    latestVersion: document.getElementById('latestVersion'),
    releaseDate: document.getElementById('releaseDate'),
    releaseNotes: document.getElementById('releaseNotes'),
    btnCheckUpdate: document.getElementById('btnCheckUpdate'),
    btnGithubUpdate: document.getElementById('btnGithubUpdate'),
    firmwareFile: document.getElementById('firmwareFile'),
    filesystemFile: document.getElementById('filesystemFile'),
    btnSelectFirmware: document.getElementById('btnSelectFirmware'),
    btnSelectFilesystem: document.getElementById('btnSelectFilesystem'),
    firmwareFileName: document.getElementById('firmwareFileName'),
    filesystemFileName: document.getElementById('filesystemFileName'),
    btnUploadFirmware: document.getElementById('btnUploadFirmware'),
    btnUploadFilesystem: document.getElementById('btnUploadFilesystem'),

    // Update Modal
    updateModal: document.getElementById('updateModal'),
    updateModalTitle: document.getElementById('updateModalTitle'),
    updateProgress: document.getElementById('updateProgress'),
    updateStatusText: document.getElementById('updateStatusText'),
    updateProgressBar: document.getElementById('updateProgressBar'),
    updateProgressText: document.getElementById('updateProgressText'),
    updateSuccess: document.getElementById('updateSuccess'),
    updateError: document.getElementById('updateError'),
    updateErrorMessage: document.getElementById('updateErrorMessage'),
    btnUpdateClose: document.getElementById('btnUpdateClose')
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Make API request
 */
async function api(endpoint, options = {}) {
    try {
        const response = await fetch(API_BASE + endpoint, {
            headers: {
                'Content-Type': 'application/json',
                ...options.headers
            },
            ...options
        });

        const data = await response.json();

        if (!response.ok) {
            throw new Error(data.error || 'Request failed');
        }

        return data;
    } catch (error) {
        console.error('API Error:', error);
        throw error;
    }
}

/**
 * Format uptime seconds to readable string
 */
function formatUptime(seconds) {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;

    if (days > 0) {
        return `${days}d ${hours}h ${mins}m`;
    } else if (hours > 0) {
        return `${hours}h ${mins}m ${secs}s`;
    } else if (mins > 0) {
        return `${mins}m ${secs}s`;
    } else {
        return `${secs}s`;
    }
}

/**
 * Format bytes to KB/MB
 */
function formatBytes(bytes) {
    if (bytes >= 1048576) {
        return (bytes / 1048576).toFixed(1) + ' MB';
    }
    return (bytes / 1024).toFixed(0) + ' KB';
}

/**
 * Show toast notification
 */
function showToast(message, type = 'info') {
    const icons = {
        success: '&#10003;',
        error: '&#10007;',
        warning: '&#9888;',
        info: '&#8505;'
    };

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
        <span class="toast-icon">${icons[type]}</span>
        <span class="toast-message">${message}</span>
    `;

    elements.toastContainer.appendChild(toast);

    // Auto remove after 4 seconds
    setTimeout(() => {
        toast.style.animation = 'toastSlideIn 0.3s ease reverse';
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

/**
 * Get current time string
 */
function getCurrentTime() {
    return new Date().toLocaleTimeString();
}

// ============================================================================
// NAVIGATION
// ============================================================================

function initNavigation() {
    elements.navItems.forEach(item => {
        item.addEventListener('click', () => {
            const page = item.dataset.page;
            navigateTo(page);
        });
    });
}

function navigateTo(page) {
    // Update nav items
    elements.navItems.forEach(item => {
        item.classList.toggle('active', item.dataset.page === page);
    });

    // Update pages
    elements.pages.forEach(p => {
        p.classList.toggle('active', p.id === `page-${page}`);
    });

    state.currentPage = page;

    // Load page-specific data
    switch (page) {
        case 'devices':
            loadDevices();
            break;
        case 'network':
            loadConfig();
            break;
        case 'mqtt':
            loadConfig();
            break;
        case 'system':
            loadConfig();
            break;
    }
}

// ============================================================================
// DASHBOARD
// ============================================================================

async function updateDashboard() {
    try {
        const data = await api('/status');

        // Update connection status
        const statusDot = elements.connectionStatus.querySelector('.status-dot');
        const statusText = elements.connectionStatus.querySelector('.status-text');
        statusDot.classList.remove('offline');
        statusDot.classList.add('online');
        statusText.textContent = 'Connected';

        // Device info
        elements.firmware.textContent = data.version || '--';

        // Update Software Update page info
        if (elements.currentFirmware) {
            elements.currentFirmware.textContent = 'v' + (data.version || '1.0.0');
        }
        if (elements.buildDate) {
            elements.buildDate.textContent = data.build_date || '--';
        }

        // Uptime
        elements.uptime.textContent = formatUptime(data.uptime || 0);

        // Network
        const networkConnected = data.network?.connected;
        elements.networkStatus.textContent = networkConnected ? 'Connected' : 'Disconnected';
        elements.networkStatus.className = `info-value badge ${networkConnected ? 'connected' : 'disconnected'}`;
        elements.networkType.textContent = data.network?.type || '--';

        const newIP = data.network?.ip;
        elements.ipAddress.textContent = newIP || '--';

        // Check for IP change
        if (state.lastIP && newIP && state.lastIP !== newIP && newIP !== '0.0.0.0') {
            showIPChangeModal(newIP);
        }
        state.lastIP = newIP;

        elements.rssi.textContent = data.network?.rssi ? `${data.network.rssi} dBm` : '--';

        // MQTT
        const mqttConnected = data.mqtt?.connected;
        elements.mqttStatus.textContent = mqttConnected ? 'Connected' : 'Disconnected';
        elements.mqttStatus.className = `info-value badge ${mqttConnected ? 'connected' : 'disconnected'}`;
        elements.mqttServer.textContent = data.mqtt?.server || '--';

        // LoRa
        const isScanning = data.lora?.scanning;
        elements.loraScanning.textContent = isScanning ? 'Active' : 'Idle';
        elements.loraScanning.className = `info-value badge ${isScanning ? 'scanning' : ''}`;
        elements.nodesRegistered.textContent = data.lora?.nodes_registered || 0;
        elements.nodesOnline.textContent = data.lora?.nodes_online || 0;

        // Memory
        const heapFree = data.heap_free || 0;
        const heapTotal = data.heap_total || 1;
        elements.heapFree.textContent = formatBytes(heapFree);
        elements.heapTotal.textContent = formatBytes(heapTotal);
        const heapUsedPercent = ((heapTotal - heapFree) / heapTotal) * 100;
        elements.heapProgress.style.width = `${heapUsedPercent}%`;

        // Last update
        elements.lastUpdate.textContent = `Updated: ${getCurrentTime()}`;

    } catch (error) {
        // Connection lost
        const statusDot = elements.connectionStatus.querySelector('.status-dot');
        const statusText = elements.connectionStatus.querySelector('.status-text');
        statusDot.classList.remove('online');
        statusDot.classList.add('offline');
        statusText.textContent = 'Disconnected';
    }
}

// ============================================================================
// DEVICES
// ============================================================================

async function loadDevices() {
    try {
        const data = await api('/nodes');
        state.devices = data.nodes || [];
        renderDevicesTable();
        elements.deviceCount.textContent = state.devices.length;
    } catch (error) {
        showToast('Failed to load devices', 'error');
    }
}

function renderDevicesTable() {
    if (state.devices.length === 0) {
        elements.devicesTable.innerHTML = `
            <tr class="empty-row">
                <td colspan="9">No devices registered</td>
            </tr>
        `;
        return;
    }

    elements.devicesTable.innerHTML = state.devices.map(device => `
        <tr>
            <td>${device.id}</td>
            <td>${device.name || 'Node ' + device.id}</td>
            <td class="mono">${device.mac}</td>
            <td>${device.type_name}</td>
            <td>
                <span class="badge ${device.online ? 'online' : 'offline'}">
                    ${device.online ? 'Online' : 'Offline'}
                </span>
            </td>
            <td>
                <label class="switch">
                    <input type="checkbox"
                           ${device.relays?.[0] ? 'checked' : ''}
                           ${!device.online ? 'disabled' : ''}
                           onchange="toggleRelay(${device.id}, 1)">
                    <span class="switch-slider"></span>
                </label>
            </td>
            <td>
                <label class="switch">
                    <input type="checkbox"
                           ${device.relays?.[1] ? 'checked' : ''}
                           ${!device.online ? 'disabled' : ''}
                           onchange="toggleRelay(${device.id}, 2)">
                    <span class="switch-slider"></span>
                </label>
            </td>
            <td>${device.online ? device.rssi + ' dBm' : '--'}</td>
            <td>
                <button class="btn btn-small btn-danger" onclick="removeDevice(${device.id})">
                    Remove
                </button>
            </td>
        </tr>
    `).join('');
}

async function toggleRelay(nodeId, relayNum) {
    try {
        await api('/control', {
            method: 'POST',
            body: JSON.stringify({
                node_id: nodeId,
                toggle_relay: relayNum
            })
        });
        showToast(`Relay ${relayNum} toggled`, 'success');

        // Refresh after short delay
        setTimeout(loadDevices, 1000);
    } catch (error) {
        showToast('Failed to toggle relay', 'error');
        loadDevices();  // Refresh to get correct state
    }
}

async function removeDevice(nodeId) {
    if (!confirm(`Are you sure you want to remove device ${nodeId}?`)) {
        return;
    }

    try {
        await api(`/nodes?id=${nodeId}`, { method: 'DELETE' });
        showToast('Device removed', 'success');
        loadDevices();
    } catch (error) {
        showToast('Failed to remove device', 'error');
    }
}

// ============================================================================
// DEVICE DISCOVERY (SCAN)
// ============================================================================

function initScanModal() {
    elements.btnScanDevices.addEventListener('click', startScan);
    elements.closeScanModal.addEventListener('click', closeScanModal);
    elements.btnStopScan.addEventListener('click', stopScan);

    // Close on overlay click
    elements.scanModal.querySelector('.modal-overlay').addEventListener('click', closeScanModal);
}

async function startScan() {
    try {
        await api('/scan');

        state.isScanning = true;
        state.scanTimeLeft = SCAN_DURATION;
        state.discoveredDevices = [];

        elements.scanModal.classList.add('active');
        elements.discoveredDevices.innerHTML = '<p style="color: var(--text-muted); text-align: center;">Waiting for devices...</p>';

        updateScanProgress();

        // Start countdown timer
        state.scanTimer = setInterval(() => {
            state.scanTimeLeft--;
            updateScanProgress();

            if (state.scanTimeLeft <= 0) {
                stopScan();
            }

            // Poll for results every 2 seconds
            if (state.scanTimeLeft % 2 === 0) {
                pollScanResults();
            }
        }, 1000);

    } catch (error) {
        showToast('Failed to start scan', 'error');
    }
}

function updateScanProgress() {
    elements.scanTimeLeft.textContent = state.scanTimeLeft;
    const progress = ((SCAN_DURATION - state.scanTimeLeft) / SCAN_DURATION) * 100;
    elements.scanProgress.style.width = `${progress}%`;
    elements.scanStatus.textContent = state.isScanning ? 'Scanning for devices...' : 'Scan complete';
}

async function pollScanResults() {
    try {
        const data = await api('/scan-results');
        state.discoveredDevices = data.devices || [];
        renderDiscoveredDevices();

        if (!data.scanning && state.isScanning) {
            stopScan();
        }
    } catch (error) {
        console.error('Failed to poll scan results:', error);
    }
}

function renderDiscoveredDevices() {
    if (state.discoveredDevices.length === 0) {
        elements.discoveredDevices.innerHTML = '<p style="color: var(--text-muted); text-align: center;">Waiting for devices...</p>';
        return;
    }

    elements.discoveredDevices.innerHTML = state.discoveredDevices.map(device => `
        <div class="discovered-device">
            <div class="device-info">
                <div class="device-mac">${device.mac}</div>
                <div class="device-meta">${device.type_name} | RSSI: ${device.rssi} dBm</div>
            </div>
            <button class="btn btn-primary btn-small" onclick="addDevice('${device.mac}')">
                Add
            </button>
        </div>
    `).join('');
}

async function addDevice(mac) {
    try {
        await api('/add', {
            method: 'POST',
            body: JSON.stringify({ mac: mac })
        });

        showToast(`Pairing started for ${mac}`, 'info');

        // Update UI
        const deviceEl = [...elements.discoveredDevices.querySelectorAll('.discovered-device')]
            .find(el => el.querySelector('.device-mac').textContent === mac);

        if (deviceEl) {
            deviceEl.querySelector('.btn').textContent = 'Adding...';
            deviceEl.querySelector('.btn').disabled = true;
        }

        // Refresh after delay
        setTimeout(() => {
            loadDevices();
            pollScanResults();
        }, 3000);

    } catch (error) {
        showToast('Failed to add device: ' + error.message, 'error');
    }
}

function stopScan() {
    state.isScanning = false;

    if (state.scanTimer) {
        clearInterval(state.scanTimer);
        state.scanTimer = null;
    }

    elements.scanProgress.classList.remove('scanning');
    elements.scanStatus.textContent = 'Scan complete';
    elements.btnStopScan.textContent = 'Close';
}

function closeScanModal() {
    stopScan();
    elements.scanModal.classList.remove('active');
    loadDevices();  // Refresh devices list
}

// ============================================================================
// IP CHANGE MODAL
// ============================================================================

function showIPChangeModal(newIP) {
    elements.newIpAddress.textContent = newIP;
    elements.ipModal.classList.add('active');
}

function initIPModal() {
    elements.btnOpenNewIP.addEventListener('click', () => {
        const ip = elements.newIpAddress.textContent;
        window.open(`http://${ip}`, '_blank');
        elements.ipModal.classList.remove('active');
    });

    elements.closeIpModal.addEventListener('click', () => {
        elements.ipModal.classList.remove('active');
    });

    elements.ipModal.querySelector('.modal-overlay').addEventListener('click', () => {
        elements.ipModal.classList.remove('active');
    });
}

// ============================================================================
// PASSWORD TOGGLE
// ============================================================================

function initPasswordToggle() {
    elements.togglePasswordBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            const targetId = btn.dataset.target;
            const input = document.getElementById(targetId);

            if (input) {
                if (input.type === 'password') {
                    input.type = 'text';
                    btn.classList.add('active');
                    btn.innerHTML = '&#128064;';  // Open eye
                } else {
                    input.type = 'password';
                    btn.classList.remove('active');
                    btn.innerHTML = '&#128065;';  // Eye
                }
            }
        });
    });
}

// ============================================================================
// NETWORK SETTINGS
// ============================================================================

function initNetworkSettings() {
    // Connection type toggle (Ethernet/WiFi)
    elements.toggleBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            elements.toggleBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');

            const type = btn.dataset.type;
            elements.wifiSettings.style.display = type === 'wifi' ? 'block' : 'none';
            elements.ethernetSettings.style.display = type === 'ethernet' ? 'block' : 'none';
        });
    });

    // Ethernet DHCP toggle
    elements.ethDHCP.addEventListener('change', () => {
        elements.staticIpSettings.style.display = elements.ethDHCP.checked ? 'none' : 'block';
    });

    // WiFi Hidden network toggle
    elements.wifiHidden.addEventListener('change', () => {
        elements.hiddenNetworkOptions.style.display = elements.wifiHidden.checked ? 'block' : 'none';
    });

    // WiFi DHCP toggle
    elements.wifiDHCP.addEventListener('change', () => {
        elements.wifiStaticSettings.style.display = elements.wifiDHCP.checked ? 'none' : 'block';
    });

    elements.btnScanWifi.addEventListener('click', scanWifiNetworks);
    elements.btnSaveWifi.addEventListener('click', saveWifiSettings);
    elements.btnSaveEthernet.addEventListener('click', saveEthernetSettings);

    // WiFi Result Modal handlers
    elements.btnWifiResultClose.addEventListener('click', closeWifiResultModal);
    elements.closeWifiResultModal.addEventListener('click', closeWifiResultModal);
    elements.wifiResultModal.querySelector('.modal-overlay').addEventListener('click', closeWifiResultModal);
    elements.btnOpenWifiNewIP.addEventListener('click', () => {
        const ip = elements.wifiNewIP.textContent;
        window.open(`http://${ip}`, '_blank');
    });

    // Ethernet Result Modal handlers
    elements.btnEthResultClose.addEventListener('click', closeEthResultModal);
    elements.closeEthResultModal.addEventListener('click', closeEthResultModal);
    elements.ethResultModal.querySelector('.modal-overlay').addEventListener('click', closeEthResultModal);
    elements.btnOpenEthNewIP.addEventListener('click', () => {
        const ip = elements.ethNewIP.textContent;
        window.open(`http://${ip}`, '_blank');
    });

    // MQTT Result Modal handlers
    elements.btnMqttResultClose.addEventListener('click', closeMqttResultModal);
    elements.closeMqttResultModal.addEventListener('click', closeMqttResultModal);
    elements.mqttResultModal.querySelector('.modal-overlay').addEventListener('click', closeMqttResultModal);
}

function showWifiResultModal(loading = true, success = false, ip = '', error = '') {
    elements.wifiResultModal.classList.add('active');

    // Reset all states
    elements.wifiResultLoading.style.display = 'none';
    elements.wifiResultSuccess.style.display = 'none';
    elements.wifiResultError.style.display = 'none';
    elements.btnOpenWifiNewIP.style.display = 'none';

    if (loading) {
        elements.wifiResultTitle.textContent = 'Connecting...';
        elements.wifiResultLoading.style.display = 'block';
    } else if (success) {
        elements.wifiResultTitle.textContent = 'Connected!';
        elements.wifiResultSuccess.style.display = 'block';
        elements.wifiNewIP.textContent = ip;
        elements.btnOpenWifiNewIP.style.display = 'inline-block';
    } else {
        elements.wifiResultTitle.textContent = 'Connection Failed';
        elements.wifiResultError.style.display = 'block';
        elements.wifiErrorMessage.textContent = error || 'Could not connect to network';
    }
}

function closeWifiResultModal() {
    elements.wifiResultModal.classList.remove('active');
}

function showEthResultModal(loading = true, success = false, message = '', error = '') {
    elements.ethResultModal.classList.add('active');

    // Reset all states
    elements.ethResultLoading.style.display = 'none';
    elements.ethResultSuccess.style.display = 'none';
    elements.ethResultError.style.display = 'none';
    elements.btnOpenEthNewIP.style.display = 'none';

    if (loading) {
        elements.ethResultTitle.textContent = 'Connecting...';
        elements.ethResultLoading.style.display = 'block';
    } else if (success) {
        elements.ethResultTitle.textContent = 'Connected!';
        elements.ethResultSuccess.style.display = 'block';
        elements.ethNewIP.textContent = message || 'Restarting...';
        // Show Open button only if we have a valid IP
        if (message && message.match(/^\d+\.\d+\.\d+\.\d+$/)) {
            elements.btnOpenEthNewIP.style.display = 'inline-block';
        }
    } else {
        elements.ethResultTitle.textContent = 'Error';
        elements.ethResultError.style.display = 'block';
        elements.ethErrorMessage.textContent = error || 'Could not save settings';
    }
}

function closeEthResultModal() {
    elements.ethResultModal.classList.remove('active');
}

function showMqttResultModal(loading = true, success = false, server = '', error = '') {
    elements.mqttResultModal.classList.add('active');

    // Reset all states
    elements.mqttResultLoading.style.display = 'none';
    elements.mqttResultSuccess.style.display = 'none';
    elements.mqttResultError.style.display = 'none';

    if (loading) {
        elements.mqttResultTitle.textContent = 'Connecting...';
        elements.mqttResultLoading.style.display = 'block';
    } else if (success) {
        elements.mqttResultTitle.textContent = 'Connected!';
        elements.mqttResultSuccess.style.display = 'block';
        elements.mqttServerConnected.textContent = server ? `Server: ${server}` : '';
    } else {
        elements.mqttResultTitle.textContent = 'Error';
        elements.mqttResultError.style.display = 'block';
        elements.mqttErrorMessage.textContent = error || 'Connection failed';
    }
}

function closeMqttResultModal() {
    elements.mqttResultModal.classList.remove('active');
}

async function scanWifiNetworks() {
    try {
        elements.btnScanWifi.textContent = 'Scanning...';
        elements.btnScanWifi.disabled = true;

        const data = await api('/wifi-scan');
        renderWifiNetworks(data.networks || []);

        showToast('WiFi scan complete', 'success');
    } catch (error) {
        showToast('WiFi scan not available', 'warning');
        elements.wifiNetworks.innerHTML = '<p style="color: var(--text-muted);">WiFi scan not available</p>';
    } finally {
        elements.btnScanWifi.textContent = 'Scan';
        elements.btnScanWifi.disabled = false;
    }
}

function renderWifiNetworks(networks) {
    if (networks.length === 0) {
        elements.wifiNetworks.innerHTML = '<p style="color: var(--text-muted);">No networks found</p>';
        return;
    }

    elements.wifiNetworks.innerHTML = networks.map(net => `
        <div class="wifi-network" onclick="selectWifiNetwork('${net.ssid}')">
            <span class="wifi-ssid">${net.ssid}</span>
            <span class="wifi-signal">${net.rssi} dBm</span>
        </div>
    `).join('');
}

function selectWifiNetwork(ssid) {
    elements.wifiSSID.value = ssid;

    // Highlight selected
    elements.wifiNetworks.querySelectorAll('.wifi-network').forEach(el => {
        el.classList.toggle('selected', el.querySelector('.wifi-ssid').textContent === ssid);
    });
}

async function saveWifiSettings() {
    const ssid = elements.wifiSSID.value.trim();
    const password = elements.wifiPassword.value;
    const hidden = elements.wifiHidden.checked;
    const channel = hidden ? parseInt(elements.wifiChannel.value) : 0;
    const useDHCP = elements.wifiDHCP.checked;

    if (!ssid) {
        showToast('Please enter WiFi SSID', 'warning');
        return;
    }

    // Build connect request
    const connectData = {
        ssid: ssid,
        password: password
    };

    // Add static IP settings if not using DHCP
    if (!useDHCP) {
        const wifiIPVal = elements.wifiIP?.value || '';
        const wifiGatewayVal = elements.wifiGateway?.value || '';

        // Validate required fields for static IP
        if (!wifiIPVal || !wifiGatewayVal) {
            showToast('IP Address and Gateway are required for static IP', 'warning');
            return;
        }

        // Validate IP format
        const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
        if (!ipRegex.test(wifiIPVal) || !ipRegex.test(wifiGatewayVal)) {
            showToast('Invalid IP address format', 'warning');
            return;
        }

        // Check gateway is not 0.0.0.0
        if (wifiGatewayVal === '0.0.0.0') {
            showToast('Gateway cannot be 0.0.0.0', 'warning');
            return;
        }

        connectData.use_static_ip = true;
        connectData.static_ip = wifiIPVal;
        connectData.subnet = elements.wifiSubnet?.value || '255.255.255.0';
        connectData.gateway = wifiGatewayVal;
        connectData.dns = elements.wifiDNS?.value || '8.8.8.8';
    } else {
        connectData.use_static_ip = false;
    }

    // Add MQTT settings if configured
    const mqttServer = elements.mqttServerInput?.value?.trim();
    if (mqttServer) {
        connectData.mqtt_server = mqttServer;
        connectData.mqtt_port = parseInt(elements.mqttPort?.value) || 1883;
        connectData.mqtt_user = elements.mqttUser?.value?.trim() || '';
        connectData.mqtt_password = elements.mqttPassword?.value || '';
    }

    // Show loading modal
    showWifiResultModal(true);

    try {
        const response = await fetch(API_BASE + '/wifi-connect', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(connectData)
        });

        const data = await response.json();

        if (data.success) {
            // Connection successful - show IP
            showWifiResultModal(false, true, data.ip);
        } else {
            // Connection failed - show error
            showWifiResultModal(false, false, '', data.error || 'Connection failed');
        }
    } catch (error) {
        // Network error (device may have restarted)
        showWifiResultModal(false, false, '', 'Connection timeout - device may be restarting');
    }
}

async function saveEthernetSettings() {
    const useDHCP = elements.ethDHCP.checked;

    // Prepare config data
    const configData = {};

    // Add static IP settings if not using DHCP
    if (!useDHCP) {
        const ethIP = document.getElementById('ethIP');
        const ethSubnet = document.getElementById('ethSubnet');
        const ethGateway = document.getElementById('ethGateway');
        const ethDNS = document.getElementById('ethDNS');

        // Validate required fields for static IP
        if (!ethIP?.value || !ethGateway?.value) {
            showToast('IP Address and Gateway are required for static IP', 'warning');
            return;
        }

        // Validate IP format
        const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
        if (!ipRegex.test(ethIP.value) || !ipRegex.test(ethGateway.value)) {
            showToast('Invalid IP address format', 'warning');
            return;
        }

        // Check gateway is not 0.0.0.0
        if (ethGateway.value === '0.0.0.0') {
            showToast('Gateway cannot be 0.0.0.0', 'warning');
            return;
        }

        configData.use_static_ip = true;
        configData.static_ip = ethIP.value;
        configData.subnet = ethSubnet?.value || '255.255.255.0';
        configData.gateway = ethGateway.value;
        configData.dns = ethDNS?.value || '8.8.8.8';
    } else {
        configData.use_static_ip = false;
    }

    // Show loading modal
    showEthResultModal(true);
    elements.btnSaveEthernet.disabled = true;

    try {
        // Call ethernet-connect endpoint (like wifi-connect)
        const response = await api('/ethernet-connect', {
            method: 'POST',
            body: JSON.stringify(configData)
        });

        if (response.success && response.ip) {
            // Show IP in modal
            showEthResultModal(false, true, response.ip);

            // Reboot after delay so user can see the IP
            setTimeout(async () => {
                try {
                    await api('/reboot', { method: 'POST' });
                } catch (e) {}
            }, 3000);
        } else {
            // No IP - show error
            showEthResultModal(false, false, '', response.error || 'Could not obtain IP address');
            elements.btnSaveEthernet.textContent = 'Save';
            elements.btnSaveEthernet.disabled = false;
        }

    } catch (error) {
        showEthResultModal(false, false, '', 'Failed: ' + error.message);
        elements.btnSaveEthernet.textContent = 'Save';
        elements.btnSaveEthernet.disabled = false;
    }
}

// ============================================================================
// MQTT SETTINGS
// ============================================================================

function initMQTTSettings() {
    elements.btnSaveMQTT.addEventListener('click', saveMQTTSettings);
    elements.btnMqttPublish.addEventListener('click', publishMQTTTest);
}

async function publishMQTTTest() {
    const topic = elements.mqttTestTopic.value.trim();
    const message = elements.mqttTestMessage.value;
    const retained = elements.mqttTestRetained.checked;

    if (!topic) {
        showToast('Please enter a topic', 'warning');
        return;
    }

    if (!message) {
        showToast('Please enter a message', 'warning');
        return;
    }

    elements.btnMqttPublish.textContent = 'Publishing...';
    elements.btnMqttPublish.disabled = true;

    try {
        await api('/mqtt-publish', {
            method: 'POST',
            body: JSON.stringify({
                topic: topic,
                message: message,
                retained: retained
            })
        });

        showToast('Message published successfully', 'success');

        // Show result
        elements.mqttPublishResult.style.display = 'block';
        elements.mqttPublishResult.innerHTML = `<span style="color: var(--status-online);">Published to: ${topic}</span>`;

        // Clear after 5 seconds
        setTimeout(() => {
            elements.mqttPublishResult.style.display = 'none';
        }, 5000);

    } catch (error) {
        showToast('Failed to publish: ' + error.message, 'error');
        elements.mqttPublishResult.style.display = 'block';
        elements.mqttPublishResult.innerHTML = `<span style="color: var(--status-offline);">Error: ${error.message}</span>`;
    } finally {
        elements.btnMqttPublish.textContent = 'Publish';
        elements.btnMqttPublish.disabled = false;
    }
}

async function saveMQTTSettings() {
    const server = elements.mqttServerInput.value.trim();
    const port = parseInt(elements.mqttPort.value) || 1883;
    const user = elements.mqttUser.value.trim();
    const password = elements.mqttPassword.value;

    if (!server) {
        showToast('Please enter MQTT server address', 'warning');
        return;
    }

    // Show loading modal
    showMqttResultModal(true);
    elements.btnSaveMQTT.disabled = true;

    try {
        const response = await api('/mqtt-connect', {
            method: 'POST',
            body: JSON.stringify({
                mqtt_server: server,
                mqtt_port: port,
                mqtt_user: user,
                mqtt_password: password
            })
        });

        if (response.success) {
            showMqttResultModal(false, true, server);
            // Update dashboard MQTT status
            setTimeout(() => fetchStatus(), 1000);
        } else {
            showMqttResultModal(false, false, '', response.error || 'Connection failed - check credentials');
        }
    } catch (error) {
        showMqttResultModal(false, false, '', error.message || 'Failed to connect');
    } finally {
        elements.btnSaveMQTT.disabled = false;
    }
}

// ============================================================================
// SYSTEM SETTINGS
// ============================================================================

function initSystemSettings() {
    elements.ledBrightness.addEventListener('input', () => {
        elements.brightnessValue.textContent = elements.ledBrightness.value + '%';
    });

    elements.btnSaveDevice.addEventListener('click', saveDeviceSettings);
    elements.btnReboot.addEventListener('click', rebootDevice);
    elements.btnFactoryReset.addEventListener('click', factoryReset);
}

async function saveDeviceSettings() {
    const name = elements.deviceNameInput.value.trim();
    const brightness = parseInt(elements.ledBrightness.value);

    try {
        await api('/config', {
            method: 'POST',
            body: JSON.stringify({
                device_name: name,
                led_brightness: brightness
            })
        });

        showToast('Device settings saved', 'success');
    } catch (error) {
        showToast('Failed to save settings', 'error');
    }
}

async function rebootDevice() {
    if (!confirm('Are you sure you want to reboot the device?')) {
        return;
    }

    try {
        await api('/reboot', { method: 'POST' });
        showToast('Rebooting...', 'info');
    } catch (error) {
        // Expected - connection will be lost
        showToast('Rebooting...', 'info');
    }
}

async function factoryReset() {
    if (!confirm('WARNING: This will erase all settings and registered devices. Are you sure?')) {
        return;
    }

    if (!confirm('This action cannot be undone. Continue?')) {
        return;
    }

    try {
        await api('/factory-reset', { method: 'POST' });
        showToast('Factory reset initiated...', 'warning');
    } catch (error) {
        showToast('Factory reset initiated...', 'warning');
    }
}

// ============================================================================
// CONFIG LOADING
// ============================================================================

async function loadConfig() {
    try {
        const data = await api('/config');

        // Populate form fields
        elements.deviceNameInput.value = data.device_name || '';
        elements.wifiSSID.value = data.wifi_ssid || '';
        elements.mqttServerInput.value = data.mqtt_server || '';
        elements.mqttPort.value = data.mqtt_port || 1883;
        elements.mqttUser.value = data.mqtt_user || '';
        elements.ledBrightness.value = data.led_brightness || 50;
        elements.brightnessValue.textContent = (data.led_brightness || 50) + '%';

        // WiFi advanced settings
        if (elements.wifiHidden) {
            elements.wifiHidden.checked = data.wifi_hidden || false;
            elements.hiddenNetworkOptions.style.display = data.wifi_hidden ? 'block' : 'none';
        }
        if (elements.wifiChannel) {
            elements.wifiChannel.value = data.wifi_channel || 0;
        }

        // Static IP settings (shared between WiFi and Ethernet)
        const useStaticIP = data.use_static_ip === true;

        // WiFi static IP
        if (elements.wifiDHCP) {
            elements.wifiDHCP.checked = !useStaticIP;
            elements.wifiStaticSettings.style.display = useStaticIP ? 'block' : 'none';
        }
        if (elements.wifiIP) {
            elements.wifiIP.value = data.static_ip || '';
            elements.wifiSubnet.value = data.subnet || '255.255.255.0';
            elements.wifiGateway.value = data.gateway || '';
            elements.wifiDNS.value = data.dns || '8.8.8.8';
        }

        // Ethernet static IP
        if (elements.ethDHCP) {
            elements.ethDHCP.checked = !useStaticIP;
            elements.staticIpSettings.style.display = useStaticIP ? 'block' : 'none';
        }
        const ethIP = document.getElementById('ethIP');
        const ethSubnet = document.getElementById('ethSubnet');
        const ethGateway = document.getElementById('ethGateway');
        const ethDNS = document.getElementById('ethDNS');
        if (ethIP) {
            ethIP.value = data.static_ip || '';
            ethSubnet.value = data.subnet || '255.255.255.0';
            ethGateway.value = data.gateway || '';
            ethDNS.value = data.dns || '8.8.8.8';
        }

        // Update dashboard device name
        elements.deviceName.textContent = data.device_name || 'Mosaic Gateway';

    } catch (error) {
        console.error('Failed to load config:', error);
    }
}

// ============================================================================
// SOFTWARE UPDATE
// ============================================================================

let githubReleaseData = null;

function initSoftwareUpdate() {
    // File selection buttons
    elements.btnSelectFirmware?.addEventListener('click', () => {
        elements.firmwareFile.click();
    });

    elements.btnSelectFilesystem?.addEventListener('click', () => {
        elements.filesystemFile.click();
    });

    // File input change handlers
    elements.firmwareFile?.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) {
            elements.firmwareFileName.textContent = file.name;
            elements.firmwareFileName.classList.add('selected');
            elements.btnUploadFirmware.disabled = false;
        } else {
            elements.firmwareFileName.textContent = 'No file selected';
            elements.firmwareFileName.classList.remove('selected');
            elements.btnUploadFirmware.disabled = true;
        }
    });

    elements.filesystemFile?.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) {
            elements.filesystemFileName.textContent = file.name;
            elements.filesystemFileName.classList.add('selected');
            elements.btnUploadFilesystem.disabled = false;
        } else {
            elements.filesystemFileName.textContent = 'No file selected';
            elements.filesystemFileName.classList.remove('selected');
            elements.btnUploadFilesystem.disabled = true;
        }
    });

    // Upload buttons
    elements.btnUploadFirmware?.addEventListener('click', () => uploadFirmware('firmware'));
    elements.btnUploadFilesystem?.addEventListener('click', () => uploadFirmware('filesystem'));

    // GitHub update buttons
    elements.btnCheckUpdate?.addEventListener('click', checkGithubUpdate);
    elements.btnGithubUpdate?.addEventListener('click', updateFromGithub);

    // Update modal close button
    elements.btnUpdateClose?.addEventListener('click', () => {
        elements.updateModal.classList.remove('active');
    });
}

function showUpdateModal(title) {
    elements.updateModalTitle.textContent = title;
    elements.updateProgress.style.display = 'block';
    elements.updateSuccess.style.display = 'none';
    elements.updateError.style.display = 'none';
    elements.btnUpdateClose.style.display = 'none';
    elements.updateProgressBar.style.width = '0%';
    elements.updateProgressText.textContent = '0%';
    elements.updateStatusText.textContent = 'Preparing update...';
    elements.updateModal.classList.add('active');
}

function updateProgress(percent, status) {
    elements.updateProgressBar.style.width = percent + '%';
    elements.updateProgressText.textContent = percent + '%';
    if (status) {
        elements.updateStatusText.textContent = status;
    }
}

function showUpdateSuccess() {
    elements.updateProgress.style.display = 'none';
    elements.updateSuccess.style.display = 'block';
    elements.updateError.style.display = 'none';
}

function showUpdateError(message) {
    elements.updateProgress.style.display = 'none';
    elements.updateSuccess.style.display = 'none';
    elements.updateError.style.display = 'block';
    elements.updateErrorMessage.textContent = message;
    elements.btnUpdateClose.style.display = 'inline-block';
}

async function uploadFirmware(type) {
    const fileInput = type === 'firmware' ? elements.firmwareFile : elements.filesystemFile;
    const file = fileInput.files[0];

    if (!file) {
        showToast('Please select a file first', 'warning');
        return;
    }

    const endpoint = type === 'firmware' ? '/update' : '/update-fs';
    const title = type === 'firmware' ? 'Updating Firmware...' : 'Updating File System...';

    showUpdateModal(title);

    try {
        const xhr = new XMLHttpRequest();

        xhr.upload.addEventListener('progress', (e) => {
            if (e.lengthComputable) {
                const percent = Math.round((e.loaded / e.total) * 100);
                updateProgress(percent, 'Uploading...');
            }
        });

        xhr.addEventListener('load', () => {
            if (xhr.status === 200) {
                try {
                    const response = JSON.parse(xhr.responseText);
                    if (response.success) {
                        updateProgress(100, 'Update complete!');
                        setTimeout(() => {
                            showUpdateSuccess();
                        }, 500);
                    } else {
                        showUpdateError(response.error || 'Update failed');
                    }
                } catch (e) {
                    showUpdateError('Invalid response from server');
                }
            } else {
                showUpdateError('Upload failed: ' + xhr.statusText);
            }
        });

        xhr.addEventListener('error', () => {
            showUpdateError('Network error during upload');
        });

        xhr.addEventListener('timeout', () => {
            showUpdateError('Upload timed out');
        });

        xhr.open('POST', API_BASE + endpoint);
        xhr.timeout = 300000; // 5 minute timeout

        const formData = new FormData();
        formData.append('file', file);

        xhr.send(formData);

    } catch (error) {
        showUpdateError(error.message || 'Upload failed');
    }
}

async function checkGithubUpdate() {
    elements.btnCheckUpdate.disabled = true;
    elements.btnCheckUpdate.textContent = 'Checking...';

    try {
        const response = await api('/github-release', {
            method: 'POST',
            body: JSON.stringify({ repo: GITHUB_REPO })
        });

        if (response.success && response.release) {
            githubReleaseData = response.release;

            elements.latestVersion.textContent = response.release.version || '--';
            elements.releaseDate.textContent = response.release.date || '--';
            elements.releaseNotes.textContent = response.release.notes || 'No release notes';

            elements.githubReleaseInfo.style.display = 'block';

            // Show update button if newer version
            if (response.release.update_available) {
                elements.btnGithubUpdate.style.display = 'inline-block';
                showToast('New version available: ' + response.release.version, 'info');
            } else {
                elements.btnGithubUpdate.style.display = 'none';
                showToast('You are running the latest version', 'success');
            }
        } else {
            showToast(response.error || 'Failed to check for updates', 'error');
        }

    } catch (error) {
        showToast('Failed to check for updates: ' + error.message, 'error');
    } finally {
        elements.btnCheckUpdate.disabled = false;
        elements.btnCheckUpdate.textContent = 'Check for Updates';
    }
}

async function updateFromGithub() {
    if (!githubReleaseData) {
        showToast('Please check for updates first', 'warning');
        return;
    }

    if (!confirm('Do you want to update to ' + githubReleaseData.version + '?')) {
        return;
    }

    showUpdateModal('Downloading Update...');

    try {
        const response = await api('/github-update', {
            method: 'POST',
            body: JSON.stringify({
                repo: GITHUB_REPO,
                version: githubReleaseData.version
            })
        });

        if (response.success) {
            // Start polling for progress
            pollUpdateProgress();
        } else {
            showUpdateError(response.error || 'Failed to start update');
        }

    } catch (error) {
        showUpdateError(error.message || 'Failed to start update');
    }
}

async function pollUpdateProgress() {
    try {
        const response = await api('/update-progress');

        if (response.status === 'downloading') {
            updateProgress(response.progress || 0, 'Downloading: ' + (response.progress || 0) + '%');
            setTimeout(pollUpdateProgress, 500);
        } else if (response.status === 'installing') {
            updateProgress(response.progress || 0, 'Installing...');
            setTimeout(pollUpdateProgress, 500);
        } else if (response.status === 'complete') {
            updateProgress(100, 'Update complete!');
            setTimeout(() => {
                showUpdateSuccess();
            }, 500);
        } else if (response.status === 'error') {
            showUpdateError(response.error || 'Update failed');
        } else {
            setTimeout(pollUpdateProgress, 500);
        }

    } catch (error) {
        // Connection lost - probably rebooting
        updateProgress(100, 'Rebooting...');
        setTimeout(() => {
            showUpdateSuccess();
        }, 2000);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

function init() {
    console.log('Mosaic RAK Gateway UI initialized');

    // Initialize all modules
    initNavigation();
    initScanModal();
    initIPModal();
    initPasswordToggle();
    initNetworkSettings();
    initMQTTSettings();
    initSystemSettings();
    initSoftwareUpdate();

    // Load initial data
    loadConfig();
    updateDashboard();

    // Start periodic updates
    state.updateTimer = setInterval(() => {
        if (state.currentPage === 'dashboard') {
            updateDashboard();
        } else if (state.currentPage === 'devices') {
            loadDevices();
        }
    }, UPDATE_INTERVAL);
}

// Make functions available globally for onclick handlers
window.toggleRelay = toggleRelay;
window.removeDevice = removeDevice;
window.addDevice = addDevice;
window.selectWifiNetwork = selectWifiNetwork;

// Start the application
document.addEventListener('DOMContentLoaded', init);
