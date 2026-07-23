// Project save/load module

/**
 * Project state tracking
 */
const projectState = {
    filePath: '',
    fileName: '',
    hasSavedProject: false
};

/**
 * Update project info display in session header
 */
function updateProjectDisplay() {
    const projectNameEl = document.getElementById('project-name');
    
    if (projectState.hasSavedProject && projectState.fileName) {
        projectNameEl.textContent = projectState.fileName;
        projectNameEl.classList.add('has-file');
        projectNameEl.title = projectState.filePath;
    } else {
        projectNameEl.textContent = 'Unsaved Project';
        projectNameEl.classList.remove('has-file');
        projectNameEl.title = '';
    }
}

/**
 * Show the startup modal
 */
function showStartupModal() {
    const modal = document.getElementById('startup-modal');
    modal.classList.remove('hidden');
    document.body.classList.add('modal-open');
}

/**
 * Hide the startup modal
 */
function hideStartupModal() {
    const modal = document.getElementById('startup-modal');
    modal.classList.add('hidden');
    document.body.classList.remove('modal-open');
}

/**
 * Handle "New Project" from startup modal
 * Opens save dialog to set project location, then starts fresh
 */
async function handleStartupNewProject() {
    const result = await backend.call('saveProjectAs');
    
    if (result.cancelled) {
        // User cancelled, stay on startup modal
        return;
    }
    
    if (result.success) {
        // Update project state
        projectState.filePath = result.filePath;
        projectState.fileName = result.fileName;
        projectState.hasSavedProject = true;

        updateProjectDisplay();
        markSavedNow();
        hideStartupModal();

        // Reload output folder state (backend sets it to project folder by default)
        await loadOutputFolderState();
        
        console.log(`New project created: ${result.filePath}`);
    } else {
        alert('Failed to create project: ' + (result.errorMessage || 'Unknown error'));
    }
}

/**
 * Handle "Open Project" from startup modal
 */
async function handleStartupOpenProject() {
    const result = await backend.call('loadProject');
    
    if (result.cancelled) {
        // User cancelled, stay on startup modal
        return;
    }
    
    if (result.success) {
        // Update project state
        projectState.filePath = result.filePath;
        projectState.fileName = result.fileName;
        projectState.hasSavedProject = true;
        
        updateProjectDisplay();
        hideStartupModal();
        
        console.log(`Project loaded from: ${result.filePath}`);
        
        // Refresh all UI state from backend
        await refreshAllUIState();
        
        // Show warning if reference signal was missing
        if (result.referenceSignalMissing) {
            alert(`Warning: Reference signal file not found:\n${result.missingReferenceSignalPath}\n\nPlease select a new reference signal.`);
        }
    } else {
        alert('Failed to load project: ' + (result.errorMessage || 'Unknown error'));
    }
}

/**
 * Initialize startup modal
 */
function initStartupModal() {
    const newBtn = document.getElementById('startup-new-btn');
    const openBtn = document.getElementById('startup-open-btn');
    
    if (newBtn) {
        newBtn.addEventListener('click', handleStartupNewProject);
    }
    
    if (openBtn) {
        openBtn.addEventListener('click', handleStartupOpenProject);
    }
}

/**
 * Refresh all UI state after loading a project
 * Reloads all state from the backend to update the UI
 */
async function refreshAllUIState() {
    try {
        // Reload audio devices and state
        await loadAudioDevices();
        await loadSampleRates();
        
        // Reload output gain trim
        outputGainTrimDb = await backend.call('getOutputGainTrim');
        document.getElementById('output-gain-trim').value = outputGainTrimDb;
        updateOutputTrimDisplay();
        
        // Reload reference signal state
        await loadSignalsFromBackend();
        
        // Reload calibration state
        const calState = await backend.call('getCalibrationState');
        if (calState && calState.completed) {
            calibrationState.completed = calState.completed;
            calibrationState.completedAt = calState.completedAt;
            calibrationState.testToneLevelDbfs = calState.testToneLevelDbfs ?? -18.0;
            calibrationState.unityLevelDbfs = calState.unityLevelDbfs;
            calibrationState.maxLevelDbfs = calState.maxLevelDbfs;
            calibrationState.outputTrimDb = calState.outputTrimDb ?? 0.0;
        } else {
            calibrationState.completed = false;
        }
        updateCalibrationStatus();
        
        // Reload recording tail
        recordingTailMs = await backend.call('getRecordingTailMs');
        document.getElementById('recording-tail').value = String(recordingTailMs);
        
        // Reload output folder state
        await loadOutputFolderState();
        
        // Reload capture controls
        await loadCaptureControls();
        
        // Reload capture list
        await loadCaptureList();
        
        // Initialize current capture display if we have captures
        if (captureListState.items.length > 0) {
            initCurrentCaptureDisplay();
        }
        
        // Update capture UI for reference signal
        updateCaptureForReferenceSignal();
        
        // Restore visual guide state if module is loaded
        if (typeof restoreGuideState === 'function') {
            try {
                const guideData = await backend.call('getGuideState');
                if (guideData) {
                    await restoreGuideState(guideData);
                }
            } catch (guideError) {
                // Guide state loading is optional - don't fail project load
                console.warn('Could not restore guide state:', guideError);
            }
        }
        
    } catch (error) {
        console.error('Error refreshing UI state:', error);
    }
}

/**
 * Reset all UI state to defaults (for new/close project)
 */
