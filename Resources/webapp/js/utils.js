// Utility functions used across modules

/**
 * Escape HTML special characters
 * @param {string} str 
 * @returns {string}
 */
function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

/**
 * Convert dB value to meter percentage (0-100)
 * Range: -60dB to 0dB
 * @param {number} dbValue - Level in dBFS
 * @returns {number} Percentage (0-100)
 */
function dbToPercent(dbValue) {
    const minDb = -60;
    const maxDb = 0;
    
    if (dbValue <= minDb) return 0;
    if (dbValue >= maxDb) return 100;
    
    return ((dbValue - minDb) / (maxDb - minDb)) * 100;
}

/**
 * Format dB value for display
 * @param {number} dbValue - Level in dBFS
 * @returns {string} Formatted string
 */
function formatDb(dbValue) {
    if (dbValue <= -100) return '-inf';
    return dbValue.toFixed(1);
}

/**
 * Format duration in seconds to MM:SS.mmm
 * @param {number} seconds 
 * @returns {string}
 */
function formatDuration(seconds) {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins}:${secs.toFixed(3).padStart(6, '0')}`;
}

/**
 * Format time in milliseconds to M:SS.s format
 * @param {number} ms - Time in milliseconds
 * @returns {string} Formatted time string
 */
function formatCaptureTime(ms) {
    const totalSeconds = ms / 1000;
    const mins = Math.floor(totalSeconds / 60);
    const secs = totalSeconds % 60;
    return `${mins}:${secs.toFixed(1).padStart(4, '0')}`;
}

/**
 * Format sample rate for display
 * @param {number} rate - Sample rate in Hz
 * @returns {string} Formatted string (e.g., "44.1 kHz")
 */
function formatSampleRate(rate) {
    return `${(rate / 1000).toFixed(rate % 1000 === 0 ? 0 : 1)} kHz`;
}

/**
 * Format gain trim value for display
 * @param {number} dbValue - Gain in dB
 * @returns {string} Formatted string with sign
 */
function formatGainTrim(dbValue) {
    const sign = dbValue >= 0 ? '+' : '';
    return `${sign}${dbValue.toFixed(1)} dB`;
}

/**
 * Format monitor gain for display
 * @param {number} dbValue - Gain in dB
 * @returns {string} Formatted string
 */
function formatMonitorGain(dbValue) {
    if (dbValue <= -60) return '-inf';
    const sign = dbValue >= 0 ? '+' : '';
    return `${sign}${dbValue.toFixed(1)} dB`;
}

/**
 * Populate a select element with options
 * @param {HTMLSelectElement} select 
 * @param {Array<{value: string, label: string}>} options 
 * @param {string} selectedValue 
 */
function populateSelect(select, options, selectedValue = '') {
    select.innerHTML = '';
    
    if (options.length === 0) {
        const option = document.createElement('option');
        option.value = '';
        option.textContent = 'No devices available';
        select.appendChild(option);
        select.disabled = true;
        return;
    }
    
    for (const opt of options) {
        const option = document.createElement('option');
        option.value = opt.value;
        option.textContent = opt.label;
        if (opt.value === selectedValue) {
            option.selected = true;
        }
        select.appendChild(option);
    }
    
    select.disabled = false;
}

/**
 * Populate channel select based on channel count
 * @param {HTMLSelectElement} select 
 * @param {number} numChannels 
 * @param {number} selectedChannel 
 */
function populateChannelSelect(select, numChannels, selectedChannel = 0) {
    const options = [];
    
    if (numChannels === 0) {
        select.innerHTML = '<option value="">No channels</option>';
        select.disabled = true;
        return;
    }
    
    for (let i = 0; i < numChannels; i++) {
        options.push({
            value: String(i),
            label: `Channel ${i + 1}`
        });
    }
    
    populateSelect(select, options, String(selectedChannel));
}
