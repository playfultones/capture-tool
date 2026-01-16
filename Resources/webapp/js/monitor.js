// Monitor output routing module

/**
 * Monitor state
 */
const monitorState = {
    device: '',
    channel: 0,
    stereo: true,
    source: 'none', // 'none', 'input', 'output'
    gainDb: -6.0,
    active: false
};

/**
 * Update monitor status display
 */
function updateMonitorStatusDisplay() {
    const statusIndicator = document.getElementById('monitor-status-indicator');
    const statusText = document.getElementById('monitor-status-text');
    const outputBtn = document.getElementById('monitor-output-btn');
    const inputBtn = document.getElementById('monitor-input-btn');
    const levelSlider = document.getElementById('monitor-level');
    const levelValue = document.getElementById('monitor-level-value');
    
    // Update level display
    levelValue.textContent = formatMonitorGain(monitorState.gainDb);
    levelSlider.value = monitorState.gainDb;
    
    // Check if monitor is configured (device selected)
    const isConfigured = monitorState.device !== '';
    
    // Enable/disable controls based on configuration
    outputBtn.disabled = !isConfigured;
    inputBtn.disabled = !isConfigured;
    levelSlider.disabled = !isConfigured;
    
    // Update toggle button states
    outputBtn.classList.toggle('active', monitorState.source === 'output');
    inputBtn.classList.toggle('active', monitorState.source === 'input');
    
    // Update status indicator
    statusIndicator.classList.remove('active', 'not-configured');
    
    if (!isConfigured) {
        statusIndicator.classList.add('not-configured');
        statusText.textContent = 'Not configured';
    } else if (monitorState.source === 'none') {
        statusText.textContent = 'Idle';
    } else {
        statusIndicator.classList.add('active');
        statusText.textContent = monitorState.source === 'input' ? 'Input' : 'Output';
    }
}

/**
 * Update monitor device dropdown with available devices
 */
async function updateMonitorDeviceList() {
    const deviceSelect = document.getElementById('monitor-device');
    
    // Get output devices (monitor uses output devices)
    const devices = audioState.outputDevices || [];
    
    // Build options HTML
    let optionsHtml = '<option value="">None (disabled)</option>';
    for (const device of devices) {
        const selected = device === monitorState.device ? 'selected' : '';
        optionsHtml += `<option value="${device}" ${selected}>${device}</option>`;
    }
    
    deviceSelect.innerHTML = optionsHtml;
}

/**
 * Update monitor channel dropdown based on selected device
 */
async function updateMonitorChannelList() {
    const channelSelect = document.getElementById('monitor-channel');
    const deviceName = monitorState.device;
    
    if (!deviceName) {
        channelSelect.innerHTML = '<option value="">Select device first</option>';
        channelSelect.disabled = true;
        return;
    }
    
    // Get channel count for the device
    const numChannels = await backend.call('getOutputChannelCount', deviceName);
    
    if (numChannels === 0) {
        channelSelect.innerHTML = '<option value="">No channels</option>';
        channelSelect.disabled = true;
        return;
    }
    
    // Build channel options (stereo pairs and individual channels)
    let optionsHtml = '';
    
    // Add stereo pairs first
    for (let i = 0; i < numChannels - 1; i += 2) {
        const selected = (monitorState.channel === i && monitorState.stereo) ? 'selected' : '';
        optionsHtml += `<option value="${i}-stereo" ${selected}>${i + 1}+${i + 2} (Stereo)</option>`;
    }
    
    // Add individual mono channels
    for (let i = 0; i < numChannels; i++) {
        const selected = (monitorState.channel === i && !monitorState.stereo) ? 'selected' : '';
        optionsHtml += `<option value="${i}-mono" ${selected}>${i + 1} (Mono)</option>`;
    }
    
    channelSelect.innerHTML = optionsHtml;
    channelSelect.disabled = false;
}

/**
 * Handle monitor device selection change
 */
async function onMonitorDeviceChange() {
    const deviceSelect = document.getElementById('monitor-device');
    const newDevice = deviceSelect.value;
    
    try {
        await backend.call('setMonitorDevice', newDevice);
        monitorState.device = newDevice;
        
        // Reset source when device changes
        if (!newDevice) {
            monitorState.source = 'none';
            await backend.call('setMonitorSource', 'none');
        }
        
        // Update channel dropdown
        await updateMonitorChannelList();
        
        // Update status display
        updateMonitorStatusDisplay();
    } catch (error) {
        console.error('Failed to set monitor device:', error);
    }
}

