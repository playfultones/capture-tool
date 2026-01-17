// Capture module - output folder, progress, controls/matrix, list, current capture

//==============================================================================
// Output Folder Selection

/**
 * Output folder state
 */
const outputFolderState = {
    path: '',
    isValid: false
};

/**
 * Update the output folder display
 */
function updateOutputFolderDisplay() {
    const displayEl = document.getElementById('output-folder-display');
    const pathEl = document.getElementById('output-folder-path');
    const errorEl = document.getElementById('output-folder-error');
    const revealBtn = document.getElementById('reveal-output-folder-btn');
    
    // Hide any previous error
    errorEl.classList.add('hidden');
    
    if (outputFolderState.path && outputFolderState.isValid) {
        pathEl.textContent = outputFolderState.path;
        displayEl.classList.add('has-folder');
        revealBtn.classList.remove('hidden');
    } else if (outputFolderState.path && !outputFolderState.isValid) {
        pathEl.textContent = outputFolderState.path;
        displayEl.classList.remove('has-folder');
        revealBtn.classList.add('hidden');
        showOutputFolderError('Folder is not writable');
    } else {
        pathEl.textContent = 'No folder selected';
        displayEl.classList.remove('has-folder');
        revealBtn.classList.add('hidden');
    }
}

/**
 * Show an error message for output folder
 * @param {string} message 
 */
function showOutputFolderError(message) {
    const errorEl = document.getElementById('output-folder-error');
    errorEl.textContent = message;
    errorEl.classList.remove('hidden');
}

/**
 * Handle browse button click - open folder picker
 */
async function browseOutputFolder() {
    const btn = document.getElementById('browse-output-folder-btn');
    
    try {
        // Disable button during folder picker
        btn.disabled = true;
        
        const result = await backend.call('browseOutputFolder');
        
        if (result.cancelled) {
            // User cancelled, do nothing
            return;
        }
        
        if (result.success) {
            // Update state
            outputFolderState.path = result.folderPath;
            outputFolderState.isValid = result.isWritable;
            
            updateOutputFolderDisplay();
        } else {
            // Show error
            showOutputFolderError(result.errorMessage || 'Failed to select folder');
        }
    } catch (error) {
        console.error('Failed to browse output folder:', error);
        showOutputFolderError('Failed to open folder picker');
    } finally {
        btn.disabled = false;
    }
}

/**
 * Load initial output folder state from backend
 */
async function loadOutputFolderState() {
    try {
        const state = await backend.call('getOutputFolderState');
        
        if (state.path) {
            outputFolderState.path = state.path;
            outputFolderState.isValid = state.isWritable;
        }
        
        updateOutputFolderDisplay();
    } catch (error) {
        console.error('Failed to load output folder state:', error);
    }
}

/**
 * Reveal the output folder in system file browser
 */
async function revealOutputFolder() {
    if (!outputFolderState.path) {
        return;
    }
    
    try {
        await backend.call('revealOutputFolder');
    } catch (error) {
        console.error('Failed to reveal output folder:', error);
    }
}

/**
 * Initialize output folder UI
 */
function initOutputFolder() {
    const browseBtn = document.getElementById('browse-output-folder-btn');
    const revealBtn = document.getElementById('reveal-output-folder-btn');
    
    browseBtn.addEventListener('click', browseOutputFolder);
    revealBtn.addEventListener('click', revealOutputFolder);
    
    // Load initial state
    loadOutputFolderState();
}

//==============================================================================
// Capture Progress UI

/**
 * Capture state enum (mirrors C++ CaptureState)
 */
const CaptureState = {
    IDLE: 'idle',
    RECORDING: 'recording',
    DONE: 'done'
};

/**
 * Capture state tracking
 */
const captureState = {
    state: CaptureState.IDLE,
    startTime: 0,
    elapsedMs: 0,
    totalDurationMs: 0,
    progressPercent: 0
};

// Progress update interval ID
let captureProgressInterval = null;

/**
 * Update capture UI based on current state
 */
function updateCaptureUI() {
    const progressFill = document.getElementById('capture-progress-fill');
    const elapsedTime = document.getElementById('capture-elapsed-time');
    const totalTime = document.getElementById('capture-total-time');
    const startBtn = document.getElementById('start-capture-btn');
    const abortBtn = document.getElementById('abort-capture-btn');
    const completeIndicator = document.getElementById('capture-complete-indicator');
    
    // Check if current capture item is already complete (for showing indicator)
    const currentItem = captureListState.items[currentCaptureState.index];
    const currentItemComplete = currentItem && currentItem.status === 'complete';
    
    // Check if current capture is pending (ready to capture)
    const currentItemPending = currentItem && currentItem.status === 'pending';
    
    switch (captureState.state) {
        case CaptureState.IDLE:
            progressFill.style.width = '0%';
            elapsedTime.textContent = '0:00.0';
            totalTime.textContent = formatCaptureTime(captureState.totalDurationMs);
            startBtn.classList.remove('hidden');
            // Only enable Start Capture if reference is loaded AND current capture is pending
            startBtn.disabled = !referenceSignalState.loaded || !currentItemPending;
            abortBtn.classList.add('hidden');
            // Only show complete indicator if viewing an already-captured item
            if (currentItemComplete) {
                completeIndicator.classList.remove('hidden');
            } else {
                completeIndicator.classList.add('hidden');
            }
            break;
            
        case CaptureState.RECORDING:
            progressFill.style.width = `${captureState.progressPercent}%`;
            elapsedTime.textContent = formatCaptureTime(captureState.elapsedMs);
            startBtn.classList.add('hidden');
            abortBtn.classList.remove('hidden');
            completeIndicator.classList.add('hidden');
            break;
            
        case CaptureState.DONE:
            // Transient done state - will auto-advance to next capture
            progressFill.style.width = '100%';
            elapsedTime.textContent = formatCaptureTime(captureState.totalDurationMs);
            startBtn.classList.remove('hidden');
            startBtn.disabled = !referenceSignalState.loaded || !currentItemPending;
            abortBtn.classList.add('hidden');
            // Don't show complete indicator here - we'll auto-advance
            completeIndicator.classList.add('hidden');
            break;
    }
}

