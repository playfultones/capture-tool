// Calibration wizard module

/**
 * Calibration state
 */
const calibrationState = {
    completed: false,
    completedAt: null,
    testToneLevelDbfs: -18.0,
    unityLevelDbfs: null,
    maxLevelDbfs: null,
    outputTrimDb: 0.0,
    notes: ''
};

/**
 * Calibration wizard controller
 */
const calibrationWizard = {
    isOpen: false,
    currentStep: 1,
    totalSteps: 4,
    
    // Track levels during calibration
    outputRms: -100,
    inputRms: -100,
    inputPeak: -100,
    peakHold: -100,
    
    // Wizard trim value (temporary, applied on finish)
    wizardTrimDb: 0.0,
    
    // Meter update unsubscribe function
    meterUnsubscribe: null,
    
    /**
     * Open the calibration wizard
     */
    open() {
        this.isOpen = true;
        this.currentStep = 1;
        this.peakHold = -100;
        this.wizardTrimDb = outputGainTrimDb; // Start with current trim value
        
        const modal = document.getElementById('calibration-wizard');
        modal.classList.remove('hidden');
        document.body.classList.add('modal-open');
        
        // Subscribe to meter updates for wizard
        this.meterUnsubscribe = backend.onEvent('meterUpdate', (data) => {
            this.onMeterUpdate(data);
        });
        
        this.renderStep();
        this.updateNavigation();
    },
    
    /**
     * Close the calibration wizard
     */
    close() {
        this.isOpen = false;
        
        const modal = document.getElementById('calibration-wizard');
        modal.classList.add('hidden');
        document.body.classList.remove('modal-open');
        
        // Unsubscribe from meter updates
        if (this.meterUnsubscribe) {
            this.meterUnsubscribe();
            this.meterUnsubscribe = null;
        }
        
        // Stop test tone if playing
        if (testToneEnabled) {
            toggleTestTone();
        }
    },
    
    /**
     * Go to next step
     */
    async nextStep() {
        if (this.currentStep === 1) {
            // Step 1 -> 2: Start test tone
            if (!testToneEnabled) {
                await toggleTestTone();
            }
        }
        
        if (this.currentStep === 2) {
            // Step 2 -> 3: Record unity level
            calibrationState.unityLevelDbfs = this.inputRms;
        }
        
        if (this.currentStep === 4) {
            // Finish calibration
            await this.finish();
            return;
        }
        
        this.currentStep++;
        this.renderStep();
        this.updateNavigation();
    },
    
    /**
     * Go to previous step
     */
    prevStep() {
        if (this.currentStep > 1) {
            this.currentStep--;
            this.renderStep();
            this.updateNavigation();
        }
    },
    
    /**
     * Finish calibration and save state
     */
    async finish() {
        // Record final values
        calibrationState.maxLevelDbfs = this.peakHold;
        calibrationState.outputTrimDb = this.wizardTrimDb;
        calibrationState.completed = true;
        calibrationState.completedAt = new Date().toISOString();
        
        // Apply the trim value to the actual output trim
        if (this.wizardTrimDb !== outputGainTrimDb) {
            await backend.call('setOutputGainTrim', this.wizardTrimDb);
            outputGainTrimDb = this.wizardTrimDb;
            document.getElementById('output-gain-trim').value = outputGainTrimDb;
            updateOutputTrimDisplay();
        }
        
        // Save calibration state to backend
        await backend.call('setCalibrationState', {
            completed: calibrationState.completed,
            completedAt: calibrationState.completedAt,
            testToneLevelDbfs: calibrationState.testToneLevelDbfs,
            unityLevelDbfs: calibrationState.unityLevelDbfs,
            maxLevelDbfs: calibrationState.maxLevelDbfs,
            outputTrimDb: calibrationState.outputTrimDb
        });
        
        // Update main UI calibration status
        updateCalibrationStatus();
        
        this.close();
    },
    
    /**
     * Handle meter updates during wizard
     */
    onMeterUpdate(data) {
        if (!this.isOpen) return;
        
        this.outputRms = data.output?.rmsDb ?? -100;
        this.inputRms = data.input?.rmsDb ?? -100;
        this.inputPeak = data.input?.peakDb ?? -100;
        
        // Update peak hold
        if (this.inputPeak > this.peakHold) {
            this.peakHold = this.inputPeak;
        }
        
        this.updateStepMeters();
    },
    
    /**
     * Reset peak hold value
     */
    resetPeakHold() {
        this.peakHold = -100;
    },
    
    /**
     * Update wizard trim value
     */
    setWizardTrim(value) {
        this.wizardTrimDb = Math.max(-12, Math.min(12, value));
    },
    
    /**
     * Get level status for step 2 (unity check)
     */
    getLevelMatchStatus() {
        const diff = this.inputRms - this.outputRms;
        const absDiff = Math.abs(diff);
        
        if (this.outputRms <= -60) {
            return { status: 'info', message: 'Test tone not detected', icon: '--' };
        }
        
        if (absDiff <= 2) {
            return { status: 'good', message: 'Levels matched', icon: '>' };
        }
        
        if (this.inputRms > -3) {
            return { status: 'error', message: 'Input too hot - reduce gain', icon: '!' };
        }
        
        if (diff < -2) {
            return { status: 'warning', message: 'Adjust input gain UP', icon: '^' };
        }
        
        return { status: 'warning', message: 'Adjust input gain DOWN', icon: 'v' };
    },
    
    /**
     * Get level status for step 4 (capture level)
     * Uses output trim to control level going to unit.
     */
    getCaptureStatus() {
        // Calculate effective output level (test tone + trim)
        const effectiveOutput = TEST_TONE_LEVEL_DBFS + this.wizardTrimDb;
        
        // Check OUTPUT clipping first (user boosted too much)
        if (effectiveOutput > 0) {
            return { status: 'error', message: 'Output clipping - reduce output trim', icon: '!' };
        }
        
        // Check INPUT clipping (unit too hot)
        if (this.peakHold > 0) {
            return { status: 'error', message: 'Input clipping - reduce output trim or interface gain', icon: '!' };
        }
        
        if (this.peakHold > -3) {
            return { status: 'warning', message: 'Very hot - consider reducing output trim', icon: '!' };
        }
        
        if (this.peakHold >= -12 && this.peakHold <= -3) {
            return { status: 'good', message: 'Good level', icon: '>' };
        }
        
        if (this.peakHold < -20) {
            return { status: 'info', message: 'Signal quiet - could increase output trim', icon: '--' };
        }
        
        return { status: 'good', message: 'Acceptable level', icon: '>' };
    },
    
    /**
     * Render current step content
     */
    renderStep() {
        const content = document.getElementById('wizard-step-content');
        const indicator = document.getElementById('wizard-step-indicator');
        
        indicator.textContent = `Step ${this.currentStep} of ${this.totalSteps}`;
        
        switch (this.currentStep) {
            case 1:
                content.innerHTML = this.renderStep1();
                break;
            case 2:
                content.innerHTML = this.renderStep2();
                break;
            case 3:
                content.innerHTML = this.renderStep3();
                break;
            case 4:
                content.innerHTML = this.renderStep4();
                this.initStep4Controls();
                break;
        }
    },
    
    /**
     * Step 1: Connect Output
     */
    renderStep1() {
        return `
            <h3 class="wizard-step-title">Connect Output</h3>
            <div class="wizard-step-description">
                <p>1. Connect your interface OUTPUT to your reamp box or directly to your unit's input.</p>
                <p>2. Make sure your interface output level is set to a reasonable level (not muted, not at maximum).</p>
                <p>The test tone will play when you click Next.</p>
            </div>
            <div class="wizard-levels">
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Output Device</span>
                    <span class="wizard-level-value">${audioState.currentOutputDevice || 'None'}</span>
                </div>
            </div>
            <div class="wizard-status status-info">
                <span class="wizard-status-icon">*</span>
                <span>Ready to start calibration</span>
            </div>
        `;
    },
    
    /**
     * Step 2: Check Unity Gain
     */
    renderStep2() {
        const status = this.getLevelMatchStatus();
        return `
            <h3 class="wizard-step-title">Check Unity Gain</h3>
            <div class="wizard-step-description">
                <p>Connect your interface OUTPUT directly back to the INPUT (bypass the unit for now).</p>
                <p>Adjust your interface's INPUT GAIN knob until the input level matches the output level (both around -18 dBFS).</p>
            </div>
            <div class="wizard-levels">
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Output Level</span>
                    <div class="wizard-level-bar">
                        <div class="wizard-level-fill" id="wizard-output-rms" style="width: 0%"></div>
                    </div>
                    <span class="wizard-level-value" id="wizard-output-value">-inf dBFS</span>
                </div>
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Input Level</span>
                    <div class="wizard-level-bar">
                        <div class="wizard-level-fill" id="wizard-input-rms" style="width: 0%"></div>
                    </div>
                    <span class="wizard-level-value" id="wizard-input-value">-inf dBFS</span>
                </div>
            </div>
            <div class="wizard-status status-${status.status}" id="wizard-step2-status">
                <span class="wizard-status-icon">${status.icon}</span>
                <span id="wizard-step2-message">${status.message}</span>
            </div>
        `;
    },
    
    /**
     * Step 3: Insert Unit
     */
    renderStep3() {
        return `
            <h3 class="wizard-step-title">Insert Unit</h3>
            <div class="wizard-step-description">
                <p>Now insert your unit into the signal chain:</p>
                <p>Interface OUT -> Unit IN -> Unit OUT -> Interface IN</p>
                <p>Set your unit to BYPASS or UNITY GAIN:</p>
                <p>- If it has a bypass switch, use it</p>
                <p>- Otherwise, set controls to produce no change</p>
                <p>The input level should still be close to -18 dBFS.</p>
            </div>
            <div class="wizard-levels">
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Output Level</span>
                    <div class="wizard-level-bar">
                        <div class="wizard-level-fill" id="wizard-output-rms" style="width: 0%"></div>
                    </div>
                    <span class="wizard-level-value" id="wizard-output-value">-inf dBFS</span>
                </div>
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Input Level</span>
                    <div class="wizard-level-bar">
                        <div class="wizard-level-fill" id="wizard-input-rms" style="width: 0%"></div>
                    </div>
                    <span class="wizard-level-value" id="wizard-input-value">-inf dBFS</span>
                </div>
            </div>
            <div class="wizard-status status-info" id="wizard-step3-status">
                <span class="wizard-status-icon">*</span>
                <span id="wizard-step3-message">Verify unit at unity/bypass</span>
            </div>
        `;
    },
    
    /**
     * Step 4: Set Capture Level
     * Uses output trim to control signal level going to unit.
     * If input clips, reduce output trim to send quieter signal to unit.
     */
    renderStep4() {
        const effectiveOutput = TEST_TONE_LEVEL_DBFS + this.wizardTrimDb;
        const status = this.getCaptureStatus();
        return `
            <h3 class="wizard-step-title">Set Capture Level</h3>
            <div class="wizard-step-description">
                <p>Set your unit to its <strong>LOUDEST</strong> setting that you plan to capture (max gain, max volume, etc.).</p>
                <p>If the input level clips, reduce the Output Trim below. This sends a quieter signal to the unit, reducing its output.</p>
            </div>
            <div class="wizard-levels">
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Output (to unit)</span>
                    <div class="wizard-level-bar">
                        <div class="wizard-level-fill" id="wizard-output-rms" style="width: 0%"></div>
                    </div>
                    <span class="wizard-level-value" id="wizard-output-value">-inf dBFS</span>
                </div>
                <div class="wizard-level-row">
                    <span class="wizard-level-label">Input (from unit)</span>
                    <div class="wizard-level-bar">
                        <div class="wizard-level-fill" id="wizard-input-rms" style="width: 0%"></div>
                        <div class="wizard-level-peak" id="wizard-input-peak" style="left: 0%"></div>
                    </div>
                    <span class="wizard-level-value" id="wizard-input-value">-inf dBFS</span>
                </div>
            </div>
            <div class="wizard-trim-control">
                <label>Output Trim</label>
                <input type="range" id="wizard-trim-slider" min="-12" max="12" step="0.1" value="${this.wizardTrimDb}">
                <span class="wizard-trim-value" id="wizard-trim-value">${formatGainTrim(this.wizardTrimDb)}</span>
            </div>
            <div class="wizard-level-row" style="margin-top: 0.75rem; padding: 0.5rem 1rem; background: var(--bg-primary); border-radius: 6px;">
                <span class="wizard-level-label">Effective Output</span>
                <span class="wizard-level-value" id="wizard-effective-output">${formatDb(effectiveOutput)} dBFS</span>
            </div>
            <div class="wizard-peak-hold">
                <span class="wizard-peak-hold-label">Input Peak Hold</span>
                <span class="wizard-peak-hold-value" id="wizard-peak-hold-value">${formatDb(this.peakHold)} dBFS</span>
                <button class="wizard-peak-hold-reset" id="wizard-peak-reset">Reset</button>
            </div>
            <div class="wizard-status status-${status.status}" id="wizard-step4-status">
                <span class="wizard-status-icon">${status.icon}</span>
                <span id="wizard-step4-message">${status.message}</span>
            </div>
            <p style="margin-top: 0.75rem; font-size: 0.8125rem; color: var(--text-secondary);">
                Tip: If clipping persists at -12dB output trim, also reduce your interface's input gain knob.
            </p>
        `;
    },
    
    /**
     * Initialize step 4 controls
     */
    initStep4Controls() {
        const slider = document.getElementById('wizard-trim-slider');
        const resetBtn = document.getElementById('wizard-peak-reset');
        
        if (slider) {
            slider.addEventListener('input', async () => {
                this.wizardTrimDb = parseFloat(slider.value);
                document.getElementById('wizard-trim-value').textContent = formatGainTrim(this.wizardTrimDb);
                this.updateStep4Display();
                
                // Apply output trim to backend immediately so user can hear the effect
                await backend.call('setOutputGainTrim', this.wizardTrimDb);
            });
        }
        
        if (resetBtn) {
            resetBtn.addEventListener('click', () => {
                this.resetPeakHold();
            });
        }
    },
    
    /**
     * Update step 4 display
     */
    updateStep4Display() {
        const effectiveOutput = TEST_TONE_LEVEL_DBFS + this.wizardTrimDb;
        const status = this.getCaptureStatus();
        
        const effectiveOutputEl = document.getElementById('wizard-effective-output');
        const statusEl = document.getElementById('wizard-step4-status');
        const messageEl = document.getElementById('wizard-step4-message');
        
        if (effectiveOutputEl) {
            effectiveOutputEl.textContent = `${formatDb(effectiveOutput)} dBFS`;
        }
        
        if (statusEl && messageEl) {
            statusEl.className = `wizard-status status-${status.status}`;
            statusEl.querySelector('.wizard-status-icon').textContent = status.icon;
            messageEl.textContent = status.message;
        }
    },
    
    /**
     * Update meter displays during calibration
     */
    updateStepMeters() {
        // Update output meter
        const outputRmsEl = document.getElementById('wizard-output-rms');
        const outputValueEl = document.getElementById('wizard-output-value');
        
        if (outputRmsEl && outputValueEl) {
            outputRmsEl.style.width = `${dbToPercent(this.outputRms)}%`;
            outputValueEl.textContent = `${formatDb(this.outputRms)} dBFS`;
        }
        
        // Update input meter
        const inputRmsEl = document.getElementById('wizard-input-rms');
        const inputValueEl = document.getElementById('wizard-input-value');
        const inputPeakEl = document.getElementById('wizard-input-peak');
        
        if (inputRmsEl && inputValueEl) {
            inputRmsEl.style.width = `${dbToPercent(this.inputRms)}%`;
            inputValueEl.textContent = `${formatDb(this.inputRms)} dBFS`;
        }
        
        if (inputPeakEl) {
            inputPeakEl.style.left = `${dbToPercent(this.inputPeak)}%`;
        }
        
        // Update peak hold display
        const peakHoldEl = document.getElementById('wizard-peak-hold-value');
        if (peakHoldEl) {
            peakHoldEl.textContent = `${formatDb(this.peakHold)} dBFS`;
        }
        
        // Update step-specific status
        if (this.currentStep === 2) {
            const status = this.getLevelMatchStatus();
            const statusEl = document.getElementById('wizard-step2-status');
            const messageEl = document.getElementById('wizard-step2-message');
            
            if (statusEl && messageEl) {
                statusEl.className = `wizard-status status-${status.status}`;
                statusEl.querySelector('.wizard-status-icon').textContent = status.icon;
                messageEl.textContent = status.message;
            }
        }
        
        if (this.currentStep === 3) {
            const status = this.getLevelMatchStatus();
            const statusEl = document.getElementById('wizard-step3-status');
            const messageEl = document.getElementById('wizard-step3-message');
            
            if (statusEl && messageEl) {
                statusEl.className = `wizard-status status-${status.status}`;
                statusEl.querySelector('.wizard-status-icon').textContent = status.icon;
                messageEl.textContent = status.message;
            }
        }
        
        if (this.currentStep === 4) {
            this.updateStep4Display();
        }
    },
    
    /**
     * Update navigation buttons
     */
    updateNavigation() {
        const backBtn = document.getElementById('wizard-back');
        const nextBtn = document.getElementById('wizard-next');
        
        // Show/hide back button
        if (this.currentStep === 1) {
            backBtn.classList.add('hidden');
        } else {
            backBtn.classList.remove('hidden');
        }
        
        // Update next button text
        if (this.currentStep === 4) {
            nextBtn.textContent = 'Finish Calibration';
        } else {
            nextBtn.textContent = 'Next';
        }
    }
};

