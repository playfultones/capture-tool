/**
 * CameraManager - Camera Device Enumeration and Stream Handling
 * 
 * Manages camera access and selection:
 * - Enumerates available video input devices
 * - Handles camera start/stop with error handling
 * - Persists selected camera in session storage
 * - Provides video element for canvas overlay alignment
 */

/** Session storage key for remembering selected camera */
const STORAGE_KEY_SELECTED_CAMERA = 'vcg_selected_camera';

window.GuideCameraManager = class GuideCameraManager {
  /**
   * @param {GuideState} state
   */
  constructor(state) {
    this.state = state;
    
    /** @type {HTMLVideoElement|null} */
    this.videoElement = null;
    
    /** @type {HTMLSelectElement|null} */
    this.selectElement = null;
    
    /** @type {HTMLButtonElement|null} */
    this.toggleButton = null;
    
    /** @type {HTMLButtonElement|null} */
    this.rotateButton = null;
  }
  
  /**
   * Initialize the camera manager
   */
  async init() {
    // Get DOM elements (using guide-specific IDs)
    this.videoElement = document.getElementById('guide-video');
    this.selectElement = document.getElementById('guide-camera-select');
    this.toggleButton = document.getElementById('guide-camera-toggle');
    this.rotateButton = document.getElementById('guide-camera-rotate');
    
    // Set up event listeners
    this.selectElement?.addEventListener('change', () => this._onDeviceChange());
    this.toggleButton?.addEventListener('click', () => this._onToggleClick());
    this.rotateButton?.addEventListener('click', () => this._onRotateClick());
    
    // Listen for rotation state changes to update video element
    this.state.on('camera:rotated', (rotation) => this._applyRotation(rotation));
    
    // Enumerate devices
    await this.enumerateDevices();
  }
  
  /**
   * Enumerate available video devices
   * @returns {Promise<MediaDeviceInfo[]>}
   */
  async enumerateDevices() {
    // Check if mediaDevices API is available
    if (!navigator.mediaDevices || !navigator.mediaDevices.enumerateDevices) {
      console.error('MediaDevices API not supported');
      this._showError('Camera access not supported in this browser');
      return [];
    }
    
    try {
      // Request permission first (needed to get device labels)
      await navigator.mediaDevices.getUserMedia({ video: true })
        .then(stream => {
          // Stop the stream immediately, we just needed permission
          stream.getTracks().forEach(track => track.stop());
        })
        .catch(error => {
          // Permission denied or no camera available
          if (error.name === 'NotAllowedError') {
            console.warn('Camera permission denied');
          } else if (error.name === 'NotFoundError') {
            console.warn('No camera device found');
          }
          // Continue anyway - we can still list devices (without labels)
        });
      
      const devices = await navigator.mediaDevices.enumerateDevices();
      const videoDevices = devices.filter(device => device.kind === 'videoinput');
      
      this.state.setDevices(videoDevices);
      this._populateSelect(videoDevices);
      
      return videoDevices;
    } catch (error) {
      console.error('Failed to enumerate devices:', error);
      this._showError('Failed to access cameras');
      return [];
    }
  }
  
  /**
   * Display error message to user
   * @param {string} message
   * @private
   */
  _showError(message) {
    if (this.selectElement) {
      this.selectElement.innerHTML = `<option value="">${message}</option>`;
      this.selectElement.disabled = true;
    }
    if (this.toggleButton) {
      this.toggleButton.disabled = true;
    }
  }
  
  /**
   * Start camera stream from specific device
   * @param {string} deviceId
   * @returns {Promise<boolean>} Success status
   */
  async startCamera(deviceId) {
    try {
      // Stop any existing stream
      this.stopCamera();
      
      const constraints = {
        video: {
          deviceId: deviceId ? { exact: deviceId } : undefined
        }
      };
      
      const stream = await navigator.mediaDevices.getUserMedia(constraints);
      
      if (this.videoElement) {
        this.videoElement.srcObject = stream;
      }
      
      this.state.setActiveCamera(deviceId, stream);
      this._updateToggleButton(true);
      
      return true;
      
    } catch (error) {
      console.error('Failed to start camera:', error);
      this._handleStreamError(error);
      return false;
    }
  }
  
  /**
   * Handle stream errors with user-friendly messages
   * @param {Error} error
   * @private
   */
  _handleStreamError(error) {
    let message = 'Failed to start camera';
    
    switch (error.name) {
      case 'NotAllowedError':
        message = 'Camera permission denied';
        break;
      case 'NotFoundError':
        message = 'Camera not found';
        break;
      case 'NotReadableError':
        message = 'Camera is in use by another application';
        break;
      case 'OverconstrainedError':
        message = 'Camera does not meet requirements';
        break;
      case 'AbortError':
        message = 'Camera access was aborted';
        break;
    }
    
    this.state.emit('camera:error', { error, message });
    this._updateToggleButton(false);
  }
  
  /**
   * Stop current camera stream
   */
  stopCamera() {
    const stream = this.state.camera.stream;
    
    if (stream) {
      stream.getTracks().forEach(track => track.stop());
    }
    
    if (this.videoElement) {
      this.videoElement.srcObject = null;
    }
    
    this.state.clearCamera();
    this._updateToggleButton(false);
  }
  
  /**
   * Get the video element for canvas rendering
   * @returns {HTMLVideoElement|null}
   */
  getVideoElement() {
    return this.videoElement;
  }
  
  /**
   * Check if camera is currently active
   * @returns {boolean}
   */
  isActive() {
    return this.state.camera.stream !== null;
  }
  
  // Private methods
  
  /**
   * Populate the camera select dropdown
   * @param {MediaDeviceInfo[]} devices
   * @private
   */
  _populateSelect(devices) {
    if (!this.selectElement) return;
    
    // Handle no cameras found
    if (devices.length === 0) {
      this.selectElement.innerHTML = '<option value="">No cameras found</option>';
      this.selectElement.disabled = true;
      if (this.toggleButton) {
        this.toggleButton.disabled = true;
      }
      return;
    }
    
    // Clear existing options and add placeholder
    this.selectElement.innerHTML = '<option value="">Select camera...</option>';
    this.selectElement.disabled = false;
    if (this.toggleButton) {
      this.toggleButton.disabled = false;
    }
    
    devices.forEach((device, index) => {
      const option = document.createElement('option');
      option.value = device.deviceId;
      option.textContent = device.label || `Camera ${index + 1}`;
      this.selectElement.appendChild(option);
    });
    
    // Restore previously selected camera from session storage
    this._restoreSelectedCamera(devices);
  }
  
  /**
   * Restore previously selected camera from session storage
   * @param {MediaDeviceInfo[]} devices
   * @private
   */
  _restoreSelectedCamera(devices) {
    const savedDeviceId = sessionStorage.getItem(STORAGE_KEY_SELECTED_CAMERA);
    
    if (savedDeviceId && this.selectElement) {
      // Check if the saved device is still available
      const deviceExists = devices.some(d => d.deviceId === savedDeviceId);
      
      if (deviceExists) {
        this.selectElement.value = savedDeviceId;
      }
    }
  }
  
  /**
   * Save selected camera to session storage
   * @param {string} deviceId
   * @private
   */
  _saveSelectedCamera(deviceId) {
    if (deviceId) {
      sessionStorage.setItem(STORAGE_KEY_SELECTED_CAMERA, deviceId);
    } else {
      sessionStorage.removeItem(STORAGE_KEY_SELECTED_CAMERA);
    }
  }
  
  /**
   * Handle device selection change
   * @private
   */
  _onDeviceChange() {
    const deviceId = this.selectElement?.value;
    
    // Save selection to session storage
    this._saveSelectedCamera(deviceId);
    
    if (deviceId && this.isActive()) {
      // If camera is already active, switch to new device
      this.startCamera(deviceId);
    }
  }
  
  /**
   * Handle toggle button click
   * @private
   */
  _onToggleClick() {
    if (this.isActive()) {
      this.stopCamera();
    } else {
      const deviceId = this.selectElement?.value;
      if (deviceId) {
        this.startCamera(deviceId);
      }
    }
  }
  
  /**
   * Update toggle button text
   * @param {boolean} isActive
   * @private
   */
  _updateToggleButton(isActive) {
    if (this.toggleButton) {
      this.toggleButton.textContent = isActive ? 'Stop Camera' : 'Start Camera';
    }
    // Enable/disable rotate button based on camera state
    if (this.rotateButton) {
      this.rotateButton.disabled = !isActive;
    }
  }
  
  /**
   * Handle rotate button click
   * @private
   */
  _onRotateClick() {
    this.state.rotateCamera();
  }
  
  /**
   * Apply rotation to video element
   * @param {number} rotation - Rotation in degrees (0, 90, 180, 270)
   * @private
   */
  _applyRotation(rotation) {
    if (!this.videoElement) return;
    
    // Remove all rotation classes
    this.videoElement.classList.remove('rotated-90', 'rotated-180', 'rotated-270');
    
    // Add appropriate rotation class
    if (rotation === 90) {
      this.videoElement.classList.add('rotated-90');
    } else if (rotation === 180) {
      this.videoElement.classList.add('rotated-180');
    } else if (rotation === 270) {
      this.videoElement.classList.add('rotated-270');
    }
  }
}
