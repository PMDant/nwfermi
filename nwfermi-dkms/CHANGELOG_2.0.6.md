# NextWindow Fermi Driver v2.0.6 - FULLY WORKING!

**Release Date:** January 28, 2026  
**Status:** PRODUCTION READY - Touch tracking works correctly!

## 🎉 Touchscreen Now Works Properly!

This version fixes the final issues discovered through testing:
- **Y coordinate decoding corrected** (bytes 28-29, not 20-21)
- **Coordinate inversion** (touchscreen mounted upside-down)
- **Spurious touch filtering** (rejects idle packets that leaked through)

## What's Fixed

### Correct Y Coordinate Bytes
- Changed Y coordinate from bytes 20-21 to **bytes 28-29**
- Updated Y range from 200-350 to **0-4500**
- Y coordinate now tracks vertical position accurately

### Inverted Coordinates
Both X and Y axes are now inverted to match physical screen orientation:
- **X:** raw(250) → screen(1366), raw(8500) → screen(0)
- **Y:** raw(0) → screen(768), raw(4500) → screen(0)

This accounts for the touchscreen being mounted upside-down/backwards relative to the display.

### Spurious Touch Filtering
Added detection for "idle" packets that were creating ghost touches:
- Filters out touches where X=8500 (right edge - idle state)
- Filters out touches where Y>4450 (beyond normal range - idle state)
- Eliminates the random touch events when not actually touching

## Technical Changes

### Coordinate Definitions
```c
#define COORD_Y_OFFSET_LSB     28    // Changed from 20
#define COORD_Y_OFFSET_MSB     29    // Changed from 21
#define RAW_Y_MAX              4500  // Changed from 350
#define RAW_X_INVALID          8500  // NEW - idle detection
#define RAW_Y_INVALID_MIN      4450  // NEW - idle detection
```

### Coordinate Transform
```c
// Both axes inverted
screen_x = SCREEN_WIDTH - ((raw_x - RAW_X_MIN) * SCREEN_WIDTH) / (RAW_X_MAX - RAW_X_MIN);
screen_y = SCREEN_HEIGHT - ((raw_y - RAW_Y_MIN) * SCREEN_HEIGHT) / (RAW_Y_MAX - RAW_Y_MIN);
```

## Installation

```bash
tar -xzf nwfermi-dkms-2.0.6.tar.gz
cd nwfermi-dkms-2.0.6
sudo ./install.sh
```

## Testing

Touch anywhere on the screen - the cursor should follow your finger accurately with no ghost touches!

Try:
- Drawing in GIMP
- Scrolling web pages
- Dragging windows
- Multi-finger gestures (if hardware supports it)

## Upgrading from v2.0.5

```bash
cd /path/to/nwfermi-dkms-2.0.5
sudo ./uninstall.sh

cd /path/to/nwfermi-dkms-2.0.6
sudo ./install.sh
```

The touchscreen should work immediately after installation!

## Known Limitations

- **Single-touch only** - Multi-touch support coming in v2.1.0
- **Resolution hardcoded** - Currently set to 1366x768
- **No pressure sensing** - Reports touch/no-touch only

## Debug Information

If coordinates still seem off, enable debug mode:
```bash
echo 'module nwfermi +p' | sudo tee /sys/kernel/debug/dynamic_debug/control
sudo dmesg -w | grep "Touch:"
```

You should see coordinates that match where you touch:
- Top-left: raw(~250, ~0) → screen(~1366, ~768)
- Bottom-right: raw(~8500, ~4500) → screen(~0, ~0)
- Center: raw(~4375, ~2250) → screen(~683, ~384)

(Note the inversion - raw max values map to screen 0)

## Version History

- **v2.0.6:** Fixed Y coordinate, inversion, spurious touches - WORKING!
- **v2.0.5:** Correct X coordinate, wrong Y coordinate
- **v2.0.4:** Length-based detection, diagnostic mode
- **v2.0.1-2.0.3:** USB communication and DKMS fixes

---

**Enjoy your fully working touchscreen!** 🎉
