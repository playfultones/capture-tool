/**
 * GuideState - Visual Guide State Management
 * 
 * Event emitter pattern for coordinating UI updates across guide components.
 * Controls are managed by capture.js - this only manages guides and camera.
 * 
 * ## Events
 * 
 * - `devices:changed` - Camera device list updated
 * - `camera:changed` - Active camera stream changed
 * - `camera:cleared` - Camera stream stopped
 * - `camera:error` - Camera error occurred
 * - `camera:rotated` - Camera rotation changed
 * - `guide:added` - New guide created
 * - `guide:updated` - Guide properties changed
 * - `guide:removed` - Guide deleted
 * - `guide:selected` - Guide selection changed
 * - `guide:create-requested` - User requested guide creation
 */

window.GuideState = class GuideState {
  constructor() {
    /**
     * Event listeners map
     * @type {Map<string, Function[]>}
     * @private
     */
    this._listeners = new Map();
    
    /**
     * Camera state
     * @type {{deviceId: string|null, stream: MediaStream|null, devices: MediaDeviceInfo[], rotation: number}}
     */
    this.camera = {
      deviceId: null,
      stream: null,
      devices: [],
      rotation: 0  // Rotation in degrees (0, 90, 180, 270)
    };
    
    /**
     * Guide instances linking controls to visual overlays
     * @type {Array<{
     *   controlId: string,
     *   fabricObject: fabric.Group,
     *   knobGuide: KnobGuide,
     *   arcStart: number,
     *   arcEnd: number
     * }>}
     */
    this.guides = [];
    
    /**
     * Currently selected guide's control ID
     * @type {string|null}
     */
    this.selectedGuideId = null;
    
    /**
     * Reference to capture controls state (set by guide-init.js)
     * @type {object|null}
     */
    this.captureControlsState = null;
  }
  
  /**
   * Subscribe to state events
   * @param {string} event - Event name
   * @param {Function} callback - Callback function
   */
  on(event, callback) {
    if (!this._listeners.has(event)) {
      this._listeners.set(event, []);
    }
    this._listeners.get(event).push(callback);
  }
  
  /**
   * Unsubscribe from state events
   * @param {string} event - Event name
   * @param {Function} callback - Callback function
   */
  off(event, callback) {
    const callbacks = this._listeners.get(event);
    if (callbacks) {
      const index = callbacks.indexOf(callback);
      if (index > -1) {
        callbacks.splice(index, 1);
      }
    }
  }
  
  /**
   * Emit state event
   * @param {string} event - Event name
   * @param {*} data - Event data
   */
  emit(event, data) {
    const callbacks = this._listeners.get(event);
    if (callbacks) {
      callbacks.forEach(callback => callback(data));
    }
  }
  
  // Camera operations
  
  /**
   * Set available camera devices
   * @param {MediaDeviceInfo[]} devices
   */
  setDevices(devices) {
    this.camera.devices = devices;
    this.emit('devices:changed', devices);
  }
  
  /**
   * Set active camera
   * @param {string} deviceId
   * @param {MediaStream} stream
   */
  setActiveCamera(deviceId, stream) {
    this.camera.deviceId = deviceId;
    this.camera.stream = stream;
    this.emit('camera:changed', { deviceId, stream });
  }
  
  /**
   * Clear active camera
   */
  clearCamera() {
    this.camera.deviceId = null;
    this.camera.stream = null;
    this.emit('camera:cleared');
  }
  
  /**
   * Rotate camera by 90 degrees clockwise
   */
  rotateCamera() {
    this.camera.rotation = (this.camera.rotation + 90) % 360;
    this.emit('camera:rotated', this.camera.rotation);
  }
  
  /**
   * Set camera rotation directly
   * @param {number} degrees - Rotation in degrees (0, 90, 180, 270)
   */
  setCameraRotation(degrees) {
    this.camera.rotation = degrees % 360;
    this.emit('camera:rotated', this.camera.rotation);
  }
  
  // Control lookup (delegates to capture.js state)
  
  /**
   * Get a control by ID from capture controls state
   * @param {string} id
   * @returns {object|undefined}
   */
  getControl(id) {
    if (!this.captureControlsState?.controls) return undefined;
    return this.captureControlsState.controls.find(c => c.id === id);
  }
  
  /**
   * Check if a guide exists for a control
   * @param {string} controlId
   * @returns {boolean}
   */
  hasGuideForControl(controlId) {
    return this.guides.some(g => g.controlId === controlId);
  }
  
  // Guide operations
  
  /**
   * Add a guide
   * @param {object} guide - { controlId, fabricObject, knobGuide, arcStart, arcEnd }
   */
  addGuide(guide) {
    this.guides.push(guide);
    this.emit('guide:added', guide);
  }
  
  /**
   * Update a guide
   * @param {string} controlId
   * @param {object} updates
   */
  updateGuide(controlId, updates) {
    const guide = this.guides.find(g => g.controlId === controlId);
    if (guide) {
      Object.assign(guide, updates);
      this.emit('guide:updated', guide);
    }
  }
  
  /**
   * Remove a guide
   * @param {string} controlId
   */
  removeGuide(controlId) {
    const index = this.guides.findIndex(g => g.controlId === controlId);
    if (index > -1) {
      const [removed] = this.guides.splice(index, 1);
      
      // Clear selection if this guide was selected
      if (this.selectedGuideId === controlId) {
        this.selectGuide(null);
      }
      
      this.emit('guide:removed', removed);
    }
  }
  
  /**
   * Get guide by control ID
   * @param {string} controlId
   * @returns {object|undefined}
   */
  getGuide(controlId) {
    return this.guides.find(g => g.controlId === controlId);
  }
  
  /**
   * Select a guide
   * @param {string|null} controlId
   */
  selectGuide(controlId) {
    this.selectedGuideId = controlId;
    this.emit('guide:selected', controlId);
  }
  
  // Serialization
  
  /**
   * Get canvas dimensions for normalization
   * @returns {{width: number, height: number}}
   */
  getCanvasDimensions() {
    const container = document.getElementById('guide-canvas-container');
    if (container) {
      return { width: container.clientWidth || 640, height: container.clientHeight || 480 };
    }
    return { width: 640, height: 480 };
  }
  
  /**
   * Serialize guides for project save
   * @returns {Array<object>}
   */
  serializeGuides() {
    const { width, height } = this.getCanvasDimensions();
    const minDim = Math.min(width, height);
    
    return this.guides.map(g => {
      const kg = g.knobGuide;
      return {
        controlId: g.controlId,
        x: kg ? kg.x / width : 0.5,
        y: kg ? kg.y / height : 0.5,
        radius: kg ? kg._baseRadius / minDim : 0.1,
        arcStart: g.arcStart,
        arcEnd: g.arcEnd
      };
    });
  }
  
  /**
   * Serialize camera selection for project save
   * @returns {string|null}
   */
  serializeCamera() {
    return this.camera.deviceId;
  }
  
  /**
   * Clear all guides (for project reset)
   */
  clearGuides() {
    // Remove each guide to trigger proper cleanup
    const controlIds = this.guides.map(g => g.controlId);
    for (const controlId of controlIds) {
      this.removeGuide(controlId);
    }
  }
};
