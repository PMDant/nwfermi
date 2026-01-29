# v2.1.0 Changelog

**Date:** January 28, 2026

## Fixed Y-Axis Inversion

### Problem in v2.0.9
Testing revealed Y-axis was inverted incorrectly:
- User touched **top-left** → Driver reported bottom-left coordinates
- User touched **bottom-right** → Driver reported top-right coordinates

The touchscreen X-axis IS backwards, but Y-axis is NORMAL.

### The Fix
Removed Y-axis inversion while keeping X inverted:

**v2.0.9 (WRONG):**
```c
screen_x = SCREEN_WIDTH - ...;   // Inverted ✓
screen_y = SCREEN_HEIGHT - ...;  // Inverted ✗ (wrong!)
```

**v2.1.0 (CORRECT):**
```c
screen_x = SCREEN_WIDTH - ...;   // Inverted ✓
screen_y = ...;                  // Normal ✓
```

## Coordinate Mapping

Now correctly maps physical touches:
- **Top-left** (raw Y≈5400) → screen(top) Y≈768
- **Bottom-right** (raw Y≈0) → screen(bottom) Y≈0
- **X still inverted** (raw X=250→right, X=8500→left)

## Installation

```bash
tar -xzf nwfermi-dkms-2.1.0.tar.gz
cd nwfermi-dkms-2.1.0
sudo dkms remove nwfermi/2.0.9 --all
sudo cp -r . /usr/src/nwfermi-2.1.0/
sudo dkms install nwfermi/2.1.0
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

## Expected Result

Touch anywhere - cursor should appear EXACTLY where you touch:
- Top-left → Top-left ✓
- Top-right → Top-right ✓
- Bottom-left → Bottom-left ✓
- Bottom-right → Bottom-right ✓

The touchscreen should now be perfectly calibrated!