/**
 * Update progress during capture (called from interval)
 */
function updateCaptureProgress() {
    if (captureState.state !== CaptureState.RECORDING) {
        return;
    }
    
    // Calculate elapsed time since capture started
    captureState.elapsedMs = Date.now() - captureState.startTime;
    
    // Calculate progress percentage
    if (captureState.totalDurationMs > 0) {
        captureState.progressPercent = Math.min(
            (captureState.elapsedMs / captureState.totalDurationMs) * 100,
            100
        );
    }
    
    updateCaptureUI();
}

/**
 * Start capture progress timer
 */
function startCaptureProgressTimer() {
    // Clear any existing interval
    stopCaptureProgressTimer();
    
    // Update every 100ms for smooth progress
    captureProgressInterval = setInterval(updateCaptureProgress, 100);
}

/**
 * Stop capture progress timer
 */
function stopCaptureProgressTimer() {
    if (captureProgressInterval !== null) {
        clearInterval(captureProgressInterval);
        captureProgressInterval = null;
    }
}

/**
 * Handle capture state change event from backend
 * @param {object} data - { state: string, elapsedMs: number, totalDurationMs: number, signalIndex: number, signalCount: number, signalName: string }
 */
function onCaptureStateChanged(data) {
    // Map backend state string to our state enum
    switch (data.state) {
        case 'idle':
            captureState.state = CaptureState.IDLE;
            stopCaptureProgressTimer();
            hideSignalIndicator();
            break;
        case 'recording':
            captureState.state = CaptureState.RECORDING;
            captureState.startTime = Date.now();
            captureState.elapsedMs = 0;
            if (data.totalDurationMs) {
                captureState.totalDurationMs = data.totalDurationMs;
            }
            startCaptureProgressTimer();
            // Update signal indicator if multiple signals
            if (data.signalCount > 1) {
                updateSignalIndicator(data.signalIndex, data.signalCount, data.signalName);
            } else {
                hideSignalIndicator();
            }
            break;
        case 'done':
            captureState.state = CaptureState.DONE;
            stopCaptureProgressTimer();
            // Set elapsed to total for final display
            captureState.elapsedMs = captureState.totalDurationMs;
            captureState.progressPercent = 100;
            break;
    }
    
    updateCaptureUI();
    
    // Update current capture display status
    onCaptureStateChangedForCurrentCapture(data);
}

/**
 * Update the signal indicator display
 */
function updateSignalIndicator(signalIndex, signalCount, signalName) {
    const indicator = document.getElementById('capture-signal-indicator');
    const indexEl = document.getElementById('capture-signal-index');
    const countEl = document.getElementById('capture-signal-count');
    const nameEl = document.getElementById('capture-signal-name');
    
    if (indicator && indexEl && countEl && nameEl) {
        indexEl.textContent = String(signalIndex + 1); // 0-based to 1-based
        countEl.textContent = String(signalCount);
        nameEl.textContent = signalName || '';
        indicator.classList.remove('hidden');
    }
}

/**
 * Hide the signal indicator
 */
function hideSignalIndicator() {
    const indicator = document.getElementById('capture-signal-indicator');
    if (indicator) {
        indicator.classList.add('hidden');
    }
}

/**
 * Handle capture complete event from backend
 * @param {object} data - { success: boolean, errorMessage: string, outputFilePath: string, durationSeconds: number, hasMoreSignals: boolean, signalIndex: number, signalCount: number }
 */
function onCaptureComplete(data) {
    // If there are more signals, the backend will auto-start the next one
    // Don't fully reset state in that case
    if (data.hasMoreSignals) {
        // Just update progress for this signal, backend will send recording state for next
        console.log(`Signal ${data.signalIndex + 1} of ${data.signalCount} complete, waiting for next...`);
        return;
    }
    
    stopCaptureProgressTimer();
    hideSignalIndicator();
    
    if (data.success) {
        captureState.state = CaptureState.DONE;
        captureState.progressPercent = 100;
        captureState.elapsedMs = captureState.totalDurationMs;
    } else {
        captureState.state = CaptureState.IDLE;
        console.error('Capture failed:', data.errorMessage);
    }
    
    updateCaptureUI();
    
    // Update current capture display
    onCaptureCompleteForCurrentCapture(data);
}

/**
 * Start a capture
 * With multiple signals, this starts the first signal for the current capture item.
 * The backend will orchestrate cycling through all signals.
 */
