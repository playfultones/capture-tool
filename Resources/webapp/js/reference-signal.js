// Reference signal module - load, play, stop, loop, clear

/**
 * Reference signal state
 */
const referenceSignalState = {
    loaded: false,
    filePath: '',
    fileName: '',
    sampleRate: 0,
    numSamples: 0,
    durationSeconds: 0,
    isPlaying: false,
    isLooping: false
};

/**
 * Update the reference signal status display
 */
function updateReferenceSignalDisplay() {
    const statusEl = document.getElementById('reference-signal-status');
    const errorEl = document.getElementById('reference-signal-error');
    const clearBtn = document.getElementById('clear-reference-btn');
    const playBtn = document.getElementById('play-reference-btn');
    const stopBtn = document.getElementById('stop-reference-btn');
    const loopBtn = document.getElementById('loop-reference-btn');
    
    // Hide any previous error
    errorEl.classList.add('hidden');
    
    if (referenceSignalState.loaded) {
        const duration = formatDuration(referenceSignalState.durationSeconds);
        const sampleRateKhz = (referenceSignalState.sampleRate / 1000).toFixed(1);
        
        statusEl.innerHTML = `
            <div class="file-info">
                <span class="file-name">${referenceSignalState.fileName}</span>
                <span class="file-details">${duration} @ ${sampleRateKhz} kHz | Mono</span>
            </div>
        `;
        
        // Show playback and clear buttons
        playBtn.classList.remove('hidden');
        stopBtn.classList.remove('hidden');
        loopBtn.classList.remove('hidden');
        clearBtn.classList.remove('hidden');
        
        // Update button states based on playback
        updatePlaybackButtonStates();
    } else {
        statusEl.innerHTML = '<span class="no-signal">No file loaded</span>';
        
        // Hide all buttons except browse
        playBtn.classList.add('hidden');
        stopBtn.classList.add('hidden');
        loopBtn.classList.add('hidden');
        clearBtn.classList.add('hidden');
    }
    
    // Update capture UI (enable/disable start button, update total time)
    updateCaptureForReferenceSignal();
}

/**
 * Update play/stop/loop button states based on playback state
 */
function updatePlaybackButtonStates() {
    const playBtn = document.getElementById('play-reference-btn');
    const stopBtn = document.getElementById('stop-reference-btn');
    const loopBtn = document.getElementById('loop-reference-btn');
    const clearBtn = document.getElementById('clear-reference-btn');
    
    if (referenceSignalState.isPlaying) {
        playBtn.disabled = true;
        playBtn.classList.add('active');
        stopBtn.disabled = false;
        clearBtn.disabled = true;  // Don't allow clearing during playback
    } else {
        playBtn.disabled = false;
        playBtn.classList.remove('active');
        stopBtn.disabled = true;
        clearBtn.disabled = false;
    }
    
    // Update loop button state
    loopBtn.classList.toggle('active', referenceSignalState.isLooping);
}

/**
 * Show an error message for reference signal loading
 * @param {string} message 
 */
function showReferenceSignalError(message) {
    const errorEl = document.getElementById('reference-signal-error');
    errorEl.textContent = message;
    errorEl.classList.remove('hidden');
}

/**
 * Start reference signal preview playback
 */
async function playReferenceSignal() {
    const playBtn = document.getElementById('play-reference-btn');
    
    try {
        playBtn.disabled = true;
        
        const success = await backend.call('startReferencePlayback');
        
        if (success) {
            referenceSignalState.isPlaying = true;
            updatePlaybackButtonStates();
        } else {
            console.error('Failed to start reference playback');
        }
    } catch (error) {
        console.error('Failed to play reference signal:', error);
    }
}

/**
 * Stop reference signal preview playback
 */
async function stopReferenceSignal() {
    const stopBtn = document.getElementById('stop-reference-btn');
    
    try {
        stopBtn.disabled = true;
        
        await backend.call('stopReferencePlayback');
        
        referenceSignalState.isPlaying = false;
        updatePlaybackButtonStates();
    } catch (error) {
        console.error('Failed to stop reference signal:', error);
    }
}

/**
 * Toggle loop mode for reference signal playback
 */
