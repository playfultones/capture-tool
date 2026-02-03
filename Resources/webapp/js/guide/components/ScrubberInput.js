/**
 * ScrubberInput - Draggable number input component
 * 
 * Provides a numeric input with two interaction modes:
 * - Click-drag horizontally to adjust value (like Figma/Blender)
 * - Double-click to enter edit mode and type directly
 * 
 * Features:
 * - Hold Shift for fine control (0.1x sensitivity)
 * - Min/max value clamping
 * - Step-based rounding
 * - Optional suffix display (e.g., '°', 'px')
 */

window.ScrubberInput = class ScrubberInput {
  /**
   * @param {object} options
   * @param {string} options.id - Unique identifier
   * @param {number} options.value - Initial value
   * @param {number} [options.min] - Minimum value
   * @param {number} [options.max] - Maximum value
   * @param {number} [options.step=1] - Step size for dragging
   * @param {string} [options.suffix=''] - Suffix to display (e.g., '°', 'px')
   * @param {Function} options.onChange - Callback when value changes
   */
  constructor(options) {
    this.id = options.id;
    this.value = options.value;
    this.min = options.min ?? -Infinity;
    this.max = options.max ?? Infinity;
    this.step = options.step ?? 1;
    this.suffix = options.suffix ?? '';
    this.onChange = options.onChange;
    
    /** @type {HTMLElement|null} */
    this._element = null;
    
    /** @type {HTMLElement|null} */
    this._displayElement = null;
    
    /** @type {HTMLInputElement|null} */
    this._inputElement = null;
    
    /** @type {boolean} */
    this._isDragging = false;
    
    /** @type {boolean} */
    this._isEditing = false;
    
    /** @type {number} */
    this._dragStartX = 0;
    
    /** @type {number} */
    this._dragStartValue = 0;
    
    /** @type {number|null} requestAnimationFrame ID */
    this._rafId = null;
    
    // Bind methods for event listeners
    this._onMouseDown = this._onMouseDown.bind(this);
    this._onMouseMove = this._onMouseMove.bind(this);
    this._onMouseUp = this._onMouseUp.bind(this);
    this._onDoubleClick = this._onDoubleClick.bind(this);
    this._onInputBlur = this._onInputBlur.bind(this);
    this._onInputKeyDown = this._onInputKeyDown.bind(this);
  }
  
  /**
   * Render the component and return the HTML element
   * @returns {HTMLElement}
   */
  render() {
    this._element = document.createElement('div');
    this._element.className = 'scrubber-input';
    this._element.id = this.id;
    
    // Display element (shown when not editing)
    this._displayElement = document.createElement('span');
    this._displayElement.className = 'scrubber-input__display';
    this._updateDisplay();
    
    // Input element (shown when editing)
    this._inputElement = document.createElement('input');
    this._inputElement.type = 'number';
    this._inputElement.className = 'scrubber-input__input';
    this._inputElement.style.display = 'none';
    if (this.min !== -Infinity) this._inputElement.min = String(this.min);
    if (this.max !== Infinity) this._inputElement.max = String(this.max);
    this._inputElement.step = String(this.step);
    
    this._element.appendChild(this._displayElement);
    this._element.appendChild(this._inputElement);
    
    // Attach event listeners (use pointer events for reliable capture)
    this._displayElement.addEventListener('pointerdown', this._onMouseDown);
    this._displayElement.addEventListener('dblclick', this._onDoubleClick);
    this._inputElement.addEventListener('blur', this._onInputBlur);
    this._inputElement.addEventListener('keydown', this._onInputKeyDown);
    
    return this._element;
  }
  
  /**
   * Update the displayed value
   * @private
   */
  _updateDisplay() {
    if (this._displayElement) {
      this._displayElement.textContent = `${Math.round(this.value)}${this.suffix}`;
    }
  }
  
  /**
   * Set the value and trigger onChange
   * @param {number} newValue
   * @private
   */
  _setValue(newValue) {
    // Clamp to min/max
    newValue = Math.max(this.min, Math.min(this.max, newValue));
    
    // Round to step
    newValue = Math.round(newValue / this.step) * this.step;
    
    if (newValue !== this.value) {
      this.value = newValue;
      this._updateDisplay();
      this.onChange?.(this.value);
    }
  }
  
  /**
   * Update value from external source (without triggering onChange)
   * Ignored if currently dragging to prevent feedback loops
   * @param {number} newValue
   */
  setValue(newValue) {
    // Don't update while dragging - prevents feedback loop
    if (this._isDragging) return;
    
    this.value = newValue;
    this._updateDisplay();
    if (this._inputElement) {
      this._inputElement.value = String(Math.round(newValue));
    }
  }
  
  /**
   * Check if currently being dragged
   * @returns {boolean}
   */
  isDragging() {
    return this._isDragging;
  }
  
  /**
   * Handle mouse down - start dragging
   * @param {MouseEvent} e
   * @private
   */
  _onMouseDown(e) {
    if (this._isEditing) return;
    
    e.preventDefault();
    e.stopPropagation();
    
    this._isDragging = true;
    this._dragStartX = e.clientX;
    this._dragStartValue = this.value;
    
    // Capture pointer to ensure we receive all events even during heavy DOM updates
    this._displayElement?.setPointerCapture(e.pointerId);
    
    // Add cursor style to body during drag
    document.body.style.cursor = 'ew-resize';
    this._element?.classList.add('scrubber-input--dragging');
    
    // Listen for pointer move/up on the element (with capture)
    this._displayElement?.addEventListener('pointermove', this._onMouseMove);
    this._displayElement?.addEventListener('pointerup', this._onMouseUp);
    this._displayElement?.addEventListener('lostpointercapture', this._onMouseUp);
  }
  
  /**
   * Handle mouse move - update value while dragging
   * @param {MouseEvent} e
   * @private
   */
  _onMouseMove(e) {
    if (!this._isDragging) return;
    
    const deltaX = e.clientX - this._dragStartX;
    // Sensitivity: 1px = 0.5 * step
    const sensitivity = e.shiftKey ? 0.1 : 0.5;
    const deltaValue = deltaX * sensitivity * this.step;
    
    const newValue = this._dragStartValue + deltaValue;
    
    // Use requestAnimationFrame to batch updates and prevent blocking
    if (this._rafId) {
      cancelAnimationFrame(this._rafId);
    }
    this._rafId = requestAnimationFrame(() => {
      this._rafId = null;
      this._setValue(newValue);
    });
  }
  
  /**
   * Handle mouse up - stop dragging
   * @param {PointerEvent} e
   * @private
   */
  _onMouseUp(e) {
    if (!this._isDragging) return;
    
    // Cancel any pending RAF
    if (this._rafId) {
      cancelAnimationFrame(this._rafId);
      this._rafId = null;
    }
    
    this._isDragging = false;
    document.body.style.cursor = '';
    this._element?.classList.remove('scrubber-input--dragging');
    
    // Release pointer capture
    if (e.pointerId !== undefined) {
      this._displayElement?.releasePointerCapture(e.pointerId);
    }
    
    this._displayElement?.removeEventListener('pointermove', this._onMouseMove);
    this._displayElement?.removeEventListener('pointerup', this._onMouseUp);
    this._displayElement?.removeEventListener('lostpointercapture', this._onMouseUp);
  }
  
  /**
   * Handle double-click - enter edit mode
   * @param {MouseEvent} e
   * @private
   */
  _onDoubleClick(e) {
    e.preventDefault();
    this._enterEditMode();
  }
  
  /**
   * Enter edit mode (show input, hide display)
   * @private
   */
  _enterEditMode() {
    if (!this._inputElement || !this._displayElement) return;
    
    this._isEditing = true;
    this._displayElement.style.display = 'none';
    this._inputElement.style.display = 'block';
    this._inputElement.value = String(Math.round(this.value));
    this._inputElement.focus();
    this._inputElement.select();
  }
  
  /**
   * Exit edit mode (hide input, show display)
   * @private
   */
  _exitEditMode() {
    if (!this._inputElement || !this._displayElement) return;
    
    this._isEditing = false;
    this._displayElement.style.display = '';
    this._inputElement.style.display = 'none';
  }
  
  /**
   * Handle input blur - commit value and exit edit mode
   * @private
   */
  _onInputBlur() {
    if (!this._inputElement) return;
    
    const newValue = parseFloat(this._inputElement.value);
    if (!isNaN(newValue)) {
      this._setValue(newValue);
    }
    
    this._exitEditMode();
  }
  
  /**
   * Handle input keydown - Enter to commit, Escape to cancel
   * @param {KeyboardEvent} e
   * @private
   */
  _onInputKeyDown(e) {
    if (e.key === 'Enter') {
      e.preventDefault();
      this._inputElement?.blur();
    } else if (e.key === 'Escape') {
      e.preventDefault();
      // Reset input value and exit without saving
      if (this._inputElement) {
        this._inputElement.value = String(Math.round(this.value));
      }
      this._exitEditMode();
    }
  }
  
  /**
   * Clean up event listeners
   */
  destroy() {
    if (this._rafId) {
      cancelAnimationFrame(this._rafId);
      this._rafId = null;
    }
    if (this._displayElement) {
      this._displayElement.removeEventListener('pointerdown', this._onMouseDown);
      this._displayElement.removeEventListener('dblclick', this._onDoubleClick);
      this._displayElement.removeEventListener('pointermove', this._onMouseMove);
      this._displayElement.removeEventListener('pointerup', this._onMouseUp);
      this._displayElement.removeEventListener('lostpointercapture', this._onMouseUp);
    }
    if (this._inputElement) {
      this._inputElement.removeEventListener('blur', this._onInputBlur);
      this._inputElement.removeEventListener('keydown', this._onInputKeyDown);
    }
  }
}
