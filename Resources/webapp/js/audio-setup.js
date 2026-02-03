// Audio setup module - devices, channels, sample rate, test tone, output trim

/**
 * Audio state management
 */
const audioState = {
    inputDevices: [],
    outputDevices: [],
    currentInputDevice: '',
    currentInputChannel: 0,
    currentOutputDevice: '',
    currentOutputChannel: 0,
    availableSampleRates: [],
    currentSampleRate: 0
};

/**
 * Initialize audio system (triggers permission dialog if needed)
 * Call this before using any audio functionality
 */
async function initializeAudio() {
    const initialized = await backend.call('isAudioInitialized');
    if (initialized) return true;
    
    console.log('Initializing audio system...');
    const success = await backend.call('initializeAudio');
    if (!success) {
        console.error('Failed to initialize audio');
    }
    return success;
}

/**
 * Load and display audio devices
 */
async function loadAudioDevices() {
    try {
        // Initialize audio first (this triggers permission dialog if needed)
        await initializeAudio();
        
        // Fetch device names and current state
        const [inputDevices, outputDevices, state] = await Promise.all([
            backend.call('getInputDevices'),
            backend.call('getOutputDevices'),
            backend.call('getAudioState')
        ]);
        
        audioState.inputDevices = inputDevices || [];
        audioState.outputDevices = outputDevices || [];
        audioState.currentInputDevice = state?.inputDevice || '';
        audioState.currentInputChannel = state?.inputChannel || 0;
        audioState.currentOutputDevice = state?.outputDevice || '';
        audioState.currentOutputChannel = state?.outputChannel || 0;
        
        // Populate device dropdowns
        const inputDeviceSelect = document.getElementById('input-device');
        const outputDeviceSelect = document.getElementById('output-device');
        
        populateSelect(
            inputDeviceSelect,
            audioState.inputDevices.map(name => ({ value: name, label: name })),
            audioState.currentInputDevice
        );
        
        populateSelect(
            outputDeviceSelect,
            audioState.outputDevices.map(name => ({ value: name, label: name })),
            audioState.currentOutputDevice
        );
        
        // Populate channel dropdowns (fetch actual channel counts)
        await updateInputChannels();
        await updateOutputChannels();
        
    } catch (error) {
        console.error('Failed to load audio devices:', error);
    }
}

/**
 * Update input channel dropdown based on selected device
 */
async function updateInputChannels() {
    const deviceSelect = document.getElementById('input-device');
    const channelSelect = document.getElementById('input-channel');
    const deviceName = deviceSelect.value;
    
    if (!deviceName) {
        populateChannelSelect(channelSelect, 0, 0);
        return;
    }
    
    // Fetch actual channel count from backend
    const numChannels = await backend.call('getInputChannelCount', deviceName);
    populateChannelSelect(channelSelect, numChannels, audioState.currentInputChannel);
}

/**
 * Update output channel dropdown based on selected device
 */
async function updateOutputChannels() {
    const deviceSelect = document.getElementById('output-device');
    const channelSelect = document.getElementById('output-channel');
    const deviceName = deviceSelect.value;
    
    if (!deviceName) {
        populateChannelSelect(channelSelect, 0, 0);
        return;
    }
    
    // Fetch actual channel count from backend
    const numChannels = await backend.call('getOutputChannelCount', deviceName);
    populateChannelSelect(channelSelect, numChannels, audioState.currentOutputChannel);
}

/**
 * Handle input device change
 */
async function onInputDeviceChange() {
    const deviceSelect = document.getElementById('input-device');
    
    audioState.currentInputDevice = deviceSelect.value;
    audioState.currentInputChannel = 0; // Reset to first channel
    
    // Update channel dropdown with actual channel count
    await updateInputChannels();
    
    // Apply to backend
    await backend.call('setInputDevice', audioState.currentInputDevice, audioState.currentInputChannel);
}

/**
 * Handle input channel change
 */
async function onInputChannelChange() {
    const channelSelect = document.getElementById('input-channel');
    audioState.currentInputChannel = parseInt(channelSelect.value, 10);
    
    await backend.call('setInputDevice', audioState.currentInputDevice, audioState.currentInputChannel);
}

/**
 * Handle output device change
 */
async function onOutputDeviceChange() {
    const deviceSelect = document.getElementById('output-device');
    
    audioState.currentOutputDevice = deviceSelect.value;
    audioState.currentOutputChannel = 0; // Reset to first channel
    
    // Update channel dropdown with actual channel count
    await updateOutputChannels();
    
    // Apply to backend
    await backend.call('setOutputDevice', audioState.currentOutputDevice, audioState.currentOutputChannel);
}

/**
 * Handle output channel change
 */
async function onOutputChannelChange() {
    const channelSelect = document.getElementById('output-channel');
    audioState.currentOutputChannel = parseInt(channelSelect.value, 10);
    
    await backend.call('setOutputDevice', audioState.currentOutputDevice, audioState.currentOutputChannel);
}

/**
 * Handle audio device list changes (hot-plug events)
 * Called when devices are connected or disconnected
 */
async function onAudioDevicesChanged() {
    console.log('Audio devices changed, refreshing device list...');
    await loadAudioDevices();
    await loadSampleRates();
    await updateMonitorDeviceList();
}

//==============================================================================
// Sample Rate

/**
 * Load and display sample rate options
 */