async function startCapture() {
    const startBtn = document.getElementById('start-capture-btn');
    
    // Check if we have reference signals
    const signals = window.referenceSignalState?.signals || [];
    if (signals.length === 0) {
        console.error('Cannot start capture - no reference signals loaded');
        return;
    }
    
    // Get the current capture item ID
    const currentItem = captureListState.items[currentCaptureState.index];
    if (!currentItem) {
        console.error('Cannot start capture - no capture item selected');
        return;
    }
    
    try {
        startBtn.disabled = true;
        
        // Calculate total duration: sum of all signals + their tails + delays
        const preDelayMs = 50;
        let totalDurationMs = 0;
        for (const signal of signals) {
            totalDurationMs += (signal.durationSeconds * 1000) + preDelayMs + signal.tailMs;
        }
        captureState.totalDurationMs = totalDurationMs;
        
        // Update total time display before starting
        document.getElementById('capture-total-time').textContent = formatCaptureTime(captureState.totalDurationMs);
        
        // Start capture with first signal (backend handles the orchestration)
        // Pass capture item ID only - backend will use first signal
        const result = await backend.call('startCapture', currentItem.id);
        
        if (!result.success) {
            console.error('Failed to start capture:', result.errorMessage);
            // Show error to user
            alert('Capture failed: ' + (result.errorMessage || 'Unknown error'));
            startBtn.disabled = false;
            return;
        }
        
        // The backend will send captureStateChanged event, but we can update immediately for responsiveness
        captureState.state = CaptureState.RECORDING;
        captureState.startTime = Date.now();
        captureState.elapsedMs = 0;
        captureState.progressPercent = 0;
        startCaptureProgressTimer();
        updateCaptureUI();
        
    } catch (error) {
        console.error('Failed to start capture:', error);
        startBtn.disabled = false;
    }
}

/**
 * Abort the current capture
 */
async function abortCapture() {
    const abortBtn = document.getElementById('abort-capture-btn');
    
    try {
        abortBtn.disabled = true;
        
        await backend.call('abortCapture');
        
        // Reset to idle state
        stopCaptureProgressTimer();
        captureState.state = CaptureState.IDLE;
        captureState.elapsedMs = 0;
        captureState.progressPercent = 0;
        updateCaptureUI();
        
        // Reset the current capture item back to pending
        const currentItem = captureListState.items[currentCaptureState.index];
        if (currentItem && currentItem.status === 'recording') {
            // Update backend status to pending
            const result = await backend.call('setCaptureItemStatus', currentItem.id, 'pending');
            if (result.success) {
                captureListState.items = result.captureList;
            }
            
            // Update local state
            currentCaptureState.status = 'pending';
            updateCaptureListDisplay();
            updateCurrentCaptureDisplay();
        }
        
    } catch (error) {
        console.error('Failed to abort capture:', error);
    } finally {
        abortBtn.disabled = false;
    }
}

/**
 * Initialize capture UI
 */
function initCapture() {
    const startBtn = document.getElementById('start-capture-btn');
    const abortBtn = document.getElementById('abort-capture-btn');
    
    startBtn.addEventListener('click', startCapture);
    abortBtn.addEventListener('click', abortCapture);
    
    // Enable/disable start button based on reference signals
    const signals = window.referenceSignalState?.signals || [];
    startBtn.disabled = signals.length === 0;
    
    // Set initial total time based on reference signals if any
    if (signals.length > 0) {
        const preDelayMs = 50;
        let totalDurationMs = 0;
        for (const signal of signals) {
            totalDurationMs += (signal.durationSeconds * 1000) + preDelayMs + signal.tailMs;
        }
        captureState.totalDurationMs = totalDurationMs;
        document.getElementById('capture-total-time').textContent = formatCaptureTime(captureState.totalDurationMs);
    }
}

/**
 * Update capture UI when reference signals change
 */
function updateCaptureForReferenceSignal() {
    const startBtn = document.getElementById('start-capture-btn');
    const signals = window.referenceSignalState?.signals || [];
    
    if (signals.length > 0) {
        // Calculate and display total duration for all signals
        const preDelayMs = 50;
        let totalDurationMs = 0;
        for (const signal of signals) {
            totalDurationMs += (signal.durationSeconds * 1000) + preDelayMs + signal.tailMs;
        }
        captureState.totalDurationMs = totalDurationMs;
        document.getElementById('capture-total-time').textContent = formatCaptureTime(captureState.totalDurationMs);
        
        // Enable start button only if:
        // 1. Not currently capturing
        // 2. Current capture item is in "pending" state
        const currentItem = captureListState.items[currentCaptureState.index];
        const currentItemPending = currentItem && currentItem.status === 'pending';
        
        if (captureState.state === CaptureState.IDLE && currentItemPending) {
            startBtn.disabled = false;
        } else {
            startBtn.disabled = true;
        }
    } else {
        captureState.totalDurationMs = 0;
        document.getElementById('capture-total-time').textContent = '0:00.0';
        startBtn.disabled = true;
    }
}

//==============================================================================
// Capture Controls (Matrix Definition)

/**
 * Capture controls state
 */
const captureControlsState = {
    controls: [],
    totalCaptureCount: 0
};

/**
 * Render a single capture control item
 * @param {object} control - { id, name, type, values, valueCount }
 * @returns {string} HTML string
 */
