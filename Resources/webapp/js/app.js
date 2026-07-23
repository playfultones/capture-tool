// Main application initialization module

/**
 * Initialize the application
 */
async function init() {
    try {
        // Load audio devices, sample rates, initialize controls, reference signal, calibration, and capture
        await loadAudioDevices();
        await loadSampleRates();
        await initTestTone();
        await initOutputGainTrim();
        await initMonitor();
        initReferenceSignal();
        await initCalibration();
        initOutputFolder();
        initCapture();
        initCaptureControls();
        initCaptureList();
        initCaptureWorkflowButtons();
        initProjectActions();
        initAllCollapsiblePanels();
        initMeterClickHandlers();

        // Set up event listeners for audio device changes
        document.getElementById('input-device').addEventListener('change', onInputDeviceChange);
        document.getElementById('input-channel').addEventListener('change', onInputChannelChange);
        document.getElementById('output-device').addEventListener('change', onOutputDeviceChange);
        document.getElementById('output-channel').addEventListener('change', onOutputChannelChange);
        document.getElementById('sample-rate').addEventListener('change', onSampleRateChange);

        // Subscribe to meter updates from backend
        backend.onEvent('meterUpdate', updateMeters);

        // Subscribe to playback state updates
        backend.onEvent('playbackStateChanged', onPlaybackStateChanged);

        // Subscribe to device list changes (hot-plug support)
        backend.onEvent('audioDevicesChanged', onAudioDevicesChanged);

        // Subscribe to capture state and completion events
        backend.onEvent('captureStateChanged', onCaptureStateChanged);
        backend.onEvent('captureComplete', onCaptureComplete);

        // Subscribe to project events from native menu
        backend.onEvent('projectNewRequested', onProjectNewRequested);
        backend.onEvent('projectLoaded', onProjectLoaded);

        // Subscribe to auto-save notifications (updates the header save indicator)
        backend.onEvent('projectSaved', onProjectSaved);
        updateSaveStatusDisplay();

        // Initialize visual guide module (optional feature) LAST: it is not
        // essential to core operation, so it must never gate the backend event
        // subscriptions above. If it ever hangs or throws, the rest of the app
        // (meters, device changes, capture, project events) stays fully wired.
        if (typeof initGuideModule === 'function') {
            try {
                await initGuideModule();
            } catch (guideError) {
                console.error('Visual guide module failed to initialize:', guideError);
            }
        }

    } catch (error) {
        console.error('Initialization failed:', error);
    }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
