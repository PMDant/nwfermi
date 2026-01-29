# Testing and Configuration Guide
## Quick Start

After installing nwfermi, your touchscreen should work immediately. Try:
- Drawing in an app like GIMP or Krita
- Scrolling through web pages
- Clicking buttons in GNOME

## Verifying Touch Coordinates

### Basic Test
```bash
# Watch for touch events
sudo dmesg -w | grep nwfermi
```

You should see stats every 5000 idle packets showing touch activity.

### Detailed Coordinate Debugging
```bash
# Enable debug output
echo 'module nwfermi +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# Watch coordinate mapping in real-time
sudo dmesg -w | grep "Touch:"
```

Example output:
```
Touch: raw(2045,340) → screen(274,760) → abs(6553,31621)
Touch: raw(6928,201) → screen(1023,4) → abs(24519,170)
```

### Test Different Screen Positions

Touch these areas and note the coordinates:

1. **Top-Left Corner**
   - Expected raw: X≈250, Y≈350
   - Expected screen: X≈0, Y≈768

2. **Top-Right Corner**
   - Expected raw: X≈8500, Y≈350
   - Expected screen: X≈1366, Y≈768

3. **Bottom-Left Corner**
   - Expected raw: X≈250, Y≈200
   - Expected screen: X≈0, Y≈0

4. **Bottom-Right Corner**
   - Expected raw: X≈8500, Y≈200
   - Expected screen: X≈1366, Y≈0

5. **Center**
   - Expected raw: X≈4375, Y≈275
   - Expected screen: X≈683, Y≈384

## Adjusting for Your Screen

If your screen resolution is different from 1366x768, you'll need to recompile with your resolution.

### Edit nwfermi.c

Find these lines (around line 57):
```c
/* Screen resolution (adjust for your display) */
#define SCREEN_WIDTH           1366
#define SCREEN_HEIGHT          768
```

Change to your resolution, for example for 1920x1080:
```c
#define SCREEN_WIDTH           1920
#define SCREEN_HEIGHT          1080
```

Then reinstall:
```bash
sudo ./uninstall.sh
sudo ./install.sh
```

## Calibration Issues

### Y-Axis Inverted?
If touches appear upside-down, the Y coordinate might need inversion.

**Symptoms:**
- Touching top of screen activates bottom
- Touching bottom activates top

**Fix:** Edit `nwfermi.c` around line 134, change this line:
```c
screen_y = ((raw_y - RAW_Y_MIN) * SCREEN_HEIGHT) / (RAW_Y_MAX - RAW_Y_MIN);
```

To:
```c
screen_y = SCREEN_HEIGHT - ((raw_y - RAW_Y_MIN) * SCREEN_HEIGHT) / (RAW_Y_MAX - RAW_Y_MIN);
```

### X-Axis Inverted?
If touches appear left-right flipped:

**Fix:** Edit `nwfermi.c` around line 131, change:
```c
screen_x = ((raw_x - RAW_X_MIN) * SCREEN_WIDTH) / (RAW_X_MAX - RAW_X_MIN);
```

To:
```c
screen_x = SCREEN_WIDTH - ((raw_x - RAW_X_MIN) * SCREEN_WIDTH) / (RAW_X_MAX - RAW_X_MIN);
```

### Coordinate Range Adjustment

If touches don't reach the edges of the screen, you may need to adjust the raw coordinate ranges.

Find these lines in `nwfermi.c` (around line 47):
```c
/* Raw coordinate ranges (from device) */
#define RAW_X_MIN              0
#define RAW_X_MAX              8500
#define RAW_Y_MIN              0  
#define RAW_Y_MAX              350
```

Use the debug output to find the actual min/max values when you touch the corners, then update these values.

## Multi-Touch Status

**Current Status:** Single-touch only (slot 0)

Multi-touch support requires:
1. Decoding touch point identifiers from the packet
2. Tracking multiple slots simultaneously
3. Proper contact ID reporting

This will be implemented in v2.1.0 after confirming single-touch works reliably.

## Performance

Expected performance:
- **Latency:** <10ms from touch to cursor movement
- **Sampling Rate:** Matches packet rate (~100-120 Hz)
- **CPU Usage:** Minimal (<1% on modern systems)

## Troubleshooting

### Touch Not Working At All

1. Check driver is loaded:
```bash
lsmod | grep nwfermi
```

2. Check device is detected:
```bash
lsusb | grep -i nextwindow
dmesg | grep nwfermi | tail -20
```

3. Verify input device exists:
```bash
ls -la /dev/input/by-id/*Fermi*
```

### Touches Offset or Inaccurate

1. Enable debug logging and check raw coordinates
2. Verify your SCREEN_WIDTH/HEIGHT settings match your display
3. Check if coordinate inversion is needed

### Erratic Behavior

If touch is jumpy or erratic:
- Check USB cable connection
- Look for USB errors: `dmesg | grep -i usb | tail`
- Try a different USB port (prefer USB 2.0 if available)

## Disable Debug Logging

If you enabled debug logging and want to disable it:
```bash
echo 'module nwfermi -p' | sudo tee /sys/kernel/debug/dynamic_debug/control
```

## Report Issues

If you encounter problems:

1. Collect debug information:
```bash
# Enable debug
echo 'module nwfermi +p' | sudo tee /sys/kernel/debug/dynamic_debug/control

# Touch all four corners and center
# Copy the output:
sudo dmesg | grep -E "(nwfermi|Touch:)" | tail -100 > debug.txt
```

2. Note your system info:
   - Screen resolution
   - Kernel version: `uname -r`
   - Distribution
   - GNOME/Wayland version

---

**Questions?** Check the main README.md for architecture documentation and packet analysis details.
