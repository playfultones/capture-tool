# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
