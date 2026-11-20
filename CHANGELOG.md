# Changelog

All notable changes to WetReducer are recorded here, in [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format.

## [Unreleased]

## [1.0.1] - 2026-11-20

### Fixed
- Linux: the plug-in now loads on systems without `libxcb-cursor` installed, such as a fresh Linux Mint.

### Changed
- The sound and saved settings are unchanged.

## [1.0.0] - 2026-10-14

### Added

- Five bands - BASS, BODY, VOICE, PRESENCE, AIR - each rebuilt from its own loudest notes as sine or square waves snapped to the nearest semitone.
- One stepped knob per band (0 the original, noon all sine, full all square), Shift for four times finer.
- One frequency strip with six handles on a semitone grid: where BASS starts, the four splits, where AIR ends.
- VOICES 1-8 per band, RESPONSE slow or fast, SOLO and a note LED per band.
- Mono and stereo buses in and out; VST3 for Windows, macOS and Linux, and an Audio Unit for Logic and GarageBand.
