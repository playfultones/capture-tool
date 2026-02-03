/**
 * guide-init.js - Visual Guide Module Initialization
 * 
 * Wires up the guide components and integrates with the capture matrix.
 * This is the main entry point for the visual guide feature.
 */

// Global guide state (accessible from capture.js)
window.guideState = null;

let cameraManager = null;
let overlayManager = null;
let propertiesPanel = null;
let guideSyncTimeout = null;

/**
 * Initialize the visual guide module
 */
async function initGuideModule() {
    // Create state
    window.guideState = new window.GuideState();
    
    // Link to capture controls state
    window.guideState.captureControlsState = window.captureControlsState;
    
    // Create managers
    cameraManager = new window.GuideCameraManager(window.guideState);
    overlayManager = new window.GuideOverlayManager(window.guideState, cameraManager);
    propertiesPanel = new window.GuidePropertiesPanel(window.guideState, overlayManager);
    
    // Initialize camera enumeration
    await cameraManager.init();
    
    // Initialize overlay canvas
    overlayManager.init();
    
    // Initialize properties panel
    propertiesPanel.init();
    
    // Initialize collapsible panels
    initGuideCollapsiblePanels();
    
    // Wire up integration with capture matrix
    initGuideMatrixIntegration();
    
    console.log('Visual guide module initialized');
}

/**
 * Initialize collapsible panel behavior for guide sections
 */
function initGuideCollapsiblePanels() {
    // Visual Guide section
    const visualGuideHeader = document.getElementById('visual-guide-header');
    const visualGuidePanel = document.getElementById('visual-guide-section');
    
    if (visualGuideHeader && visualGuidePanel) {
        visualGuideHeader.addEventListener('click', () => {
            visualGuidePanel.classList.toggle('collapsed');
        });
    }
    
    // Guide Properties section
    const propsHeader = document.getElementById('guide-properties-header');
    const propsPanel = document.getElementById('guide-properties-section');
    
    if (propsHeader && propsPanel) {
        propsHeader.addEventListener('click', () => {
            propsPanel.classList.toggle('collapsed');
        });
    }
}

/**
 * Wire up integration between guide module and capture matrix
 */
function initGuideMatrixIntegration() {
    const controlsList = document.getElementById('capture-controls-list');
    if (!controlsList) return;
    
    // Delegate click handling for guide buttons
    controlsList.addEventListener('click', (e) => {
        // Create Guide button
        const createBtn = e.target.closest('.btn-create-guide');
        if (createBtn) {
            e.stopPropagation();
            const controlId = createBtn.dataset.controlId;
            if (controlId && window.guideState) {
                // Expand the visual guide panel
                const visualGuidePanel = document.getElementById('visual-guide-section');
                visualGuidePanel?.classList.remove('collapsed');
                
                // Request guide creation
                window.guideState.emit('guide:create-requested', { controlId });
            }
            return;
        }
        
        // View Guide button  
        const viewBtn = e.target.closest('.btn-view-guide');
        if (viewBtn) {
            e.stopPropagation();
            const controlId = viewBtn.dataset.controlId;
            if (controlId && window.guideState) {
                const guide = window.guideState.getGuide(controlId);
                if (guide) {
                    // Select the guide
                    window.guideState.selectGuide(controlId);
                    
                    // Expand both panels
                    document.getElementById('visual-guide-section')?.classList.remove('collapsed');
                    document.getElementById('guide-properties-section')?.classList.remove('collapsed');
                }
            }
            return;
        }
        
        // Delete Guide button (small X next to view button)
        const deleteBtn = e.target.closest('.btn-delete-guide');
        if (deleteBtn) {
            e.stopPropagation();
            const controlId = deleteBtn.dataset.controlId;
            if (controlId && window.guideState) {
                window.guideState.removeGuide(controlId);
                // Trigger re-render of capture controls
                if (typeof updateCaptureControlsDisplay === 'function') {
                    updateCaptureControlsDisplay();
                }
            }
            return;
        }
    });
    
    // Listen for guide changes to update matrix display and sync to backend
    if (window.guideState) {
        window.guideState.on('guide:added', () => {
            if (typeof updateCaptureControlsDisplay === 'function') {
                updateCaptureControlsDisplay();
            }
            syncGuideStateToBackend();
        });
        
        window.guideState.on('guide:removed', () => {
            if (typeof updateCaptureControlsDisplay === 'function') {
                updateCaptureControlsDisplay();
            }
            syncGuideStateToBackend();
        });
        
        window.guideState.on('guide:updated', () => {
            syncGuideStateToBackend();
        });
    }
}

