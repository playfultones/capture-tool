/**
 * KnobGuide - Custom Fabric.js Object for Knob Overlay
 * 
 * Renders a circular guide overlay for a hardware knob with:
 * - Base circle outline
 * - Adjustable arc range (drag handles to set min/max positions)
 * - Value tick marks and labels distributed along the arc
 * 
 * Supports move, resize (uniform scale), and arc handle dragging.
 */

// Uses window.ArcMarkers from ArcMarkers.js

/**
 * Default arc angles (knob convention, in canvas coordinates)
 * 
 * Canvas coordinate system (Y increases downward):
 * - 0° = 3 o'clock (right)
 * - 90° = 6 o'clock (down)
 * - 180° = 9 o'clock (left)
 * - 270° = 12 o'clock (up)
 * 
 * Typical potentiometer range: 7 o'clock to 5 o'clock (clockwise, ~300° sweep)
 * - Start: 7 o'clock = 120° (30° past 6 o'clock toward 9 o'clock)
 * - End: 5 o'clock = 60° (30° before 6 o'clock toward 3 o'clock)
 */
const DEFAULT_ARC_START = 120;
const DEFAULT_ARC_END = 60;

/** Visual styling constants */
const STYLE = {
  SELECTED: {
    strokeColor: '#ff6600',
    strokeWidth: 3,
    handleStroke: '#00aaff',
    opacity: 1,
    showHandles: true
  },
  DESELECTED: {
    strokeColor: '#ff6600',
    strokeWidth: 1.5,
    handleStroke: '#888888',
    opacity: 1,  // Keep full opacity for readability
    showHandles: false  // Hide arc handle indicators when not selected
  }
};

/**
 * Constrain a point to lie on a circle's perimeter
 * @param {number} pointX - Point X coordinate
 * @param {number} pointY - Point Y coordinate
 * @param {number} centerX - Circle center X
 * @param {number} centerY - Circle center Y
 * @param {number} radius - Circle radius
 * @returns {{x: number, y: number, angle: number}} Constrained position and angle in degrees
 */
function constrainToCircle(pointX, pointY, centerX, centerY, radius) {
  const dx = pointX - centerX;
  const dy = pointY - centerY;
  const angle = Math.atan2(dy, dx);
  
  return {
    x: centerX + radius * Math.cos(angle),
    y: centerY + radius * Math.sin(angle),
    angle: ((angle * 180) / Math.PI + 360) % 360 // Convert to degrees, normalize to 0-360
  };
}

