# v2.0.7 Changelog

**Date:** January 28, 2026

## Fixed
- **Spurious touch filtering:** Changed filter from `X==8500 OR Y>4450` to `X>10000 OR Y>10000`
  - Previous filter was too aggressive, blocking legitimate touches at X=8500 (right edge)
  - New filter catches garbage packets (raw values like 64778, 61953) while allowing valid touches

## Technical Changes
```c
// OLD (v2.0.6):
#define RAW_X_INVALID 8500
#define RAW_Y_INVALID_MIN 4450
if (raw_x == RAW_X_INVALID || raw_y > RAW_Y_INVALID_MIN) return;

// NEW (v2.0.7):
#define RAW_COORD_MAX_VALID 10000
if (raw_x > RAW_COORD_MAX_VALID || raw_y > RAW_COORD_MAX_VALID) return;
```

## Installation
```bash
tar -xzf nwfermi-dkms-2.0.7.tar.gz
cd nwfermi-dkms-2.0.7
sudo make clean && sudo make install
```

Or if using DKMS:
```bash
sudo dkms remove nwfermi/2.0.6 --all  # remove old version
sudo dkms install /path/to/nwfermi-dkms-2.0.7
```

## Expected Behavior
- Touch anywhere: coordinates should track accurately
- No ghost touches when not touching the screen
- Right edge touches (X≈8500) now work correctly