/**
 * Debounced sync of guide state to backend for project persistence
 */
function syncGuideStateToBackend() {
    // Debounce to avoid excessive backend calls during drag operations
    if (guideSyncTimeout) {
        clearTimeout(guideSyncTimeout);
    }
    
    guideSyncTimeout = setTimeout(async () => {
        if (typeof backend !== 'undefined' && backend.call) {
            try {
                const guideData = serializeGuideState();
                await backend.call('setGuideState', guideData);
            } catch (error) {
                // Silently fail - backend may not have this method yet
                console.debug('Could not sync guide state to backend:', error);
            }
        }
    }, 500);
}

/**
 * Serialize guide state for project save
 * @returns {object}
 */
function serializeGuideState() {
    if (!window.guideState) {
        return { guides: [], cameraDeviceId: null };
    }
    
    return {
        guides: window.guideState.serializeGuides(),
        cameraDeviceId: window.guideState.serializeCamera()
    };
}

/**
 * Restore guide state from project load
 * @param {object} data - { guides: Array, cameraDeviceId: string|null }
 */
async function restoreGuideState(data) {
    if (!window.guideState || !data) return;
    
    // Clear existing guides
    window.guideState.clearGuides();
    
    // Restore camera selection
    if (data.cameraDeviceId && cameraManager) {
        const selectEl = document.getElementById('guide-camera-select');
        if (selectEl) {
            selectEl.value = data.cameraDeviceId;
            // Optionally auto-start camera
            // await cameraManager.startCamera(data.cameraDeviceId);
        }
    }
    
    // Restore guides
    if (data.guides && Array.isArray(data.guides) && overlayManager) {
        const { width, height } = window.guideState.getCanvasDimensions();
        const minDim = Math.min(width, height);
        
        for (const guideData of data.guides) {
            // Check if control still exists
            const control = window.guideState.getControl(guideData.controlId);
            if (!control) continue;
            
            // Create guide with denormalized coordinates
            const x = guideData.x * width;
            const y = guideData.y * height;
            const radius = guideData.radius * minDim;
            
            // Directly create the guide via overlay manager's internal method
            // We need to expose a way to create guides with specific arc angles
            const guide = new window.KnobGuide({
                x,
                y,
                radius,
                control,
                canvas: overlayManager.canvas,
                onArcChange: (arcStart, arcEnd) => {
                    window.guideState.updateGuide(guideData.controlId, { arcStart, arcEnd });
                }
            });
            
            // Set arc angles
            guide.arcStart = guideData.arcStart;
            guide.arcEnd = guideData.arcEnd;
            guide._rebuildGroup();
            
            // Add to canvas
            const fabricObject = guide.getFabricObject();
            if (fabricObject && overlayManager.canvas) {
                overlayManager.canvas.add(fabricObject);
            }
            
            const startHandle = guide.getStartHandle();
            const endHandle = guide.getEndHandle();
            if (startHandle) overlayManager.canvas.add(startHandle);
            if (endHandle) overlayManager.canvas.add(endHandle);
            
            // Add to state
            window.guideState.addGuide({
                controlId: guideData.controlId,
                fabricObject,
                knobGuide: guide,
                arcStart: guideData.arcStart,
                arcEnd: guideData.arcEnd
            });
        }
        
        overlayManager.canvas?.renderAll();
    }
    
    // Update UI
    if (typeof updateCaptureControlsDisplay === 'function') {
        updateCaptureControlsDisplay();
    }
}

/**
 * Clear all guide state (for new project)
 */
function clearGuideState() {
    if (window.guideState) {
        window.guideState.clearGuides();
    }
    if (cameraManager) {
        cameraManager.stopCamera();
    }
}

// Export functions for use by project.js
window.initGuideModule = initGuideModule;
window.serializeGuideState = serializeGuideState;
window.restoreGuideState = restoreGuideState;
window.clearGuideState = clearGuideState;
