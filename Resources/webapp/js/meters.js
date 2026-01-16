// Level metering module

/**
 * Update meter UI elements
 * @param {object} meterData - { input: {rmsDb, peakDb}, output: {rmsDb, peakDb}, monitor: {rmsDb, peakDb} }
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
        inputRmsValue.textContent = formatDb(meterData.input.rmsDb);
        inputPeakValue.textContent = formatDb(meterData.input.peakDb);
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
        outputRmsValue.textContent = formatDb(meterData.output.rmsDb);
        outputPeakValue.textContent = formatDb(meterData.output.peakDb);
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
        monitorRmsValue.textContent = formatDb(meterData.monitor.rmsDb);
        monitorPeakValue.textContent = formatDb(meterData.monitor.peakDb);
    }
}