/**
 * Handle monitor channel selection change
 */
async function onMonitorChannelChange() {
    const channelSelect = document.getElementById('monitor-channel');
    const value = channelSelect.value;
    
    if (!value) return;
    
    // Parse the value (format: "0-stereo" or "0-mono")
    const [channelStr, mode] = value.split('-');
    const channelIndex = parseInt(channelStr, 10);
    const stereo = mode === 'stereo';
    
    try {
        await backend.call('setMonitorChannel', channelIndex, stereo);
        monitorState.channel = channelIndex;
        monitorState.stereo = stereo;
    } catch (error) {
        console.error('Failed to set monitor channel:', error);
    }
}

/**
 * Handle monitor source toggle (output button)
 */
async function onMonitorOutputToggle() {
    if (!monitorState.device) return;
    
    try {
        // Toggle: if already monitoring output, turn off; otherwise monitor output
        const newSource = monitorState.source === 'output' ? 'none' : 'output';
        await backend.call('setMonitorSource', newSource);
        monitorState.source = newSource;
        monitorState.active = newSource !== 'none';
        updateMonitorStatusDisplay();
    } catch (error) {
        console.error('Failed to toggle monitor output:', error);
    }
}

/**
 * Handle monitor source toggle (input button)
 */
async function onMonitorInputToggle() {
    if (!monitorState.device) return;
    
    try {
        // Toggle: if already monitoring input, turn off; otherwise monitor input
        const newSource = monitorState.source === 'input' ? 'none' : 'input';
        await backend.call('setMonitorSource', newSource);
        monitorState.source = newSource;
        monitorState.active = newSource !== 'none';
        updateMonitorStatusDisplay();
    } catch (error) {
        console.error('Failed to toggle monitor input:', error);
    }
}

/**
 * Handle monitor level slider change
 */
async function onMonitorLevelChange() {
    const slider = document.getElementById('monitor-level');
    const newValue = parseFloat(slider.value);
    
    try {
        const actualValue = await backend.call('setMonitorGain', newValue);
        monitorState.gainDb = actualValue;
        updateMonitorStatusDisplay();
    } catch (error) {
        console.error('Failed to set monitor gain:', error);
        // Revert slider to previous value
        slider.value = monitorState.gainDb;
    }
}

/**
 * Reset monitor level on double-click
 */
async function onMonitorLevelReset() {
    try {
        const actualValue = await backend.call('setMonitorGain', -6.0);
        monitorState.gainDb = actualValue;
        document.getElementById('monitor-level').value = monitorState.gainDb;
        updateMonitorStatusDisplay();
    } catch (error) {
        console.error('Failed to reset monitor gain:', error);
    }
}

/**
 * Load monitor state from backend
 */
async function loadMonitorState() {
    try {
        const state = await backend.call('getMonitorState');
        
        monitorState.device = state.device || '';
        monitorState.channel = state.channel || 0;
        monitorState.stereo = state.stereo !== false;
        monitorState.source = state.source || 'none';
        monitorState.gainDb = state.gainDb ?? -6.0;
        monitorState.active = state.active || false;
        
        await updateMonitorDeviceList();
        await updateMonitorChannelList();
        updateMonitorStatusDisplay();
    } catch (error) {
        console.error('Failed to load monitor state:', error);
    }
}

/**
 * Initialize monitor controls
 */
async function initMonitor() {
    const deviceSelect = document.getElementById('monitor-device');
    const channelSelect = document.getElementById('monitor-channel');
    const outputBtn = document.getElementById('monitor-output-btn');
    const inputBtn = document.getElementById('monitor-input-btn');
    const levelSlider = document.getElementById('monitor-level');
    
    // Set up event listeners
    deviceSelect.addEventListener('change', onMonitorDeviceChange);
    channelSelect.addEventListener('change', onMonitorChannelChange);
    outputBtn.addEventListener('click', onMonitorOutputToggle);
    inputBtn.addEventListener('click', onMonitorInputToggle);
    levelSlider.addEventListener('input', onMonitorLevelChange);
    levelSlider.addEventListener('dblclick', onMonitorLevelReset);
    
    // Load initial state
    await loadMonitorState();
}
