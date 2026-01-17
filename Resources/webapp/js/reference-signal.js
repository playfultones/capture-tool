// Reference signals module - multiple signals with per-signal tail settings

/**
 * Reference signals state
 */
let referenceSignals = [];
let selectedSignalId = null;
let isPlaying = false;
let isLooping = false;

/**
 * Format duration for display (e.g., "2.5s" or "1:23.4")
 */
function formatSignalDuration(seconds) {
    if (seconds < 60) {
        return seconds.toFixed(1) + 's';
    }
    const mins = Math.floor(seconds / 60);
    const secs = (seconds % 60).toFixed(1);
    return `${mins}:${secs.padStart(4, '0')}`;
}

/**
 * Render the signal list
 */
function renderSignalList() {
    const listEl = document.getElementById('reference-signals-list');
    const emptyEl = document.getElementById('reference-signals-empty');
    
    listEl.innerHTML = '';
    
    if (referenceSignals.length === 0) {
        emptyEl.style.display = 'block';
        updatePlaybackButtonStates();
        updateCaptureForReferenceSignals();
        return;
    }
    
    emptyEl.style.display = 'none';
    
    for (const signal of referenceSignals) {
        const row = document.createElement('div');
        row.className = 'reference-signal-row';
        row.dataset.id = signal.id;
        
        if (signal.id === selectedSignalId) {
            row.classList.add('selected');
        }
        
        const sampleRateKhz = (signal.sampleRate / 1000).toFixed(1);
        
        row.innerHTML = `
            <div class="signal-info">
                <span class="signal-name">${signal.fileName}</span>
                <span class="signal-details">${formatSignalDuration(signal.durationSeconds)} @ ${sampleRateKhz} kHz</span>
            </div>
            <label class="signal-tail-label">Tail:</label>
            <select class="signal-tail-select" data-id="${signal.id}">
                <option value="0" ${signal.tailMs === 0 ? 'selected' : ''}>0ms</option>
                <option value="250" ${signal.tailMs === 250 ? 'selected' : ''}>250ms</option>
                <option value="500" ${signal.tailMs === 500 ? 'selected' : ''}>500ms</option>
                <option value="1000" ${signal.tailMs === 1000 ? 'selected' : ''}>1000ms</option>
            </select>
            <button class="btn-remove-signal" data-id="${signal.id}" title="Remove signal">
                <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor">
                    <path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"/>
                </svg>
            </button>
        `;
        
        // Click row to select for preview
        row.addEventListener('click', (e) => {
            // Don't select if clicking on controls
            if (e.target.closest('.signal-tail-select') || e.target.closest('.btn-remove-signal')) {
                return;
            }
            selectSignalForPreview(signal.id);
        });
        
        listEl.appendChild(row);
    }
    
    // Attach event handlers for tail dropdowns and remove buttons
    listEl.querySelectorAll('.signal-tail-select').forEach(select => {
        select.addEventListener('change', (e) => {
            e.stopPropagation();
            onTailChange(e.target.dataset.id, parseInt(e.target.value));
        });
    });
    
    listEl.querySelectorAll('.btn-remove-signal').forEach(btn => {
        btn.addEventListener('click', (e) => {
            e.stopPropagation();
            removeSignal(e.currentTarget.dataset.id);
        });
    });
    
    updatePlaybackButtonStates();
    updateCaptureForReferenceSignals();
}

/**
 * Update playback button states
 */
function updatePlaybackButtonStates() {
    const playBtn = document.getElementById('play-reference-btn');
    const stopBtn = document.getElementById('stop-reference-btn');
    const loopBtn = document.getElementById('loop-reference-btn');
    
    const hasSelection = selectedSignalId !== null;
    const hasSignals = referenceSignals.length > 0;
    
    playBtn.disabled = !hasSelection || isPlaying;
    stopBtn.disabled = !isPlaying;
    
    loopBtn.classList.toggle('active', isLooping);
}

/**
 * Select a signal for preview playback
 */
async function selectSignalForPreview(id) {
    try {
        const success = await backend.call('selectReferenceSignalForPreview', id);
        
        if (success) {
            selectedSignalId = id;
            
            // Update visual selection
            document.querySelectorAll('.reference-signal-row').forEach(row => {
                row.classList.toggle('selected', row.dataset.id === id);
            });
            
            updatePlaybackButtonStates();
        }
    } catch (error) {
        console.error('Failed to select signal for preview:', error);
    }
}