function renderCaptureControlItem(control) {
    const valuesPlaceholder = control.type === 'continuous' 
        ? '0-10:1 (start-end:step)' 
        : 'Low, Mid, High';
    
    // Ensure values is an array
    const values = Array.isArray(control.values) ? control.values : [];
    
    const valuesDisplay = control.type === 'continuous'
        ? (values.length > 0 ? `${values[0]}-${values[values.length - 1]}` : '')
        : values.join(', ');
    
    return `
        <div class="capture-control-item" data-control-id="${control.id}">
            <input type="text" 
                   class="control-name-input" 
                   placeholder="Control name"
                   value="${escapeHtml(control.name)}"
                   data-field="name">
            <select class="control-type-select" data-field="type">
                <option value="discrete" ${control.type === 'discrete' ? 'selected' : ''}>Discrete</option>
                <option value="continuous" ${control.type === 'continuous' ? 'selected' : ''}>Continuous</option>
            </select>
            <input type="text" 
                   class="control-values-input" 
                   placeholder="${valuesPlaceholder}"
                   value="${escapeHtml(valuesDisplay)}"
                   data-field="values">
            <span class="control-value-count">${control.valueCount} val${control.valueCount !== 1 ? 's' : ''}</span>
            <button class="btn-remove-control" title="Remove control">x</button>
        </div>
    `;
}

/**
 * Update the capture controls display
 */
function updateCaptureControlsDisplay() {
    const listEl = document.getElementById('capture-controls-list');
    const countEl = document.getElementById('total-capture-count');
    
    if (!listEl || !countEl) {
        console.error('Capture controls DOM elements not found');
        return;
    }
    
    if (!captureControlsState.controls || captureControlsState.controls.length === 0) {
        listEl.innerHTML = `
            <div class="empty-controls-message">
                No controls defined. Add a control to define your capture matrix.
            </div>
        `;
        countEl.textContent = '0';
        countEl.classList.remove('warning', 'error');
    } else {
        // Render all controls
        const controlsHtml = captureControlsState.controls
            .map(ctrl => renderCaptureControlItem(ctrl))
            .join('');
        
        listEl.innerHTML = controlsHtml;
        
        // Re-attach event listeners
        attachControlEventListeners();
        
        // Update total count
        const count = captureControlsState.totalCaptureCount || 0;
        countEl.textContent = count.toLocaleString();
        
        // Add warning/error classes based on count
        countEl.classList.remove('warning', 'error');
        if (count > 10000) {
            countEl.classList.add('error');
        } else if (count > 1000) {
            countEl.classList.add('warning');
        }
    }
    
    // Update capture list buttons (generate/clear)
    if (typeof updateCaptureListButtons === 'function') {
        updateCaptureListButtons();
    }
}

/**
 * Attach event listeners to control items
 */
function attachControlEventListeners() {
    const listEl = document.getElementById('capture-controls-list');
    
    // Name input change
    listEl.querySelectorAll('.control-name-input').forEach(input => {
        input.addEventListener('change', onControlFieldChange);
    });
    
    // Type select change
    listEl.querySelectorAll('.control-type-select').forEach(select => {
        select.addEventListener('change', onControlTypeChange);
    });
    
    // Values input change
    listEl.querySelectorAll('.control-values-input').forEach(input => {
        input.addEventListener('change', onControlFieldChange);
    });
    
    // Remove button click
    listEl.querySelectorAll('.btn-remove-control').forEach(btn => {
        btn.addEventListener('click', onRemoveControlClick);
    });
}

/**
 * Handle control field change (name or values)
 * @param {Event} event 
 */
async function onControlFieldChange(event) {
    const controlItem = event.target.closest('.capture-control-item');
    const controlId = controlItem.dataset.controlId;
    
    const nameInput = controlItem.querySelector('[data-field="name"]');
    const typeSelect = controlItem.querySelector('[data-field="type"]');
    const valuesInput = controlItem.querySelector('[data-field="values"]');
    
    try {
        const result = await backend.call(
            'updateCaptureControl',
            controlId,
            nameInput.value,
            typeSelect.value,
            valuesInput.value
        );
        
        if (result.success) {
            captureControlsState.controls = result.controls;
            captureControlsState.totalCaptureCount = result.totalCaptureCount;
            updateCaptureControlsDisplay();
        }
    } catch (error) {
        console.error('Failed to update control:', error);
    }
}

/**
 * Handle control type change
 * @param {Event} event 
 */
async function onControlTypeChange(event) {
    const controlItem = event.target.closest('.capture-control-item');
    const valuesInput = controlItem.querySelector('[data-field="values"]');
    
    // Update placeholder based on type
    const newType = event.target.value;
    valuesInput.placeholder = newType === 'continuous' 
        ? '0-10:1 (start-end:step)' 
        : 'Low, Mid, High';
    
    // Clear the values input when switching types
    valuesInput.value = '';
    
    // Trigger update
    await onControlFieldChange(event);
}

/**
 * Handle remove control button click
 * @param {Event} event 
 */
async function onRemoveControlClick(event) {
    const controlItem = event.target.closest('.capture-control-item');
    const controlId = controlItem.dataset.controlId;
    
    try {
        const result = await backend.call('removeCaptureControl', controlId);
        
        if (result.success) {
            captureControlsState.controls = result.controls;
            captureControlsState.totalCaptureCount = result.totalCaptureCount;
            updateCaptureControlsDisplay();
        }
    } catch (error) {
        console.error('Failed to remove control:', error);
    }
}

/**
 * Handle add control button click
 */
