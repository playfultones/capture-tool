/**
 * ArcMarkers - Value Tick Marks Along Arc
 * Renders radial tick marks with labels at each value position
 * 
 * Angle convention (canvas coordinates, Y increases downward):
 * - 0° = 3 o'clock (right)
 * - 90° = 6 o'clock (down)
 * - 180° = 9 o'clock (left)
 * - 270° = 12 o'clock (up)
 * 
 * Arc direction is always clockwise from start to end.
 */

window.ArcMarkers = class ArcMarkers {
  /**
   * @param {object} options
   * @param {(string|number)[]} options.values - Values to display
   * @param {number} options.arcStart - Arc start angle in degrees
   * @param {number} options.arcEnd - Arc end angle in degrees
   * @param {number} options.radius - Circle radius
   */
  constructor(options) {
    this.values = options.values || [];
    this.arcStart = options.arcStart;
    this.arcEnd = options.arcEnd;
    this.radius = options.radius;
    
    /** Tick mark length extending outward from arc */
    this.markerLength = 12;
    
    /** Distance from tick end to label center */
    this.labelOffset = 16;
  }
  
  /**
   * Calculate evenly distributed angles along the arc for each value
   * Arc goes clockwise from arcStart to arcEnd
   * @returns {Array<{angle: number, value: string|number}>} Array of angle/value pairs
   */
  distributeValuesAlongArc() {
    const positions = [];
    const valueCount = this.values.length;
    
    if (valueCount === 0) {
      return positions;
    }
    
    // Calculate arc span (clockwise direction)
    // If end < start, we're crossing 0° so add 360° to get the actual sweep
    let arcSpan = this.arcEnd - this.arcStart;
    if (arcSpan < 0) {
      arcSpan += 360;
    }
    
    for (let i = 0; i < valueCount; i++) {
      // Single value gets centered, multiple values get distributed
      const t = valueCount === 1 ? 0.5 : i / (valueCount - 1);
      let angle = this.arcStart + (arcSpan * t);
      // Normalize angle to 0-360 range
      if (angle >= 360) {
        angle -= 360;
      }
      positions.push({
        angle,
        value: this.values[i]
      });
    }
    
    return positions;
  }
  
  /**
   * Create Fabric.js objects for markers
   * @param {number} centerX - Guide center X
   * @param {number} centerY - Guide center Y
   * @returns {fabric.Object[]}
   */
  createMarkers(centerX, centerY) {
    // Check if Fabric.js is loaded
    if (typeof fabric === 'undefined') {
      console.error('Fabric.js not loaded');
      return [];
    }
    
    const objects = [];
    const positions = this.distributeValuesAlongArc();
    
    positions.forEach(({ angle, value }) => {
      const rad = (angle * Math.PI) / 180;
      
      // Tick mark start (on the arc)
      const startX = this.radius * Math.cos(rad);
      const startY = this.radius * Math.sin(rad);
      
      // Tick mark end (extending outward)
      const endX = (this.radius + this.markerLength) * Math.cos(rad);
      const endY = (this.radius + this.markerLength) * Math.sin(rad);
      
      // Create tick line (relative to group origin at 0,0)
      const tick = new fabric.Line([startX, startY, endX, endY], {
        stroke: '#ffffff',
        strokeWidth: 2,
        originX: 'center',
        originY: 'center'
      });
      objects.push(tick);
      
      // Label position (beyond tick)
      const labelX = (this.radius + this.markerLength + this.labelOffset) * Math.cos(rad);
      const labelY = (this.radius + this.markerLength + this.labelOffset) * Math.sin(rad);
      
      // Create label
      const label = new fabric.Text(String(value), {
        left: labelX,
        top: labelY,
        fontSize: 10,
        fill: '#ffffff',
        fontFamily: 'sans-serif',
        originX: 'center',
        originY: 'center'
      });
      objects.push(label);
    });
    
    return objects;
  }
  
  /**
   * Update values and return new markers
   * @param {(string|number)[]} values
   * @param {number} centerX
   * @param {number} centerY
   * @returns {fabric.Object[]}
   */
  updateValues(values, centerX, centerY) {
    this.values = values;
    return this.createMarkers(centerX, centerY);
  }
  
  /**
   * Update arc range and return new markers
   * @param {number} arcStart
   * @param {number} arcEnd
   * @param {number} centerX
   * @param {number} centerY
   * @returns {fabric.Object[]}
   */
  updateArcRange(arcStart, arcEnd, centerX, centerY) {
    this.arcStart = arcStart;
    this.arcEnd = arcEnd;
    return this.createMarkers(centerX, centerY);
  }
}