/**
 * Update calibration status display in main UI
 */
function updateCalibrationStatus() {
    const statusText = document.getElementById('calibration-status-text');
    const statusDiv = statusText.parentElement;
    
    if (calibrationState.completed) {
        const completedAt = new Date(calibrationState.completedAt);
        const now = new Date();
        const diffMs = now - completedAt;
        const diffHours = Math.floor(diffMs / (1000 * 60 * 60));
        const diffMins = Math.floor(diffMs / (1000 * 60));
        
        let timeAgo;
        if (diffHours > 0) {
            timeAgo = `${diffHours} hour${diffHours > 1 ? 's' : ''} ago`;
        } else if (diffMins > 0) {
            timeAgo = `${diffMins} min${diffMins > 1 ? 's' : ''} ago`;
        } else {
            timeAgo = 'just now';
        }
        
        statusText.textContent = `Calibrated (${timeAgo})`;
        statusDiv.classList.add('calibrated');
        statusDiv.classList.remove('warning');
    } else {
        statusText.textContent = 'Not calibrated';
        statusDiv.classList.remove('calibrated');
        statusDiv.classList.remove('warning');
    }
}

/**
 * Initialize calibration UI
 */
async function initCalibration() {
    const calibrateBtn = document.getElementById('calibrate-btn');
    const cancelBtn = document.getElementById('wizard-cancel');
    const backBtn = document.getElementById('wizard-back');
    const nextBtn = document.getElementById('wizard-next');
    
    // Open wizard button
    calibrateBtn.addEventListener('click', () => {
        calibrationWizard.open();
    });
    
    // Wizard navigation
    cancelBtn.addEventListener('click', () => {
        calibrationWizard.close();
    });
    
    backBtn.addEventListener('click', () => {
        calibrationWizard.prevStep();
    });
    
    nextBtn.addEventListener('click', () => {
        calibrationWizard.nextStep();
    });
    
    // Load calibration state from backend
    try {
        const state = await backend.call('getCalibrationState');
        if (state && state.completed) {
            calibrationState.completed = state.completed;
            calibrationState.completedAt = state.completedAt;
            calibrationState.testToneLevelDbfs = state.testToneLevelDbfs ?? -18.0;
            calibrationState.unityLevelDbfs = state.unityLevelDbfs;
            calibrationState.maxLevelDbfs = state.maxLevelDbfs;
            calibrationState.outputTrimDb = state.outputTrimDb ?? 0.0;
        }
        updateCalibrationStatus();
    } catch (error) {
        console.error('Failed to load calibration state:', error);
    }
}
