# v2.1.1 Changelog

**Date:** January 28, 2026

## Added BTN_TOUCH Reporting for Click Events

### Problem in v2.1.0
Coordinates were correct, but clicks weren't registering:
- Touch events visible in evtest
- Cursor position tracked correctly
- But icons/buttons didn't activate on touch

This is because we were only reporting multi-touch (ABS_MT_*) events without the legacy BTN_TOUCH that triggers actual clicks in many desktop environments.

### The Fix
Added legacy single-touch event reporting alongside multi-touch:

**What we now report on touch:**
```c
// Multi-touch (for position tracking)
input_report_abs(ABS_MT_POSITION_X, ...)
input_report_abs(ABS_MT_POSITION_Y, ...)

// Legacy single-touch (for click events) - NEW!
input_report_key(BTN_TOUCH, 1);     // Press
input_report_abs(ABS_X, ...)
input_report_abs(ABS_Y, ...)
```

**On release:**
```c
input_report_key(BTN_TOUCH, 0);     // Release - NEW!
```

## Why This Matters

Many desktop environments (including GNOME/Wayland) use BTN_TOUCH as the click trigger:
- BTN_TOUCH = 1 → Mouse button down
- BTN_TOUCH = 0 → Mouse button up

Without this, the touchscreen acts like a hovering cursor that never clicks.

## Installation

```bash
tar -xzf nwfermi-dkms-2.1.1.tar.gz
cd nwfermi-dkms-2.1.1
sudo dkms remove nwfermi/2.1.0 --all
sudo cp -r . /usr/src/nwfermi-2.1.1/
sudo dkms install nwfermi/2.1.1
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

## Expected Result

Touch anywhere on screen:
- ✅ Cursor appears at touch location
- ✅ Icons/buttons activate on tap
- ✅ Menus open
- ✅ Text can be selected
- ✅ All UI elements respond to touch

The touchscreen should now work as a fully functional input device!