async function onAddControlClick() {
    const addBtn = document.getElementById('add-control-btn');
    
    try {
        // Disable button during operation
        if (addBtn) addBtn.disabled = true;
        
        const result = await backend.call('addCaptureControl', '', 'discrete', '0');
        
        if (result && result.success) {
            captureControlsState.controls = result.controls || [];
            captureControlsState.totalCaptureCount = result.totalCaptureCount || 0;
            updateCaptureControlsDisplay();
            
            // Focus the name input of the new control
            const newControlEl = document.querySelector(`[data-control-id="${result.id}"]`);
            if (newControlEl) {
                const nameInput = newControlEl.querySelector('.control-name-input');
                nameInput.focus();
                nameInput.select();
            }
        } else {
            console.error('Add control failed or returned unexpected result:', result);
        }
    } catch (error) {
        console.error('Failed to add control:', error);
    } finally {
        // Re-enable button
        if (addBtn) addBtn.disabled = false;
    }
}

/**
 * Load capture controls from backend
 */
async function loadCaptureControls() {
    try {
        const controls = await backend.call('getCaptureControls');
        const totalCount = await backend.call('getTotalCaptureCount');
        
        captureControlsState.controls = controls || [];
        captureControlsState.totalCaptureCount = totalCount || 0;
        
        updateCaptureControlsDisplay();
    } catch (error) {
        console.error('Failed to load capture controls:', error);
    }
}

/**
 * Initialize capture controls UI
 */
function initCaptureControls() {
    const addBtn = document.getElementById('add-control-btn');
    if (addBtn) {
        addBtn.addEventListener('click', onAddControlClick);
    }
    
    // Load initial state
    loadCaptureControls();
}

//==============================================================================
// Current Capture Display

/**
 * Current capture display state
 */
const currentCaptureState = {
    index: 0,         // 0-based index into capture list
    total: 0,         // Total number of captures
    controlValues: {}, // Map of control name -> value
    status: 'pending' // 'pending', 'recording', 'complete'
};

/**
 * Update the filename preview for the current capture item
 */
async function updateFilenamePreview() {
    const filenameEl = document.getElementById('filename-preview-value');
    if (!filenameEl) return;
    
    // Get current capture item
    const currentItem = captureListState.items[currentCaptureState.index];
    
    if (!currentItem) {
        filenameEl.textContent = '--';
        filenameEl.className = 'filename-preview-value';
        return;
    }
    
    try {
        const result = await backend.call('getExpectedCaptureFilename', currentItem.id);
        
        if (result.filename) {
            filenameEl.textContent = result.filename;
            filenameEl.className = 'filename-preview-value';
            
            // Add warning if no output folder configured
            if (!result.hasOutputFolder) {
                filenameEl.className = 'filename-preview-value warning';
                filenameEl.title = 'No output folder configured - select one in Session Settings';
            } else {
                filenameEl.title = result.fullPath || '';
            }
        } else {
            filenameEl.textContent = '--';
            filenameEl.className = 'filename-preview-value';
        }
    } catch (error) {
        console.error('Failed to get filename preview:', error);
        filenameEl.textContent = '--';
        filenameEl.className = 'filename-preview-value';
    }
}

/**
 * Update the current capture display panel
 */
function updateCurrentCaptureDisplay() {
    const displayEl = document.getElementById('current-capture-display');
    const noListMessage = document.getElementById('no-capture-list-message');
    const indexEl = document.getElementById('current-capture-index');
    const totalEl = document.getElementById('current-capture-total');
    const settingsEl = document.getElementById('current-capture-settings');
    const statusBadge = document.getElementById('current-capture-status-badge');
    const instructionsEl = document.querySelector('.current-capture-instructions');
    const instructionsText = instructionsEl?.querySelector('.instructions-text');
    const skipBtn = document.getElementById('skip-capture-btn');
    const recaptureBtn = document.getElementById('recapture-btn');
    const startBtn = document.getElementById('start-capture-btn');
    
    // Show/hide based on whether we have a capture list
    if (captureListState.items.length === 0) {
        displayEl.classList.add('hidden');
        noListMessage.classList.remove('hidden');
        return;
    }
    
    displayEl.classList.remove('hidden');
    noListMessage.classList.add('hidden');
    
    // Update capture number display
    currentCaptureState.total = captureListState.items.length;
    indexEl.textContent = currentCaptureState.index + 1;
    totalEl.textContent = currentCaptureState.total;
    
    // Get current capture item
    const currentItem = captureListState.items[currentCaptureState.index];
    
    // Update control values display
    if (currentItem && currentItem.controlValues) {
        currentCaptureState.controlValues = currentItem.controlValues;
        
        const controlNames = Object.keys(currentItem.controlValues);
        const fewControls = controlNames.length <= 4;
        
        // Build setting cards HTML
        let settingsHtml = '';
        for (const name of controlNames) {
            const value = currentItem.controlValues[name];
            settingsHtml += `
                <div class="capture-setting-card">
                    <span class="capture-setting-name">${escapeHtml(name)}</span>
                    <span class="capture-setting-value">${escapeHtml(value)}</span>
                </div>
            `;
        }
        
        settingsEl.innerHTML = settingsHtml;
        settingsEl.classList.toggle('few-controls', fewControls);
    } else {
        settingsEl.innerHTML = '<div class="current-capture-empty">No capture selected</div>';
        settingsEl.classList.remove('few-controls');
    }
    
    // Update status badge and workflow controls based on current status
    statusBadge.className = 'capture-status-badge';
    instructionsEl.classList.remove('recording');
    
    // Default: hide all workflow buttons
    skipBtn.classList.add('hidden');
    recaptureBtn.classList.add('hidden');
    
    // Disable start capture by default
    startBtn.disabled = true;
    
    switch (currentCaptureState.status) {
        case 'pending':
            statusBadge.textContent = 'Pending';
            statusBadge.classList.add('status-pending');
            if (instructionsText) {
                instructionsText.textContent = 'Dial in the settings above on your hardware, then start capture';
            }
            // Show Skip button, enable Start Capture if reference signal is loaded
            skipBtn.classList.remove('hidden');
            if (referenceSignalState.loaded) {
                startBtn.disabled = false;
            }
            break;
        case 'recording':
            statusBadge.textContent = 'Recording';
            statusBadge.classList.add('status-recording');
            instructionsEl.classList.add('recording');
            if (instructionsText) {
                instructionsText.textContent = 'Recording in progress...';
            }
            // No workflow buttons during recording
            break;
        case 'complete':
            statusBadge.textContent = 'Complete';
            statusBadge.classList.add('status-complete');
            if (instructionsText) {
                instructionsText.textContent = 'Capture complete! Click Re-capture or move to next.';
            }
            // Show Re-capture and Skip buttons
            recaptureBtn.classList.remove('hidden');
            skipBtn.classList.remove('hidden');
            break;
    }
    
    // Update filename preview (async, doesn't block UI)
    updateFilenamePreview();
}

