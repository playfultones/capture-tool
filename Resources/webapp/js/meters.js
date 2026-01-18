// Level metering module

/**
 * Update meter UI elements
 * @param {object} meterData - { input: {rmsDb, peakDb, rmsHoldDb, peakHoldDb}, ... }
 */
function updateMeters(meterData) {
    // Input meters
    const inputRmsBar = document.getElementById('input-rms-bar');
    const inputPeakBar = document.getElementById('input-peak-bar');
    const inputRmsValue = document.getElementById('input-rms-value');
    const inputPeakValue = document.getElementById('input-peak-value');
    
    if (meterData.input) {
        const inputRmsPercent = dbToPercent(meterData.input.rmsDb);
        const inputPeakPercent = dbToPercent(meterData.input.peakDb);
        
        inputRmsBar.style.width = `${inputRmsPercent}%`;
        inputPeakBar.style.left = `${inputPeakPercent}%`;
        // Show hold values in labels (max observed since last reset)
        inputRmsValue.textContent = formatDb(meterData.input.rmsHoldDb);
        inputPeakValue.textContent = formatDb(meterData.input.peakHoldDb);
    }
    
    // Output meters
    const outputRmsBar = document.getElementById('output-rms-bar');
    const outputPeakBar = document.getElementById('output-peak-bar');
    const outputRmsValue = document.getElementById('output-rms-value');
    const outputPeakValue = document.getElementById('output-peak-value');
    
    if (meterData.output) {
        const outputRmsPercent = dbToPercent(meterData.output.rmsDb);
        const outputPeakPercent = dbToPercent(meterData.output.peakDb);
        
        outputRmsBar.style.width = `${outputRmsPercent}%`;
        outputPeakBar.style.left = `${outputPeakPercent}%`;
        // Show hold values in labels (max observed since last reset)
        outputRmsValue.textContent = formatDb(meterData.output.rmsHoldDb);
        outputPeakValue.textContent = formatDb(meterData.output.peakHoldDb);
    }
    
    // Monitor meters
    const monitorRmsBar = document.getElementById('monitor-rms-bar');
    const monitorPeakBar = document.getElementById('monitor-peak-bar');
    const monitorRmsValue = document.getElementById('monitor-rms-value');
    const monitorPeakValue = document.getElementById('monitor-peak-value');
    
    if (meterData.monitor && monitorRmsBar) {
        const monitorRmsPercent = dbToPercent(meterData.monitor.rmsDb);
        const monitorPeakPercent = dbToPercent(meterData.monitor.peakDb);
        
        monitorRmsBar.style.width = `${monitorRmsPercent}%`;
        monitorPeakBar.style.left = `${monitorPeakPercent}%`;
        // Show hold values in labels (max observed since last reset)
        monitorRmsValue.textContent = formatDb(meterData.monitor.rmsHoldDb);
        monitorPeakValue.textContent = formatDb(meterData.monitor.peakHoldDb);
    }
}

/**
 * Reset peak hold values on all meters
 * Call backend to reset, the values will update on next meter refresh
 */
async function resetPeakHold() {
    try {
        await backend.call('resetPeakHold');
    } catch (e) {
        console.error('Failed to reset peak hold:', e);
    }
}

/**
 * Initialize meter click handlers for peak hold reset
 * Clicking on the meter values area resets peak hold
 */
function initMeterClickHandlers() {
    // Get all meter-values containers
    const meterValueContainers = document.querySelectorAll('.meter-values');
    
    meterValueContainers.forEach(container => {
        container.style.cursor = 'pointer';
        container.title = 'Click to reset peak hold';
        container.addEventListener('click', resetPeakHold);
    });
}