async function resetUIToDefaults() {
    // Reset project state
    projectState.filePath = '';
    projectState.fileName = '';
    projectState.hasSavedProject = false;
    updateProjectDisplay();
    
    // Reset reference signal state
    referenceSignalState.loaded = false;
    referenceSignalState.filePath = '';
    referenceSignalState.fileName = '';
    referenceSignalState.sampleRate = 0;
    referenceSignalState.numSamples = 0;
    referenceSignalState.durationSeconds = 0;
    referenceSignalState.isPlaying = false;
    referenceSignalState.isLooping = false;
    updateReferenceSignalDisplay();
    
    // Reset calibration state
    calibrationState.completed = false;
    calibrationState.completedAt = null;
    calibrationState.unityLevelDbfs = null;
    calibrationState.maxLevelDbfs = null;
    calibrationState.outputTrimDb = 0.0;
    updateCalibrationStatus();
    
    // Reset output gain trim
    outputGainTrimDb = 0.0;
    const trimSlider = document.getElementById('output-gain-trim');
    if (trimSlider) {
        trimSlider.value = 0;
    }
    updateOutputTrimDisplay();
    
    // Reset capture controls
    captureControlsState.controls = [];
    captureControlsState.totalCaptureCount = 0;
    updateCaptureControlsDisplay();
    
    // Reset capture list
    captureListState.items = [];
    captureListState.controlNames = [];
    updateCaptureListDisplay();
    updateCaptureListButtons();
    
    // Reset current capture display
    currentCaptureState.index = 0;
    currentCaptureState.total = 0;
    currentCaptureState.controlValues = {};
    currentCaptureState.status = 'ready';
    updateCurrentCaptureDisplay();
    
    // Reset output folder display
    const folderPathEl = document.getElementById('output-folder-path');
    const folderStatusEl = document.getElementById('output-folder-status');
    if (folderPathEl) folderPathEl.textContent = 'No folder selected';
    if (folderStatusEl) folderStatusEl.textContent = '';
    
    // Reset recording tail to default
    recordingTailMs = 500;
    const tailSelect = document.getElementById('recording-tail');
    if (tailSelect) tailSelect.value = '500';
    
    // Clear visual guide state if module is loaded
    if (typeof clearGuideState === 'function') {
        clearGuideState();
    }
}

/**
 * Handle project new requested event from native menu (New Project)
 * This resets the UI and immediately opens the save dialog
 */
async function onProjectNewRequested() {
    console.log('New project requested from menu');
    
    // Reset UI to defaults first
    await resetUIToDefaults();
    
    // Open save dialog (same as startup modal "New Project" button)
    const result = await backend.call('saveProjectAs');
    
    if (result.cancelled) {
        // User cancelled - show startup modal so they can choose what to do
        showStartupModal();
        return;
    }
    
    if (result.success) {
        // Update project state
        projectState.filePath = result.filePath;
        projectState.fileName = result.fileName;
        projectState.hasSavedProject = true;

        updateProjectDisplay();
        markSavedNow();
        hideStartupModal();

        // Reload output folder state (backend sets it to project folder by default)
        await loadOutputFolderState();
        
        console.log(`New project created: ${result.filePath}`);
    } else {
        alert('Failed to create project: ' + (result.errorMessage || 'Unknown error'));
        showStartupModal();
    }
}

/**
 * Handle project loaded event from native menu (Open Project)
 */
async function onProjectLoaded(data) {
    console.log('Project loaded from menu:', data);
    
    if (data && data.success) {
        // Update project state
        projectState.filePath = data.filePath || '';
        projectState.fileName = data.fileName || '';
        projectState.hasSavedProject = true;
        
        updateProjectDisplay();
        hideStartupModal();
        
        // Refresh all UI state from backend
        await refreshAllUIState();
        
        // Show warning if reference signal was missing
        if (data.referenceSignalMissing) {
            alert(`Warning: Reference signal file not found:\n${data.missingReferenceSignalPath}\n\nPlease select a new reference signal.`);
        }
    } else if (data && data.errorMessage) {
        alert('Failed to load project: ' + data.errorMessage);
    }
}

/**
 * Initialize project actions (startup modal)
 */
function initProjectActions() {
    // Initialize startup modal buttons
    initStartupModal();

    // Show startup modal on launch
    showStartupModal();

    // Keep the "saved N ago" label fresh even when nothing changes
    setInterval(updateSaveStatusDisplay, 15000);
}

//==============================================================================
// Auto-save status indicator

/** Epoch millis of the last successful auto-save (null = never) */
let lastSavedMs = null;

/**
 * Render the save-status label in the header.
 */
function updateSaveStatusDisplay() {
    const el = document.getElementById('save-status');
    if (!el) return;

    if (lastSavedMs == null) {
        el.textContent = 'Not saved yet';
        el.classList.remove('saved');
        el.title = 'No changes have been auto-saved yet';
        return;
    }

    const secs = Math.max(0, Math.floor((Date.now() - lastSavedMs) / 1000));
    let ago;
    if (secs < 5) ago = 'just now';
    else if (secs < 60) ago = `${secs}s ago`;
    else if (secs < 3600) ago = `${Math.floor(secs / 60)}m ago`;
    else ago = `${Math.floor(secs / 3600)}h ago`;

    const t = new Date(lastSavedMs);
    const hh = String(t.getHours()).padStart(2, '0');
    const mm = String(t.getMinutes()).padStart(2, '0');

    el.textContent = `Saved ${ago}`;
    el.classList.add('saved');
    el.title = `Last auto-saved at ${hh}:${mm}`;
}

/**
 * Handle the backend "projectSaved" event (emitted after each auto-save).
 */
function onProjectSaved(data) {
    lastSavedMs = (data && data.timeMs) ? data.timeMs : Date.now();
    updateSaveStatusDisplay();
}

/**
 * Mark the project as saved "now" (used e.g. right after loading a project,
 * which reflects the on-disk state).
 */
function markSavedNow() {
    lastSavedMs = Date.now();
    updateSaveStatusDisplay();
}
