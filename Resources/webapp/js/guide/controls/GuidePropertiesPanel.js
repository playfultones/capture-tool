/**
 * GuidePropertiesPanel - UI for Selected Guide Properties
 * 
 * Displays and allows editing of the currently selected guide's properties:
 * - Position (X, Y) with drag-scrubber inputs
 * - Size (diameter) with drag-scrubber input
 * - Arc range (start/end angles) with drag-scrubber inputs
 * - Delete guide action
 * 
 * Uses ScrubberInput components for precise value adjustment via click-drag.
 */

// Uses window.ScrubberInput from ScrubberInput.js

window.GuidePropertiesPanel = class GuidePropertiesPanel {
  /**
   * @param {GuideState} state
   * @param {GuideOverlayManager} overlayManager
   */
  constructor(state, overlayManager) {
    this.state = state;
    this.overlayManager = overlayManager;
    
    /** @type {HTMLElement|null} */
    this.containerElement = null;
    
    /** @type {HTMLElement|null} */
    this.contentElement = null;
    
    /** @type {Map<string, ScrubberInput>} Active scrubber inputs */
    this._scrubbers = new Map();
  }
  
  /**
   * Initialize the panel
   */
  init() {
    this.containerElement = document.getElementById('guide-properties-section');
    this.contentElement = document.getElementById('guide-properties-content');
    
    // Subscribe to state changes
    this.state.on('guide:selected', () => this._render());
    this.state.on('guide:added', () => this._render());
    this.state.on('guide:removed', () => this._render());
    this.state.on('guide:updated', (guide) => this._onGuideUpdated(guide));
    
    // Initial render
    this._render();
  }
  
  /**
   * Handle guide updated - update scrubber values without full re-render
   * @param {object} guide
   * @private
   */
  _onGuideUpdated(guide) {
    if (guide.controlId !== this.state.selectedGuideId) return;
    
    const knobGuide = guide.knobGuide;
    if (!knobGuide) return;
    
    // Update scrubber values directly (avoids re-render flicker during drag)
    this._scrubbers.get('guide-x')?.setValue(Math.round(knobGuide.x));
    this._scrubbers.get('guide-y')?.setValue(Math.round(knobGuide.y));
    this._scrubbers.get('guide-size')?.setValue(Math.round(knobGuide.getEffectiveRadius() * 2));
    this._scrubbers.get('guide-arc-start')?.setValue(Math.round(knobGuide.arcStart));
    this._scrubbers.get('guide-arc-end')?.setValue(Math.round(knobGuide.arcEnd));
  }
  
  /**
   * Clean up existing scrubbers
   * @private
   */
  _cleanupScrubbers() {
    for (const scrubber of this._scrubbers.values()) {
      scrubber.destroy();
    }
    this._scrubbers.clear();
  }
  
  /**
   * Render the panel content
   * @private
   */
  _render() {
    if (!this.contentElement) return;
    
    // Clean up existing scrubbers before re-rendering
    this._cleanupScrubbers();
    
    const selectedGuideId = this.state.selectedGuideId;
    
    if (!selectedGuideId) {
      this.contentElement.innerHTML = `
        <p class="guide-properties__placeholder">No guide selected</p>
      `;
      return;
    }
    
    const guide = this.state.getGuide(selectedGuideId);
    const control = this.state.getControl(selectedGuideId);
    
    if (!guide || !control) {
      this.contentElement.innerHTML = `
        <p class="guide-properties__placeholder">No guide selected</p>
      `;
      return;
    }
    
    // Get values from knobGuide (real-time values during manipulation)
    const knobGuide = guide.knobGuide;
    const arcStart = knobGuide?.arcStart ?? guide.arcStart;
    const arcEnd = knobGuide?.arcEnd ?? guide.arcEnd;
    const x = Math.round(knobGuide?.x ?? 0);
    const y = Math.round(knobGuide?.y ?? 0);
    const diameter = Math.round((knobGuide?.getEffectiveRadius() ?? 50) * 2);
    
    // Build DOM structure
    this.contentElement.innerHTML = `
      <div class="guide-properties__info">
        <div class="guide-properties__row">
          <span class="guide-properties__label">Control:</span>
          <span class="guide-properties__value">${this._escapeHtml(control.name)}</span>
        </div>
        
        <div class="guide-properties__group">
          <div class="guide-properties__group-title">Position</div>
          <div class="guide-properties__row">
            <label class="guide-properties__label">X:</label>
            <span id="scrubber-x"></span>
          </div>
          <div class="guide-properties__row">
            <label class="guide-properties__label">Y:</label>
            <span id="scrubber-y"></span>
          </div>
        </div>
        
        <div class="guide-properties__group">
          <div class="guide-properties__group-title">Size</div>
          <div class="guide-properties__row">
            <label class="guide-properties__label">Diameter:</label>
            <span id="scrubber-size"></span>
          </div>
        </div>
        
        <div class="guide-properties__group">
          <div class="guide-properties__group-title">Arc Range</div>
          <div class="guide-properties__row">
            <label class="guide-properties__label">Start:</label>
            <span id="scrubber-arc-start"></span>
          </div>
          <div class="guide-properties__row">
            <label class="guide-properties__label">End:</label>
            <span id="scrubber-arc-end"></span>
          </div>
        </div>
      </div>
      <div class="guide-properties__actions">
        <button class="guide-properties__delete-btn" id="delete-guide-btn" type="button">
          Delete Guide
        </button>
      </div>
    `;
    
    // Create and mount scrubber inputs
    this._createScrubbers(x, y, diameter, arcStart, arcEnd);
    
    // Attach delete button listener
    const deleteBtn = document.getElementById('delete-guide-btn');
    deleteBtn?.addEventListener('click', () => this._onDeleteClick());
  }
  
  /**
   * Create scrubber inputs and mount them to the DOM
   * @param {number} x
   * @param {number} y
   * @param {number} diameter
   * @param {number} arcStart
   * @param {number} arcEnd
   * @private
   */
  _createScrubbers(x, y, diameter, arcStart, arcEnd) {
    // X position
    const xScrubber = new window.ScrubberInput({
      id: 'guide-x',
      value: x,
      step: 1,
      suffix: '',
      onChange: (value) => this._onPositionChange(value, null)
    });
    this._scrubbers.set('guide-x', xScrubber);
    document.getElementById('scrubber-x')?.replaceWith(xScrubber.render());
    
    // Y position
    const yScrubber = new window.ScrubberInput({
      id: 'guide-y',
      value: y,
      step: 1,
      suffix: '',
      onChange: (value) => this._onPositionChange(null, value)
    });
    this._scrubbers.set('guide-y', yScrubber);
    document.getElementById('scrubber-y')?.replaceWith(yScrubber.render());
    
    // Diameter
    const sizeScrubber = new window.ScrubberInput({
      id: 'guide-size',
      value: diameter,
      min: 20,
      step: 1,
      suffix: '',
      onChange: (value) => this._onSizeChange(value)
    });
    this._scrubbers.set('guide-size', sizeScrubber);
    document.getElementById('scrubber-size')?.replaceWith(sizeScrubber.render());
    
    // Arc start
    const arcStartScrubber = new window.ScrubberInput({
      id: 'guide-arc-start',
      value: Math.round(arcStart),
      min: 0,
      max: 360,
      step: 1,
      suffix: '°',
      onChange: (value) => this._onArcStartChange(value)
    });
    this._scrubbers.set('guide-arc-start', arcStartScrubber);
    document.getElementById('scrubber-arc-start')?.replaceWith(arcStartScrubber.render());
    
    // Arc end
    const arcEndScrubber = new window.ScrubberInput({
      id: 'guide-arc-end',
      value: Math.round(arcEnd),
      min: 0,
      max: 360,
      step: 1,
      suffix: '°',
      onChange: (value) => this._onArcEndChange(value)
    });
    this._scrubbers.set('guide-arc-end', arcEndScrubber);
    document.getElementById('scrubber-arc-end')?.replaceWith(arcEndScrubber.render());
  }
  
  /**
   * Handle position change from scrubber
   * @param {number|null} newX
   * @param {number|null} newY
   * @private
   */
  _onPositionChange(newX, newY) {
    const guide = this._getSelectedGuide();
    if (!guide?.knobGuide) return;
    
    const x = newX ?? guide.knobGuide.x;
    const y = newY ?? guide.knobGuide.y;
    guide.knobGuide.setPosition(x, y);
  }
  
  /**
   * Handle size change from scrubber
   * @param {number} diameter
   * @private
   */
  _onSizeChange(diameter) {
    const guide = this._getSelectedGuide();
    if (!guide?.knobGuide) return;
    
    guide.knobGuide.setSize(diameter);
  }
  
  /**
   * Handle arc start change from scrubber
   * @param {number} arcStart
   * @private
   */
  _onArcStartChange(arcStart) {
    const guide = this._getSelectedGuide();
    if (!guide?.knobGuide) return;
    
    guide.knobGuide.setArcAngles(arcStart, guide.knobGuide.arcEnd);
  }
  
  /**
   * Handle arc end change from scrubber
   * @param {number} arcEnd
   * @private
   */
  _onArcEndChange(arcEnd) {
    const guide = this._getSelectedGuide();
    if (!guide?.knobGuide) return;
    
    guide.knobGuide.setArcAngles(guide.knobGuide.arcStart, arcEnd);
  }
  
  /**
   * Get the currently selected guide
   * @returns {object|undefined}
   * @private
   */
  _getSelectedGuide() {
    const selectedGuideId = this.state.selectedGuideId;
    if (!selectedGuideId) return undefined;
    return this.state.getGuide(selectedGuideId);
  }
  
  /**
   * Handle delete button click
   * @private
   */
  _onDeleteClick() {
    this.overlayManager.deleteSelectedGuide();
  }
  
  /**
   * Escape HTML to prevent XSS
   * @param {string} str
   * @returns {string}
   * @private
   */
  _escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }
}