/**
 * Set the current capture index and update display
 * @param {number} index - 0-based capture index
 */
function setCurrentCaptureIndex(index) {
    if (index < 0 || index >= captureListState.items.length) {
        return;
    }
    
    currentCaptureState.index = index;
    
    // Set status from the capture item
    const item = captureListState.items[index];
    if (item) {
        currentCaptureState.status = item.status || 'pending';
    } else {
        currentCaptureState.status = 'pending';
    }
    
    updateCurrentCaptureDisplay();
    
    // Scroll to current item in capture list table (highlight it)
    highlightCurrentCaptureInList();
}

/**
 * Advance to the next pending (or ready) capture
 * @returns {boolean} True if advanced, false if no more pending/ready captures
 */
function advanceToNextCapture() {
    // Find the next pending or ready capture after current index
    for (let i = currentCaptureState.index + 1; i < captureListState.items.length; i++) {
        const status = captureListState.items[i].status;
        if (status === 'pending') {
            setCurrentCaptureIndex(i);
            return true;
        }
    }
    
    // Wrap around to find any pending capture from the beginning
    for (let i = 0; i < currentCaptureState.index; i++) {
        const status = captureListState.items[i].status;
        if (status === 'pending') {
            setCurrentCaptureIndex(i);
            return true;
        }
    }
    
    // No more pending captures
    return false;
}

/**
 * Skip the current capture (move to next pending capture)
 */
function skipCurrentCapture() {
    // Simply advance to next pending capture
    const advanced = advanceToNextCapture();
    
    if (!advanced) {
        // No more captures to advance to - stay on current
        console.log('No more pending captures to skip to');
    }
}

/**
 * Re-capture: reset a completed capture back to pending state
 */
async function recaptureCurrentCapture() {
    const currentItem = captureListState.items[currentCaptureState.index];
    if (!currentItem || currentItem.status !== 'complete') {
        return;
    }
    
    const recaptureBtn = document.getElementById('recapture-btn');
    
    try {
        recaptureBtn.disabled = true;
        
        const result = await backend.call('setCaptureItemStatus', currentItem.id, 'pending');
        
        if (result.success) {
            // Update local state
            captureListState.items = result.captureList;
            currentCaptureState.status = 'pending';
            
            // Update displays
            updateCaptureListDisplay();
            updateCurrentCaptureDisplay();
        }
    } catch (error) {
        console.error('Failed to reset capture for re-capture:', error);
    } finally {
        recaptureBtn.disabled = false;
    }
}

/**
 * Highlight the current capture row in the list table
 */
