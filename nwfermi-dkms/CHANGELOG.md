# NextWindow Fermi Driver Changelog

## Version 2.0.3 (2026-01-27)

### Fixed
- **CRITICAL**: Eliminated start/stop flapping during initialization
- URBs now run continuously from probe to disconnect  
- Stable operation without repeated start/stop cycles

### Changed
- Removed input device open/close callbacks
- Simplified lifecycle management
- Added disconnected flag for clean shutdown

## Version 2.0.2 (2026-01-27)

### Added
- Complete packet parsing implementation
- Multitouch slot tracking
- Direct Wayland/GNOME support

### Known Issues
- Start/stop flapping (fixed in 2.0.3)
