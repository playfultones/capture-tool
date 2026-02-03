/**
 * GuideOverlayManager - Fabric.js Canvas Management
 * 
 * Coordinates between camera feed and canvas overlays. Handles:
 * - Fabric.js canvas initialization and resizing
 * - Guide creation workflow (click-drag to draw circle)
 * - Guide selection and deletion
 * - Synchronizing guide state with GuideState
 */

// Uses window.KnobGuide from KnobGuide.js

window.GuideOverlayManager = class GuideOverlayManager {
  /**
   * @param {GuideState} state
   * @param {GuideCameraManager} cameraManager
   */
  constructor(state, cameraManager) {
    this.state = state;
    this.cameraManager = cameraManager;
    
    /** @type {fabric.Canvas|null} */
    this.canvas = null;
    
    /** @type {HTMLElement|null} */
    this.containerElement = null;
    
    /** @type {number|null} */
    this.renderLoopId = null;
    
    /** @type {boolean} */
    this.isCreatingGuide = false;
    
    /** @type {{x: number, y: number}|null} */
    this.creationStart = null;
    
    /** @type {fabric.Circle|null} Preview circle shown during drag */
    this._previewCircle = null;
    
    /** @type {string|null} Control ID for pending guide creation */
    this._pendingControlId = null;
  }
  
  /**
   * Initialize the overlay manager
   */
  init() {
    this.containerElement = document.getElementById('guide-canvas-container');
    
    // Initialize Fabric.js canvas
    this._initCanvas();
    
    // Set up resize handler
    window.addEventListener('resize', () => this._onResize());
    
    // Set up keyboard handler for Delete key
    window.addEventListener('keydown', (e) => this._onKeyDown(e));
    
    // Subscribe to state changes
    this.state.on('camera:changed', () => this._onCameraChanged());
    this.state.on('camera:cleared', () => this._onCameraCleared());
    this.state.on('guide:create-requested', (data) => this._startGuideCreation(data.controlId));
    this.state.on('guide:removed', (guide) => this._onGuideRemoved(guide));
    this.state.on('guide:selected', (controlId) => this._onGuideSelectionChanged(controlId));
    
    // Initial size
    this._onResize();
  }
  
  // Private methods
  
  /**
   * Initialize Fabric.js canvas
   * @private
   */
  _initCanvas() {
    // Check if Fabric.js is loaded
    if (typeof fabric === 'undefined') {
      console.error('Fabric.js not loaded');
      return;
    }
    
    this.canvas = new fabric.Canvas('guide-canvas', {
      selection: true,
      preserveObjectStacking: true,
      backgroundColor: 'transparent'
    });
    
    // Set up canvas events for guide creation
    this.canvas.on('mouse:down', (e) => this._onMouseDown(e));
    this.canvas.on('mouse:move', (e) => this._onMouseMove(e));
    this.canvas.on('mouse:up', (e) => this._onMouseUp(e));
    this.canvas.on('selection:created', (e) => this._onSelectionChanged(e));
    this.canvas.on('selection:updated', (e) => this._onSelectionChanged(e));
    this.canvas.on('selection:cleared', () => this._onSelectionCleared());
  }
  
  /**
   * Handle window resize
   * @private
   */
  _onResize() {
    if (!this.canvas || !this.containerElement) return;
    
    const rect = this.containerElement.getBoundingClientRect();
    this.canvas.setDimensions({
      width: rect.width,
      height: rect.height
    });
    this.canvas.renderAll();
  }
  
  /**
   * Handle camera stream changed
   * @private
   */
  _onCameraChanged() {
    // Camera is now active - add class to hide placeholder
    this.containerElement?.classList.add('has-camera');
  }
  
  /**
   * Handle camera cleared
   * @private
   */
  _onCameraCleared() {
    this.containerElement?.classList.remove('has-camera');
  }
  
  /**
   * Start guide creation mode
   * @param {string} controlId
   * @private
   */
  _startGuideCreation(controlId) {
    // Prevent duplicate guides for the same control
    if (this.state.getGuide(controlId)) {
      console.warn(`Guide already exists for control ${controlId}`);
      return;
    }
    
    this.isCreatingGuide = true;
    this._pendingControlId = controlId;
    this.containerElement?.classList.add('creating');
  }
  
  /**
   * Handle mouse down on canvas
   * @param {fabric.IEvent} e
   * @private
   */
  _onMouseDown(e) {
    if (!this.isCreatingGuide) return;
    if (!e.pointer) return;
    
    // Disable selection during guide creation
    this.canvas.selection = false;
    
    // Start drawing a guide
    this.creationStart = { x: e.pointer.x, y: e.pointer.y };
    
    // Create preview circle
    this._previewCircle = new fabric.Circle({
      left: e.pointer.x,
      top: e.pointer.y,
      radius: 0,
      fill: 'transparent',
      stroke: '#ff6600',
      strokeWidth: 2,
      strokeDashArray: [5, 5],
      originX: 'center',
      originY: 'center',
      selectable: false,
      evented: false
    });
    this.canvas.add(this._previewCircle);
  }
  
  /**
   * Handle mouse move on canvas
   * @param {fabric.IEvent} e
   * @private
   */
  _onMouseMove(e) {
    if (!this.isCreatingGuide || !this.creationStart || !this._previewCircle || !e.pointer) {
      return;
    }
    
    // Calculate radius from drag distance
    const dx = e.pointer.x - this.creationStart.x;
    const dy = e.pointer.y - this.creationStart.y;
    const radius = Math.sqrt(dx * dx + dy * dy);
    
    // Update preview circle
    this._previewCircle.set({ radius });
    this.canvas.renderAll();
  }
  
  /**
   * Handle mouse up on canvas
   * @param {fabric.IEvent} e
   * @private
   */
  _onMouseUp(e) {
    if (!this.isCreatingGuide || !this.creationStart || !e.pointer) {
      return;
    }
    
    // Remove preview circle
    this._removePreviewCircle();
    
    // Calculate circle from drag
    const dx = e.pointer.x - this.creationStart.x;
    const dy = e.pointer.y - this.creationStart.y;
    const radius = Math.sqrt(dx * dx + dy * dy);
    
    // Minimum radius check
    if (radius < 20) {
      this._cancelGuideCreation();
      return;
    }
    
    // Create the guide
    this._createGuide(
      this.creationStart.x,
      this.creationStart.y,
      radius,
      this._pendingControlId
    );
    
    this._cancelGuideCreation();
  }
  
  /**
   * Remove the preview circle from canvas
   * @private
   */
  _removePreviewCircle() {
    if (this._previewCircle && this.canvas) {
      this.canvas.remove(this._previewCircle);
      this._previewCircle = null;
    }
  }
  
  /**
   * Cancel guide creation mode
   * @private
   */
  _cancelGuideCreation() {
    this._removePreviewCircle();
    this.isCreatingGuide = false;
    this.creationStart = null;
    this._pendingControlId = null;
    this.containerElement?.classList.remove('creating');
    
    // Re-enable selection
    if (this.canvas) {
      this.canvas.selection = true;
    }
  }
  
  /**
   * Create a knob guide
   * @param {number} x - Center X
   * @param {number} y - Center Y
   * @param {number} radius
   * @param {string} controlId
   * @private
   */
  _createGuide(x, y, radius, controlId) {
    const control = this.state.getControl(controlId);
    if (!control) return;
    
    const guide = new window.KnobGuide({
      x,
      y,
      radius,
      control,
      canvas: this.canvas,
      onArcChange: (arcStart, arcEnd) => {
        // Update state and emit guide:updated for real-time panel updates
        this.state.updateGuide(controlId, { arcStart, arcEnd });
      }
    });
    
    // Add group to canvas
    const fabricObject = guide.getFabricObject();
    if (fabricObject && this.canvas) {
      this.canvas.add(fabricObject);
    }
    
    // Add handles to canvas (separate from group so they're independently draggable)
    const startHandle = guide.getStartHandle();
    const endHandle = guide.getEndHandle();
    if (startHandle && this.canvas) {
      this.canvas.add(startHandle);
    }
    if (endHandle && this.canvas) {
      this.canvas.add(endHandle);
    }
    
    this.canvas.renderAll();
    
    // Add to state (store the KnobGuide instance for handle access)
    this.state.addGuide({
      controlId,
      fabricObject,
      knobGuide: guide,
      arcStart: guide.arcStart,
      arcEnd: guide.arcEnd
    });
  }
  
  /**
   * Handle guide removed from state
   * @param {object} guide
   * @private
   */
  _onGuideRemoved(guide) {
    if (!this.canvas || !guide) return;
    
    const canvasObjects = this.canvas.getObjects();
    
    // Find all objects that belong to this guide by controlId
    const objectsToRemove = canvasObjects.filter(obj => 
      obj.controlId === guide.controlId
    );
    
    // Discard active object first
    this.canvas.discardActiveObject();
    
    // Remove all matching objects
    for (const obj of objectsToRemove) {
      this.canvas.remove(obj);
    }
    
    this.canvas.requestRenderAll();
  }
  
  /**
   * Handle selection changed
   * @param {fabric.IEvent} e
   * @private
   */
  _onSelectionChanged(e) {
    const selected = e.selected?.[0];
    if (selected?.controlId) {
      this.state.selectGuide(selected.controlId);
    }
  }
  
  /**
   * Handle selection cleared
   * @private
   */
  _onSelectionCleared() {
    this.state.selectGuide(null);
  }
  
  /**
   * Handle guide selection changed from state - update visual feedback
   * @param {string|null} selectedControlId
   * @private
   */
  _onGuideSelectionChanged(selectedControlId) {
    // Update visual styling for all guides (selected vs deselected)
    for (const guide of this.state.guides) {
      if (guide.knobGuide) {
        const isSelected = guide.controlId === selectedControlId;
        guide.knobGuide.setSelected(isSelected);
      }
    }
    
    if (!this.canvas) return;
    
    // Update Fabric.js selection to match (only if not already matching)
    const activeObject = this.canvas.getActiveObject();
    const activeControlId = activeObject?.controlId;
    
    if (selectedControlId && activeControlId !== selectedControlId) {
      const guide = this.state.getGuide(selectedControlId);
      if (guide?.fabricObject) {
        this.canvas.setActiveObject(guide.fabricObject);
      }
    } else if (!selectedControlId && activeObject) {
      this.canvas.discardActiveObject();
    }
    
    this.canvas.requestRenderAll();
  }
  
  /**
   * Handle keyboard events
   * @param {KeyboardEvent} e
   * @private
   */
  _onKeyDown(e) {
    // Delete key removes selected guide
    if (e.key === 'Delete' || e.key === 'Backspace') {
      // Don't interfere with form inputs
      const target = e.target;
      if (target instanceof HTMLInputElement || 
          target instanceof HTMLTextAreaElement ||
          target instanceof HTMLSelectElement) {
        return;
      }
      
      this._deleteSelectedGuide();
    }
  }
  
  /**
   * Delete the currently selected guide
   */
  deleteSelectedGuide() {
    this._deleteSelectedGuide();
  }
  
  /**
   * Delete the currently selected guide (internal)
   * @private
   */
  _deleteSelectedGuide() {
    const selectedGuideId = this.state.selectedGuideId;
    if (!selectedGuideId) return;
    
    // Remove guide from state (this will trigger guide:removed event)
    // Note: This only removes the guide, not the associated control
    this.state.removeGuide(selectedGuideId);
  }
}