function highlightCurrentCaptureInList() {
    const tbody = document.getElementById('capture-list-tbody');
    if (!tbody) return;
    
    // Remove highlight from all rows
    tbody.querySelectorAll('tr').forEach(row => {
        row.classList.remove('current-capture-row');
    });
    
    // Add highlight to current row
    const currentItem = captureListState.items[currentCaptureState.index];
    if (currentItem) {
        const currentRow = tbody.querySelector(`tr[data-capture-id="${currentItem.id}"]`);
        if (currentRow) {
            currentRow.classList.add('current-capture-row');
            // Scroll row into view if needed
            currentRow.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
    }
}

/**
 * Handle capture state changed - update current capture display status
 * @param {object} data - Capture state from backend
 */
function onCaptureStateChangedForCurrentCapture(data) {
    switch (data.state) {
        case 'recording':
            currentCaptureState.status = 'recording';
            // Mark current item as recording in the list
            if (captureListState.items[currentCaptureState.index]) {
                captureListState.items[currentCaptureState.index].status = 'recording';
                updateCaptureListDisplay();
            }
            break;
        case 'done':
            currentCaptureState.status = 'complete';
            // Mark the current item as complete in the list state
            if (captureListState.items[currentCaptureState.index]) {
                captureListState.items[currentCaptureState.index].status = 'complete';
                updateCaptureListDisplay();
            }
            break;
        case 'idle':
            // If we were recording and now idle, it was aborted - reset to pending
            // (abortCapture function will also handle this, but this is a fallback)
            if (currentCaptureState.status === 'recording') {
                currentCaptureState.status = 'pending';
                if (captureListState.items[currentCaptureState.index]) {
                    captureListState.items[currentCaptureState.index].status = 'pending';
                    updateCaptureListDisplay();
                }
            }
            break;
    }
    updateCurrentCaptureDisplay();
}

/**
 * Handle capture complete - auto-advance to next pending capture
 * @param {object} data - Capture completion data
 */
async function onCaptureCompleteForCurrentCapture(data) {
    if (data.success) {
        // Update the capture list item status in the backend (persist it)
        const currentItem = captureListState.items[currentCaptureState.index];
        if (currentItem) {
            try {
                const result = await backend.call('setCaptureItemStatus', currentItem.id, 'complete');
                if (result.success) {
                    captureListState.items = result.captureList;
                }
            } catch (error) {
                console.error('Failed to persist capture complete status:', error);
                // Fall back to local update
                currentItem.status = 'complete';
            }
            updateCaptureListDisplay();
        }
        
        // Auto-advance to next pending capture after a brief delay
        setTimeout(() => {
            const advanced = advanceToNextCapture();
            
            if (!advanced) {
                // No more pending captures - stay on current (now complete) item
                currentCaptureState.status = 'complete';
                updateCurrentCaptureDisplay();
            }
            
            // Reset capture state to idle for next capture
            captureState.state = CaptureState.IDLE;
            updateCaptureUI();
        }, 500);
    }
}

/**
 * Initialize current capture display
 * Called after capture list is loaded/generated
 */
function initCurrentCaptureDisplay() {
    // Set to first pending capture if list exists
    if (captureListState.items.length > 0) {
        // Find first pending capture
        const firstPending = captureListState.items.findIndex(
            item => item.status === 'pending'
        );
        if (firstPending >= 0) {
            setCurrentCaptureIndex(firstPending);
        } else {
            // All complete, show last one
            setCurrentCaptureIndex(captureListState.items.length - 1);
        }
    }
    
    updateCurrentCaptureDisplay();
}

/**
 * Initialize capture workflow buttons (Skip, Re-capture)
 */
function initCaptureWorkflowButtons() {
    const skipBtn = document.getElementById('skip-capture-btn');
    const recaptureBtn = document.getElementById('recapture-btn');
    
    if (skipBtn) {
        skipBtn.addEventListener('click', skipCurrentCapture);
    }
    
    if (recaptureBtn) {
        recaptureBtn.addEventListener('click', recaptureCurrentCapture);
    }
}

//==============================================================================
// Capture List (Generated from Matrix)

/**
 * Capture list state
 */
const captureListState = {
    items: [],
    controlNames: [] // Column headers from controls
};

/**
 * Status display labels and classes
 */
const statusDisplay = {
    pending: { label: 'Pending', className: 'status-pending' },
    recording: { label: 'Recording', className: 'status-recording' },
    complete: { label: 'Complete', className: 'status-complete' },
    failed: { label: 'Failed', className: 'status-failed' }
};

/**
 * Update the generate/clear button states based on controls
 */
function updateCaptureListButtons() {
    const generateBtn = document.getElementById('generate-list-btn');
    const clearBtn = document.getElementById('clear-list-btn');
    
    const hasControls = captureControlsState.controls && captureControlsState.controls.length > 0;
    const hasValidCount = captureControlsState.totalCaptureCount > 0 && captureControlsState.totalCaptureCount <= 100000;
    const hasListItems = captureListState.items.length > 0;
    
    // Enable generate button only if we have controls with valid count
    generateBtn.disabled = !hasControls || !hasValidCount;
    
    // Show clear button only if we have list items
    if (hasListItems) {
        clearBtn.classList.remove('hidden');
    } else {
        clearBtn.classList.add('hidden');
    }
}

/**
 * Generate the capture list from the matrix
 */
async function generateCaptureList() {
    const generateBtn = document.getElementById('generate-list-btn');
    
    try {
        generateBtn.disabled = true;
        generateBtn.textContent = 'Generating...';
        
        const result = await backend.call('generateCaptureList');
        
        if (result.success) {
            captureListState.items = result.captureList || [];
            
            // Extract control names from controls state
            captureListState.controlNames = captureControlsState.controls.map(c => c.name);
            
            // Update the display
            updateCaptureListDisplay();
            updateCaptureListButtons();
            
            console.log(`Generated ${result.count} capture items`);
            
            // Initialize current capture display with the new list
            initCurrentCaptureDisplay();
        } else {
            console.error('Failed to generate capture list');
        }
    } catch (error) {
        console.error('Error generating capture list:', error);
    } finally {
        generateBtn.disabled = false;
        generateBtn.textContent = 'Generate List';
        updateCaptureListButtons();
    }
}

/**
 * Clear the capture list
 */
async function clearCaptureList() {
    const clearBtn = document.getElementById('clear-list-btn');
    
    try {
        clearBtn.disabled = true;
        
        await backend.call('clearCaptureList');
        
        captureListState.items = [];
        captureListState.controlNames = [];
        
        // Reset and hide current capture display
        currentCaptureState.index = 0;
        currentCaptureState.total = 0;
        currentCaptureState.controlValues = {};
        currentCaptureState.status = 'ready';
        
        updateCaptureListDisplay();
        updateCaptureListButtons();
        updateCurrentCaptureDisplay();
        
    } catch (error) {
        console.error('Error clearing capture list:', error);
    } finally {
        clearBtn.disabled = false;
    }
}

/**
 * Update the capture list table display
 */
function updateCaptureListDisplay() {
    const countEl = document.getElementById('capture-list-count');
    const progressTextEl = document.getElementById('capture-list-progress-text');
    const thead = document.querySelector('#capture-list-table thead tr');
    const tbody = document.getElementById('capture-list-tbody');
    const tableWrapper = document.querySelector('.capture-list-table-wrapper');
    const emptyMessage = document.getElementById('capture-list-empty');
    
    const count = captureListState.items.length;
    
    if (count === 0) {
        tableWrapper.classList.add('hidden');
        emptyMessage.classList.remove('hidden');
        countEl.textContent = '0 captures';
        progressTextEl.textContent = '0 / 0 complete';
        return;
    }
    
    tableWrapper.classList.remove('hidden');
    emptyMessage.classList.add('hidden');
    
    // Update count display
    countEl.textContent = `${count} capture${count !== 1 ? 's' : ''}`;
    
    // Calculate and update progress
    const completeCount = captureListState.items.filter(item => item.status === 'complete').length;
    progressTextEl.textContent = `${completeCount} / ${count} complete`;
    
    // Build table header with control columns
    let headerHtml = `
        <th class="col-index">#</th>
        <th class="col-status">Status</th>
    `;
    for (const name of captureListState.controlNames) {
        headerHtml += `<th class="col-control">${escapeHtml(name)}</th>`;
    }
    thead.innerHTML = headerHtml;
    
    // Build table body rows
    let bodyHtml = '';
    for (const item of captureListState.items) {
        const status = statusDisplay[item.status] || statusDisplay.pending;
        
        bodyHtml += `<tr data-capture-id="${item.id}">`;
        bodyHtml += `<td class="col-index">${item.index}</td>`;
        bodyHtml += `<td class="col-status"><span class="status-badge ${status.className}">${status.label}</span></td>`;
        
        // Add control value columns
        for (const name of captureListState.controlNames) {
            const value = item.controlValues ? (item.controlValues[name] || '-') : '-';
            bodyHtml += `<td class="col-control">${escapeHtml(value)}</td>`;
        }
        
        bodyHtml += '</tr>';
    }
    tbody.innerHTML = bodyHtml;
    
    // Attach click handlers to rows for selecting captures
    attachCaptureListRowHandlers();
    
    // Highlight current capture row
    highlightCurrentCaptureInList();
}

/**
 * Attach click handlers to capture list rows
 */
function attachCaptureListRowHandlers() {
    const tbody = document.getElementById('capture-list-tbody');
    if (!tbody) return;
    
    tbody.querySelectorAll('tr').forEach((row, index) => {
        row.style.cursor = 'pointer';
        row.addEventListener('click', () => {
            setCurrentCaptureIndex(index);
        });
    });
}

/**
 * Load capture list from backend (on startup)
 */
async function loadCaptureList() {
    try {
        const items = await backend.call('getCaptureList');
        captureListState.items = items || [];
        
        // Extract control names if we have items
        if (captureListState.items.length > 0 && captureControlsState.controls) {
            captureListState.controlNames = captureControlsState.controls.map(c => c.name);
        }
        
        updateCaptureListDisplay();
        updateCaptureListButtons();
        
        // Initialize current capture display if we have items
        if (captureListState.items.length > 0) {
            initCurrentCaptureDisplay();
        }
    } catch (error) {
        console.error('Failed to load capture list:', error);
    }
}

/**
 * Initialize capture list UI
 */
function initCaptureList() {
    const generateBtn = document.getElementById('generate-list-btn');
    const clearBtn = document.getElementById('clear-list-btn');
    
    if (generateBtn) {
        generateBtn.addEventListener('click', generateCaptureList);
    }
    
    if (clearBtn) {
        clearBtn.addEventListener('click', clearCaptureList);
    }
    
    // Load initial state
    loadCaptureList();
}

//==============================================================================
// Collapsible Panels

/**
 * Initialize collapse/expand functionality for a collapsible panel
 * @param {string} headerId - ID of the collapsible header element
 * @param {string} panelId - ID of the panel section element
 */
function initCollapsiblePanel(headerId, panelId) {
    const header = document.getElementById(headerId);
    const panel = document.getElementById(panelId);
    
    if (header && panel) {
        header.addEventListener('click', () => {
            panel.classList.toggle('collapsed');
        });
    }
}

/**
 * Initialize all collapsible sections
 */
function initAllCollapsiblePanels() {
    // Audio Setup section
    initCollapsiblePanel('audio-setup-header', 'audio-setup');
    
    // Session Settings section
    initCollapsiblePanel('session-settings-header', 'session-settings');
    
    // Capture List section
    initCollapsiblePanel('capture-list-header', 'capture-list-section');
    
    // Capture View section
    initCollapsiblePanel('capture-view-header', 'capture-view-section');
}

/**
 * Expand the capture list panel (e.g., when a capture is selected)
 */
function expandCaptureList() {
    const panel = document.getElementById('capture-list-section');
    if (panel) {
        panel.classList.remove('collapsed');
    }
}

/**
 * Collapse the capture list panel
 */
function collapseCaptureList() {
    const panel = document.getElementById('capture-list-section');
    if (panel) {
        panel.classList.add('collapsed');
    }
}