/**
 * Handle tail duration change
 */
async function onTailChange(id, tailMs) {
    try {
        const actualTail = await backend.call('setReferenceSignalTail', id, tailMs);
        
        if (actualTail >= 0) {
            // Update local state
            const signal = referenceSignals.find(s => s.id === id);
            if (signal) {
                signal.tailMs = actualTail;
            }
        }
    } catch (error) {
        console.error('Failed to set signal tail:', error);
    }
}

/**
 * Remove a signal from the list
 */
async function removeSignal(id) {
    try {
        const success = await backend.call('removeReferenceSignal', id);
        
        if (success) {
            referenceSignals = referenceSignals.filter(s => s.id !== id);
            
            // If removed signal was selected, clear selection
            if (selectedSignalId === id) {
                selectedSignalId = null;
            }
            
            renderSignalList();
        }
    } catch (error) {
        console.error('Failed to remove signal:', error);
    }
}

/**
 * Browse and add signals
 */
async function browseAndAddSignals() {
    const browseBtn = document.getElementById('browse-reference-btn');
    
    try {
        browseBtn.disabled = true;
        
        const result = await backend.call('browseAndAddReferenceSignals');
        
        if (result.cancelled) {
            return;
        }
        
        // Update local state with all signals from backend
        if (result.signals && result.signals.signals) {
            referenceSignals = result.signals.signals;
            selectedSignalId = result.signals.selectedId || null;
            isPlaying = result.signals.isPlaying || false;
            isLooping = result.signals.isLooping || false;
        }
        
        renderSignalList();
        
        // Show errors if any
        if (result.errors && result.errors.length > 0) {
            const errorMessages = result.errors.map(e => `${e.fileName}: ${e.errorMessage}`).join('\n');
            showReferenceSignalError(errorMessages);
        }
        
    } catch (error) {
        console.error('Failed to browse signals:', error);
        showReferenceSignalError('Failed to open file picker');
    } finally {
        browseBtn.disabled = false;
    }
}

/**
 * Show error message
 */
function showReferenceSignalError(message) {
    const errorEl = document.getElementById('reference-signal-error');
    errorEl.textContent = message;
    errorEl.classList.remove('hidden');
    
    // Auto-hide after 5 seconds
    setTimeout(() => {
        errorEl.classList.add('hidden');
    }, 5000);
}

/**
 * Start preview playback
 */
async function playReferenceSignal() {
    const playBtn = document.getElementById('play-reference-btn');
    
    try {
        playBtn.disabled = true;
        
        const success = await backend.call('startReferencePlayback');
        
        if (success) {
            isPlaying = true;
            updatePlaybackButtonStates();
        }
    } catch (error) {
        console.error('Failed to play signal:', error);
    }
}

/**
 * Stop preview playback
 */
async function stopReferenceSignal() {
    const stopBtn = document.getElementById('stop-reference-btn');
    
    try {
        stopBtn.disabled = true;
        
        await backend.call('stopReferencePlayback');
        
        isPlaying = false;
        updatePlaybackButtonStates();
    } catch (error) {
        console.error('Failed to stop signal:', error);
    }
}

/**
 * Toggle loop mode
 */
async function toggleLoopReferenceSignal() {
    try {
        const newLoopState = !isLooping;
        await backend.call('setReferencePlaybackLoop', newLoopState);
        
        isLooping = newLoopState;
        updatePlaybackButtonStates();
    } catch (error) {
        console.error('Failed to toggle loop:', error);
    }
}

/**
 * Handle playback state changes from backend
 */
function onPlaybackStateChanged(data) {
    if (isPlaying !== data.isPlaying) {
        isPlaying = data.isPlaying;
        updatePlaybackButtonStates();
    }
}

/**
 * Load signals state from backend
 */
async function loadSignalsFromBackend() {
    try {
        const state = await backend.call('getReferenceSignals');
        
        if (state && state.signals) {
            referenceSignals = state.signals;
            selectedSignalId = state.selectedId || null;
            isPlaying = state.isPlaying || false;
            isLooping = state.isLooping || false;
        }
        
        renderSignalList();
    } catch (error) {
        console.error('Failed to load signals:', error);
    }
}