async function loadSampleRates() {
    const sampleRateSelect = document.getElementById('sample-rate');
    
    try {
        const [availableRates, currentRate] = await Promise.all([
            backend.call('getAvailableSampleRates'),
            backend.call('getCurrentSampleRate')
        ]);
        
        audioState.availableSampleRates = availableRates || [];
        audioState.currentSampleRate = currentRate || 0;
        
        if (audioState.availableSampleRates.length === 0) {
            sampleRateSelect.innerHTML = '<option value="">No rates available</option>';
            sampleRateSelect.disabled = true;
            return;
        }
        
        const options = audioState.availableSampleRates.map(rate => ({
            value: String(rate),
            label: formatSampleRate(rate)
        }));
        
        populateSelect(sampleRateSelect, options, String(audioState.currentSampleRate));
        
    } catch (error) {
        console.error('Failed to load sample rates:', error);
        sampleRateSelect.innerHTML = '<option value="">Error loading</option>';
        sampleRateSelect.disabled = true;
    }
}

/**
 * Handle sample rate change
 */
async function onSampleRateChange() {
    const sampleRateSelect = document.getElementById('sample-rate');
    const newRate = parseInt(sampleRateSelect.value, 10);
    
    if (isNaN(newRate) || newRate === 0) {
        return;
    }
    
    try {
        const success = await backend.call('setSampleRate', newRate);
        
        if (success) {
            audioState.currentSampleRate = newRate;
        } else {
            // Revert to previous value on failure
            sampleRateSelect.value = String(audioState.currentSampleRate);
            console.error('Failed to set sample rate');
        }
    } catch (error) {
        // Revert to previous value on error
        sampleRateSelect.value = String(audioState.currentSampleRate);
        console.error('Error setting sample rate:', error);
    }
}

//==============================================================================
// Test Tone

/**
 * Test tone state
 */
let testToneEnabled = false;

/**
 * Toggle test tone on/off
 */
async function toggleTestTone() {
    const btn = document.getElementById('test-tone-btn');
    
    try {
        testToneEnabled = !testToneEnabled;
        await backend.call('setTestToneEnabled', testToneEnabled);
        
        // Update button state
        btn.classList.toggle('active', testToneEnabled);
    } catch (error) {
        // Revert state on error
        testToneEnabled = !testToneEnabled;
        console.error('Failed to toggle test tone:', error);
    }
}

/**
 * Initialize test tone button
 */
async function initTestTone() {
    const btn = document.getElementById('test-tone-btn');
    
    // Get initial state from backend
    try {
        testToneEnabled = await backend.call('isTestToneEnabled');
        btn.classList.toggle('active', testToneEnabled);
        btn.disabled = false;
        btn.addEventListener('click', toggleTestTone);
    } catch (error) {
        console.error('Failed to initialize test tone:', error);
    }
}

//==============================================================================
// Output Gain Trim

/**
 * Current output gain trim value in dB
 * Applied to test tone and reference signal playback.
 * Use this to reduce the signal going to the unit if input is clipping.
 */
let outputGainTrimDb = 0.0;

/**
 * Test tone level constant (-18 dBFS)
 */
const TEST_TONE_LEVEL_DBFS = -18.0;

/**
 * Update the output trim display and warning state
 */
function updateOutputTrimDisplay() {
    const valueEl = document.getElementById('output-gain-trim-value');
    const warningEl = document.getElementById('output-trim-warning');
    
    valueEl.textContent = formatGainTrim(outputGainTrimDb);
    
    // Calculate effective output level (test tone + trim)
    const effectiveOutput = TEST_TONE_LEVEL_DBFS + outputGainTrimDb;
    
    // Show warning if output is getting hot
    if (effectiveOutput > -1) {
        warningEl.textContent = effectiveOutput > 0 
            ? 'Output clipping - reduce trim!' 
            : 'Output level hot - consider reducing trim';
        warningEl.classList.remove('hidden');
        warningEl.classList.toggle('error', effectiveOutput > 0);
    } else {
        warningEl.classList.add('hidden');
    }
}

/**
 * Handle output gain trim slider change
 */
async function onOutputGainTrimChange() {
    const slider = document.getElementById('output-gain-trim');
    const newValue = parseFloat(slider.value);
    
    try {
        // Update backend
        const actualValue = await backend.call('setOutputGainTrim', newValue);
        outputGainTrimDb = actualValue;
        updateOutputTrimDisplay();
    } catch (error) {
        console.error('Failed to set output gain trim:', error);
        // Revert slider to previous value
        slider.value = outputGainTrimDb;
    }
}

/**
 * Reset output gain trim to 0 dB on double-click
 */
async function onOutputGainTrimReset() {
    const slider = document.getElementById('output-gain-trim');
    
    try {
        const actualValue = await backend.call('setOutputGainTrim', 0.0);
        outputGainTrimDb = actualValue;
        slider.value = outputGainTrimDb;
        updateOutputTrimDisplay();
    } catch (error) {
        console.error('Failed to reset output gain trim:', error);
    }
}

/**
 * Initialize output gain trim control
 */
async function initOutputGainTrim() {
    const slider = document.getElementById('output-gain-trim');
    
    try {
        // Get initial value from backend
        outputGainTrimDb = await backend.call('getOutputGainTrim');
        slider.value = outputGainTrimDb;
        updateOutputTrimDisplay();
        
        // Set up event listeners
        slider.addEventListener('input', onOutputGainTrimChange);
        slider.addEventListener('dblclick', onOutputGainTrimReset);
    } catch (error) {
        console.error('Failed to initialize output gain trim:', error);
    }
}