window.KnobGuide = class KnobGuide {
  /**
   * @param {object} options
   * @param {number} options.x - Center X
   * @param {number} options.y - Center Y
   * @param {number} options.radius - Circle radius
   * @param {import('../controls/CaptureControl.js').CaptureControl} options.control
   * @param {fabric.Canvas} options.canvas
   * @param {Function} [options.onArcChange] - Callback when arc angles change
   */
  constructor(options) {
    this.x = options.x;
    this.y = options.y;
    this.radius = options.radius;
    this.control = options.control;
    this.canvas = options.canvas;
    
    /** @type {Function|null} Callback for arc angle changes */
    this._onArcChange = options.onArcChange || null;
    
    /** Original radius before any scaling (used to calculate effective radius) */
    this._baseRadius = options.radius;
    
    /** Arc start angle in degrees (0° = 3 o'clock) */
    this.arcStart = DEFAULT_ARC_START;
    
    /** Arc end angle in degrees */
    this.arcEnd = DEFAULT_ARC_END;
    
    /** @type {fabric.Group|null} Main visual group */
    this._fabricObject = null;
    
    /** @type {fabric.Circle|null} Invisible drag handle for arc start */
    this._startDragHandle = null;
    
    /** @type {fabric.Circle|null} Invisible drag handle for arc end */
    this._endDragHandle = null;
    
    /** @type {window.ArcMarkers} */
    this.arcMarkers = new window.ArcMarkers({
      values: this.control.values,
      arcStart: this.arcStart,
      arcEnd: this.arcEnd,
      radius: this.radius
    });
    
    /** @type {boolean} Whether this guide is currently selected */
    this._isSelected = true;
    
    this._build();
  }
  
  /**
   * Build the Fabric.js group and drag handles
   * @private
   */
  _build() {
    if (typeof fabric === 'undefined') {
      console.error('Fabric.js not loaded');
      return;
    }
    
    // Build the main visual group
    this._buildGroup();
    
    // Create invisible drag handles (separate from group)
    this._startDragHandle = this._createDragHandle('start');
    this._endDragHandle = this._createDragHandle('end');
    
    // Set up transform event handlers for the group
    this._setupTransformEvents();
  }
  
  /**
   * Build or rebuild the main visual group
   * @private
   */
  _buildGroup() {
    const objects = [];
    
    // Base circle
    const circle = new fabric.Circle({
      radius: this.radius,
      fill: 'transparent',
      stroke: '#ff6600',
      strokeWidth: 2,
      originX: 'center',
      originY: 'center'
    });
    objects.push(circle);
    
    // Visual handle indicators (part of group, not draggable themselves)
    const startHandle = this._createVisualHandle(this.arcStart);
    const endHandle = this._createVisualHandle(this.arcEnd);
    objects.push(startHandle, endHandle);
    
    // Value markers
    const markers = this.arcMarkers.createMarkers(this.x, this.y);
    objects.push(...markers);
    
    // Create the group
    this._fabricObject = new fabric.Group(objects, {
      left: this.x,
      top: this.y,
      originX: 'center',
      originY: 'center',
      lockUniScaling: true,
      selectable: true,
      evented: true,
      hasControls: true,
      hasBorders: true
    });
    
    this._fabricObject.controlId = this.control.id;
    this._fabricObject.knobGuide = this;
  }
  
  /**
   * Create a visual handle circle (rendered in group, not interactive)
   * @param {number} angleDeg
   * @returns {fabric.Circle}
   * @private
   */
  _createVisualHandle(angleDeg) {
    const rad = (angleDeg * Math.PI) / 180;
    const x = this.radius * Math.cos(rad);
    const y = this.radius * Math.sin(rad);
    
    return new fabric.Circle({
      left: x,
      top: y,
      radius: 10,
      fill: '#ffffff',
      stroke: '#00aaff',
      strokeWidth: 3,
      originX: 'center',
      originY: 'center',
      isVisualHandle: true  // Tag for identification
    });
  }
  
  /**
   * Create an invisible drag handle for interaction
   * @param {'start'|'end'} handleType
   * @returns {fabric.Circle}
   * @private
   */
  _createDragHandle(handleType) {
    const angle = handleType === 'start' ? this.arcStart : this.arcEnd;
    const pos = this._getHandlePosition(angle);
    
    const handle = new fabric.Circle({
      left: pos.x,
      top: pos.y,
      radius: 15, // Slightly larger for easier grabbing
      fill: 'transparent',
      stroke: 'transparent',
      originX: 'center',
      originY: 'center',
      selectable: true,
      evented: true,
      hasControls: false,
      hasBorders: false,
      hoverCursor: 'grab',
      moveCursor: 'grabbing',
      // Custom properties
      isHandle: true,
      handleType: handleType,
      knobGuide: this
    });
    
    this._setupHandleConstraint(handle, handleType);
    
    return handle;
  }
  
  /**
   * Calculate handle position in absolute canvas coordinates
   * @param {number} angleDeg
   * @returns {{x: number, y: number}}
   * @private
   */
  _getHandlePosition(angleDeg) {
    const rad = (angleDeg * Math.PI) / 180;
    const effectiveRadius = this.getEffectiveRadius();
    return {
      x: this.x + effectiveRadius * Math.cos(rad),
      y: this.y + effectiveRadius * Math.sin(rad)
    };
  }
  
  /**
   * Set up constraint and update logic for a drag handle
   * @param {fabric.Circle} handle
   * @param {'start'|'end'} handleType
   * @private
   */
  _setupHandleConstraint(handle, handleType) {
    handle.on('moving', () => {
      const currentX = handle.left;
      const currentY = handle.top;
      
      // Constrain to circle perimeter
      const effectiveRadius = this.getEffectiveRadius();
      const constrained = constrainToCircle(
        currentX,
        currentY,
        this.x,
        this.y,
        effectiveRadius
      );
      
      // Update handle position
      handle.set({
        left: constrained.x,
        top: constrained.y
      });
      
      // Update arc angle
      if (handleType === 'start') {
        this.arcStart = constrained.angle;
      } else {
        this.arcEnd = constrained.angle;
      }
      
      // Rebuild the visual group with new angles
      this._rebuildGroup();
      
      // Notify listener of arc change
      if (this._onArcChange) {
        this._onArcChange(this.arcStart, this.arcEnd);
      }
    });
  }
  
  /**
   * Rebuild the visual group (called when arc angles change)
   * @private
   */
  _rebuildGroup() {
    if (!this.canvas || !this._fabricObject) return;
    
    // Store current transform
    const left = this._fabricObject.left;
    const top = this._fabricObject.top;
    const scaleX = this._fabricObject.scaleX;
    const scaleY = this._fabricObject.scaleY;
    const angle = this._fabricObject.angle;
    
    // Check if this object was selected
    const wasSelected = this.canvas.getActiveObject() === this._fabricObject;
    
    // Remove old group from canvas
    this.canvas.remove(this._fabricObject);
    
    // Update arc markers with new angles
    this.arcMarkers = new window.ArcMarkers({
      values: this.control.values,
      arcStart: this.arcStart,
      arcEnd: this.arcEnd,
      radius: this._baseRadius
    });
    
    // Rebuild the group
    this._buildGroup();
    
    // Restore transform
    this._fabricObject.set({ left, top, scaleX, scaleY, angle });
    this._fabricObject.setCoords();
    
    // Add back to canvas
    this.canvas.add(this._fabricObject);
    
    // Restore selection if it was selected
    if (wasSelected) {
      this.canvas.setActiveObject(this._fabricObject);
    }
    
    // Ensure drag handles are on top
    if (this._startDragHandle) this.canvas.bringToFront(this._startDragHandle);
    if (this._endDragHandle) this.canvas.bringToFront(this._endDragHandle);
    
    this.canvas.requestRenderAll();
  }
  
  /**
   * Set up event handlers for group transform updates
   * @private
   */
  _setupTransformEvents() {
    if (!this._fabricObject) return;
    
    this._fabricObject.on('moving', () => {
      this._updatePositionFromFabric();
      this._updateDragHandlePositions();
    });
    
    this._fabricObject.on('scaling', () => {
      this._updateSizeFromFabric();
      this._updateDragHandlePositions();
    });
    
    this._fabricObject.on('modified', () => {
      this._updatePositionFromFabric();
      this._updateSizeFromFabric();
      this._updateDragHandlePositions();
    });
  }
  
  /**
   * Update drag handle positions to match guide
   * @private
   */
  _updateDragHandlePositions() {
    if (this._startDragHandle) {
      const pos = this._getHandlePosition(this.arcStart);
      this._startDragHandle.set({ left: pos.x, top: pos.y });
      this._startDragHandle.setCoords();
    }
    
    if (this._endDragHandle) {
      const pos = this._getHandlePosition(this.arcEnd);
      this._endDragHandle.set({ left: pos.x, top: pos.y });
      this._endDragHandle.setCoords();
    }
    
    if (this.canvas) {
      this.canvas.requestRenderAll();
    }
  }
  
  /**
   * Update internal position from Fabric.js object
   * @private
   */
  _updatePositionFromFabric() {
    if (!this._fabricObject) return;
    this.x = this._fabricObject.left;
    this.y = this._fabricObject.top;
  }
  
  /**
   * Update internal size from Fabric.js object
   * @private
   */
  _updateSizeFromFabric() {
    if (!this._fabricObject) return;
    const scale = this._fabricObject.scaleX;
    this.radius = this._baseRadius * scale;
  }
  
  /**
   * Get the current effective radius (accounting for scale)
   * @returns {number}
   */
  getEffectiveRadius() {
    if (!this._fabricObject) return this.radius;
    return this._baseRadius * this._fabricObject.scaleX;
  }
  
  /**
   * Get the start drag handle
   * @returns {fabric.Circle|null}
   */
  getStartHandle() {
    return this._startDragHandle;
  }
  
  /**
   * Get the end drag handle
   * @returns {fabric.Circle|null}
   */
  getEndHandle() {
    return this._endDragHandle;
  }
  
  /**
   * Get the main Fabric.js group
   * @returns {fabric.Group|null}
   */
  getFabricObject() {
    return this._fabricObject;
  }
  
  /**
   * Update arc angles
   * @param {number} startDeg
   * @param {number} endDeg
   */
  setArcAngles(startDeg, endDeg) {
    this.arcStart = startDeg;
    this.arcEnd = endDeg;
    this._rebuildGroup();
    this._updateDragHandlePositions();
    
    // Notify listener of arc change
    if (this._onArcChange) {
      this._onArcChange(this.arcStart, this.arcEnd);
    }
  }
  
  /**
   * Update position
   * @param {number} x - Center X
   * @param {number} y - Center Y
   */
  setPosition(x, y) {
    this.x = x;
    this.y = y;
    
    if (this._fabricObject) {
      this._fabricObject.set({ left: x, top: y });
      this._fabricObject.setCoords();
    }
    
    this._updateDragHandlePositions();
    
    if (this.canvas) {
      this.canvas.requestRenderAll();
    }
  }
  
  /**
   * Update size (diameter)
   * @param {number} width - Diameter (width = height for circle)
   */
  setSize(width) {
    const newRadius = width / 2;
    const scale = newRadius / this._baseRadius;
    
    if (this._fabricObject) {
      this._fabricObject.set({ scaleX: scale, scaleY: scale });
      this._fabricObject.setCoords();
    }
    
    this.radius = newRadius;
    this._updateDragHandlePositions();
    
    if (this.canvas) {
      this.canvas.requestRenderAll();
    }
  }
  
  /**
   * Update the control values
   * @param {(string|number)[]} values
   */
  setValues(values) {
    this.control.values = values;
    this._rebuildGroup();
  }
  
  /**
   * Set the selected state of the guide (visual feedback)
   * @param {boolean} isSelected
   */
  setSelected(isSelected) {
    this._isSelected = isSelected;
    const style = isSelected ? STYLE.SELECTED : STYLE.DESELECTED;
    
    // Update the main group
    if (this._fabricObject) {
      this._fabricObject.set({ opacity: style.opacity });
      
      // Update objects in the group
      const objects = this._fabricObject.getObjects();
      objects.forEach(obj => {
        if (obj.type === 'circle' && obj.radius === this._baseRadius) {
          // Main circle
          obj.set({
            stroke: style.strokeColor,
            strokeWidth: style.strokeWidth
          });
        } else if (obj.isVisualHandle) {
          // Visual handle indicators (blueish circles) - hide when not selected
          obj.set({ visible: style.showHandles });
        }
      });
    }
    
    // Update drag handles visibility/style
    if (this._startDragHandle) {
      this._startDragHandle.set({
        visible: isSelected,
        evented: isSelected
      });
    }
    if (this._endDragHandle) {
      this._endDragHandle.set({
        visible: isSelected,
        evented: isSelected
      });
    }
    
    if (this.canvas) {
      this.canvas.requestRenderAll();
    }
  }
  
  /**
   * Check if the guide is currently selected
   * @returns {boolean}
   */
  isSelected() {
    return this._isSelected;
  }
}