/**
 * Update capture section based on reference signals
 * Called when signals change
 */
function updateCaptureForReferenceSignals() {
    // Enable/disable start capture button based on having signals
    const startBtn = document.getElementById('start-capture-btn');
    if (startBtn) {
        // This will be further controlled by capture.js
        // We just ensure the basic prerequisite is met
        if (referenceSignals.length === 0) {
            startBtn.disabled = true;
        }
    }
    
    // Update total duration display if capture.js provides a function for it
    if (typeof updateCaptureForReferenceSignal === 'function') {
        // For backwards compatibility, pass first signal's info
        if (referenceSignals.length > 0) {
            const totalDuration = referenceSignals.reduce((sum, s) => {
                return sum + s.durationSeconds + (s.tailMs / 1000) + 0.05; // +50ms delay
            }, 0);
            
            // Create a pseudo-state for capture.js
            window.referenceSignalState = {
                loaded: true,
                durationSeconds: totalDuration,
                signals: referenceSignals
            };
        } else {
            window.referenceSignalState = {
                loaded: false,
                durationSeconds: 0,
                signals: []
            };
        }
    }
}

/**
 * Set up drag and drop handlers
 */
function setupDragAndDrop() {
    const dropZone = document.getElementById('reference-signals-drop-zone');
    
    if (!dropZone) return;
    
    // Prevent default drag behaviors
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        dropZone.addEventListener(eventName, (e) => {
            e.preventDefault();
            e.stopPropagation();
        });
    });
    
    // Highlight on drag enter/over
    ['dragenter', 'dragover'].forEach(eventName => {
        dropZone.addEventListener(eventName, () => {
            dropZone.classList.add('drag-over');
        });
    });
    
    // Remove highlight on drag leave/drop
    ['dragleave', 'drop'].forEach(eventName => {
        dropZone.addEventListener(eventName, () => {
            dropZone.classList.remove('drag-over');
        });
    });
    
    // Handle drop
    dropZone.addEventListener('drop', async (e) => {
        const files = e.dataTransfer.files;
        
        if (files.length === 0) return;
        
        // Check if we have file paths (not available in standard WebView)
        // In JUCE WebView on macOS, file.path may not be exposed
        const firstFile = files[0];
        if (!firstFile.path) {
            // File paths not available - direct user to use Browse button
            showReferenceSignalError('Drag & drop not supported in this context. Please use the Browse button to add files.');
            return;
        }
        
        // Add each WAV file
        const errors = [];
        
        for (const file of files) {
            // Check file extension
            if (!file.name.toLowerCase().endsWith('.wav')) {
                errors.push(`${file.name}: Only WAV files are supported`);
                continue;
            }
            
            const filePath = file.path;
            
            try {
                const result = await backend.call('addReferenceSignal', filePath);
                
                if (!result.success) {
                    errors.push(`${file.name}: ${result.errorMessage}`);
                } else if (result.signal) {
                    referenceSignals.push(result.signal);
                }
            } catch (error) {
                errors.push(`${file.name}: ${error.message || 'Unknown error'}`);
            }
        }
        
        renderSignalList();
        
        if (errors.length > 0) {
            showReferenceSignalError(errors.join('\n'));
        }
    });
}

/**
 * Get reference signals for capture module
 */
function getReferenceSignals() {
    return referenceSignals;
}

/**
 * Initialize reference signals module
 */
function initReferenceSignal() {
    const browseBtn = document.getElementById('browse-reference-btn');
    const playBtn = document.getElementById('play-reference-btn');
    const stopBtn = document.getElementById('stop-reference-btn');
    const loopBtn = document.getElementById('loop-reference-btn');
    
    browseBtn.addEventListener('click', browseAndAddSignals);
    playBtn.addEventListener('click', playReferenceSignal);
    stopBtn.addEventListener('click', stopReferenceSignal);
    loopBtn.addEventListener('click', toggleLoopReferenceSignal);
    
    setupDragAndDrop();
    
    // Load initial state
    loadSignalsFromBackend();
}

// Export for use by other modules
window.getReferenceSignals = getReferenceSignals;
window.referenceSignalState = { loaded: false, durationSeconds: 0, signals: [] };
