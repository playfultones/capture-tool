# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.1] - 2026-02-04

### Changed
- Request high channel count to support multi-channel interfaces

### Fixed
- Prevent crashes during device reconfiguration by managing audio callbacks
- Ensure app bundle is re-signed after modifying resources

## [0.2.0] - 2026-02-03

### Added
- Visual capture guide feature with camera overlay and adjustable controls
- Camera rotation functionality and UI support
- Visual guide management system
- Camera permission settings for visual capture guide overlay
- Audio initialization and permission handling
- Default output folder is now set to project directory if not specified

### Changed
- Updated README to remove redundant structure sections and added visual guide image

### Fixed
- Capture matrix generation logic
- Defer device change listener registration to prevent crashes during initialization
- Update autoSaveProject to use existing project file if available
- Capture tail remaining logic to handle playback state correctly
- Visual handle indicators and opacity settings for knob styles
- Camera container class handling for active and creation states
- Update GAIN control type from continuous to discrete in documentation

## [0.1.3] - 2026-01-18

### Added
- Support for multiple output file paths in capture items
- Support for roundtrip entries in capture list and UI
- Peak hold reset functionality and meter UI hold value display

### Changed
- Enhanced LevelMeter with sample rate handling for accurate RMS integration
- Updated signal loading to use project sample rate for validation

## [0.1.2] - 2026-01-17

### Added
- Enhanced reference signal management

### Changed
- Updated documentation

### Fixed
- Preserve all channel bitmasks when switching audio devices

## [0.1.1] - 2026-01-17

### Added
- Multi-reference signal management
- Git-based versioning

## [0.1.0] - 2026-01-16

### Added

- Initial release of Reference Capture Tool
- Audio I/O setup with device enumeration and channel selection
- Support for 44.1/48/96 kHz sample rates
- Real-time input/output level metering (RMS + peak, dBFS)
- Test tone generator (1kHz sine at -18dBFS) for calibration
- Output gain trim (±12dB) for level calibration
- Reference signal loading and preview (mono WAV)
- Capture matrix system for defining controls and parameter values
- Automatic capture list generation from matrix (cartesian product)
- Capture workflow with confirm/capture/skip actions
- Configurable recording tail (0/250/500/1000ms)
- Auto-export with standardized naming convention
- Capture log generation (JSON metadata for ML pipeline)
- Project save/load for session resumption
- Auto-save after each capture
- 4-step calibration wizard for gain staging
- WebView-based UI (HTML/CSS/JS) with JUCE 8 backend
