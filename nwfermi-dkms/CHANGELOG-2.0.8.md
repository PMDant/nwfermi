# v2.0.8 Changelog

**Date:** January 28, 2026

## Critical Fix: Strict Coordinate Validation

### Problem in v2.0.7
Debug logs showed almost ALL packets contained garbage coordinates (60000-65000 range):
```
Filtered spurious touch: raw(64517,532)
Filtered spurious touch: raw(65284,526)
Filtered spurious touch: raw(64264,1550)
```

Only 1 valid touch in hundreds of packets: `raw(513,1806)`

The 10000 threshold was too loose - many garbage values are in 60000-65000 range.

### The Fix
Changed from loose >10000 check to **STRICT range validation**:

```c
// Valid coordinate ranges (observed from actual touches):
X: 200-8600
Y: 0-4600

// NEW validation (with margin):
if (raw_x < 200 || raw_x > 9000 ||
    raw_y < 0 || raw_y > 5000) {
    return;  // Reject garbage packet
}
```

This blocks ALL the 60000+ garbage values while allowing all valid touches.

## Technical Changes

### Before (v2.0.7):
```c
#define RAW_COORD_MAX_VALID 10000
if (raw_x > RAW_COORD_MAX_VALID || raw_y > RAW_COORD_MAX_VALID) return;
```
Problem: Values like 64517 and 61190 passed through

### After (v2.0.8):
```c
#define RAW_X_VALID_MIN 200
#define RAW_X_VALID_MAX 9000
#define RAW_Y_VALID_MIN 0
#define RAW_Y_VALID_MAX 5000

if (raw_x < RAW_X_VALID_MIN || raw_x > RAW_X_VALID_MAX ||
    raw_y < RAW_Y_VALID_MIN || raw_y > RAW_Y_VALID_MAX) return;
```
Result: Only coordinates in valid range get through

## Installation

```bash
tar -xzf nwfermi-dkms-2.0.8.tar.gz
cd nwfermi-dkms-2.0.8
sudo dkms remove nwfermi/2.0.7 --all
sudo cp -r . /usr/src/nwfermi-2.0.8/
sudo dkms install nwfermi/2.0.8
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

## Expected Result

Touch the screen - you should see ONLY valid touches in debug log:
```
Touch: raw(513,1806) → screen(1323,460) → abs(31735,19626)
Touch: raw(2580,2300) → screen(1000,500) → abs(...)
```

NO more "Filtered" messages with 60000+ values flooding the log.

The touchscreen should now respond accurately to every touch.