async function toggleLoopReferenceSignal() {
    try {
        const newLoopState = !referenceSignalState.isLooping;
        await backend.call('setReferencePlaybackLoop', newLoopState);
        
        referenceSignalState.isLooping = newLoopState;
        updatePlaybackButtonStates();
    } catch (error) {
        console.error('Failed to toggle loop mode:', error);
    }
}

/**
 * Clear the loaded reference signal
 */
async function clearReferenceSignal() {
    const clearBtn = document.getElementById('clear-reference-btn');
    
    try {
        clearBtn.disabled = true;
        
        // Stop playback first if active
        if (referenceSignalState.isPlaying) {
            await backend.call('stopReferencePlayback');
            referenceSignalState.isPlaying = false;
        }
        
        await backend.call('clearReferenceSignal');
        
        // Reset state
        referenceSignalState.loaded = false;
        referenceSignalState.filePath = '';
        referenceSignalState.fileName = '';
        referenceSignalState.sampleRate = 0;
        referenceSignalState.numSamples = 0;
        referenceSignalState.durationSeconds = 0;
        
        updateReferenceSignalDisplay();
    } catch (error) {
        console.error('Failed to clear reference signal:', error);
    } finally {
        clearBtn.disabled = false;
    }
}

/**
 * Handle browse button click - open file picker
 */
async function browseReferenceSignal() {
    const btn = document.getElementById('browse-reference-btn');
    
    try {
        // Disable button during file picker
        btn.disabled = true;
        
        const result = await backend.call('browseReferenceSignal');
        
        if (result.cancelled) {
            // User cancelled, do nothing
            return;
        }
        
        if (result.success) {
            // Update state
            referenceSignalState.loaded = true;
            referenceSignalState.filePath = result.filePath;
            referenceSignalState.fileName = result.fileName;
            referenceSignalState.sampleRate = result.sampleRate;
            referenceSignalState.numSamples = result.numSamples;
            referenceSignalState.durationSeconds = result.durationSeconds;
            
            updateReferenceSignalDisplay();
        } else {
            // Show error
            showReferenceSignalError(result.errorMessage);
        }
    } catch (error) {
        console.error('Failed to browse reference signal:', error);
        showReferenceSignalError('Failed to open file picker');
    } finally {
        btn.disabled = false;
    }
}

/**
 * Load initial reference signal state from backend
 */
async function loadReferenceSignalState() {
    try {
        const state = await backend.call('getReferenceSignalState');
        
        if (state.loaded) {
            referenceSignalState.loaded = true;
            referenceSignalState.filePath = state.filePath;
            referenceSignalState.fileName = state.fileName;
            referenceSignalState.sampleRate = state.sampleRate;
            referenceSignalState.numSamples = state.numSamples;
            referenceSignalState.durationSeconds = state.durationSeconds;
        }
        
        // Also update playback and loop state
        referenceSignalState.isPlaying = state.isPlaying || false;
        referenceSignalState.isLooping = state.isLooping || false;
        
        updateReferenceSignalDisplay();
    } catch (error) {
        console.error('Failed to load reference signal state:', error);
    }
}

/**
 * Handle playback state changes from backend
 * @param {object} data - { isPlaying: boolean }
 */
function onPlaybackStateChanged(data) {
    if (referenceSignalState.isPlaying !== data.isPlaying) {
        referenceSignalState.isPlaying = data.isPlaying;
        updatePlaybackButtonStates();
    }
}

/**
 * Initialize reference signal UI
 */
function initReferenceSignal() {
    const browseBtn = document.getElementById('browse-reference-btn');
    const playBtn = document.getElementById('play-reference-btn');
    const stopBtn = document.getElementById('stop-reference-btn');
    const loopBtn = document.getElementById('loop-reference-btn');
    const clearBtn = document.getElementById('clear-reference-btn');
    
    browseBtn.addEventListener('click', browseReferenceSignal);
    playBtn.addEventListener('click', playReferenceSignal);
    stopBtn.addEventListener('click', stopReferenceSignal);
    loopBtn.addEventListener('click', toggleLoopReferenceSignal);
    clearBtn.addEventListener('click', clearReferenceSignal);
    
    // Load initial state
    loadReferenceSignalState();
}
