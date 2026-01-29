# NextWindow Fermi Touchscreen Driver v2.1.5

A modern Linux kernel driver for NextWindow Fermi USB touchscreens, designed to work directly with Wayland/GNOME without requiring a userspace daemon.

## Features

- **No daemon required**: Direct kernel-to-input subsystem communication
- **Wayland/GNOME compatible**: Works natively with modern Linux desktops  
- **Multitouch support**: Full support for up to 10 simultaneous touch points
- **Protocol B multitouch**: Uses modern slotted multitouch protocol
- **Power management**: Proper suspend/resume support
- **DKMS integration**: Automatically rebuilds on kernel updates

## Supported Devices

- HP Compaq Elite 8300 Touch All-in-One PC (0x1926:0x1846, 0x1926:0x1878)
- Lenovo ThinkCentre M92Z (0x1926:0x1875)
- Many other NextWindow Fermi devices (see driver source for full list)

## Installation

### Quick Install YMMV

```bash
sudo ./install.sh
sudo modprobe nwfermi
```

### Manual Installation

```bash
# Install to DKMS
sudo cp -r nwfermi-2.1.5 /usr/src/
sudo dkms add -m nwfermi -v 2.1.5
sudo dkms build -m nwfermi -v 2.1.5
sudo dkms install -m nwfermi -v 2.1.5

# Load the module
sudo modprobe nwfermi
```

## Debugging

If your touchscreen isn't working, enable debug output to see what's happening:

### Enable Kernel Debug Messages

```bash
# Set kernel log level
sudo bash -c 'echo 8 > /proc/sys/kernel/printk'

# Enable dynamic debugging for this module  
sudo bash -c 'echo "module nwfermi +p" > /sys/kernel/debug/dynamic_debug/control'

# Watch kernel messages
sudo dmesg -w | grep -i fermi
```

### Check Device Detection

```bash
# See if device is detected
lsusb | grep 1926

# Check if module is loaded
lsmod | grep nwfermi

# Check input devices
ls -la /dev/input/by-id/ | grep -i next
```

### Monitor Touch Events

```bash
# Install evtest if not available
sudo apt-get install evtest

# List input devices
sudo evtest

# Select your touchscreen device number and touch the screen
# You should see EV_ABS events with coordinates
```

### Capture Raw USB Traffic

To understand what the device is sending, you can capture USB traffic:

```bash
# Load usbmon module
sudo modprobe usbmon

# Find your device bus and device number
lsusb | grep 1926

# Capture traffic (replace X with your bus number)
sudo cat /sys/kernel/debug/usb/usbmon/Xu > /tmp/usb_capture.log

# In another terminal, use the touchscreen
# Then stop capture with Ctrl+C

# View the capture
less /tmp/usb_capture.log
```

## Troubleshooting

### Touchscreen not responding

1. **Check if device is detected**: `lsusb | grep 1926`
2. **Check if module is loaded**: `lsmod | grep nwfermi`
3. **Check kernel messages**: `dmesg | grep -i fermi`
4. **Enable debug output** (see Debugging section above)
5. **Check for conflicts**: `lsmod | grep -i touch` - look for other touchscreen drivers

### nwfermi-daemon & Python shim still needed?

This driver should completely replace the need for the daemon and Python shim. If you still have them running:

```bash
# Find and stop the shim process then the daemon process
ps aux | grep nwfermi
sudo systemctl stop nwfermi-* # may not glob services

# Disable the shim from starting automatically
sudo systemctl disable nwfermi-shim  # if it's a service
```

### Coordinates inverted or wrong

The packet parsing may need tuning for your specific device. Please:

1. Enable debug output
2. Capture a few touch events
3. Share the debug output so we can improve the driver

## Known Issues

- **Initial version**: The packet parsing is based on USB traffic analysis and may need refinement for your specific device
- **Delta encoding**: The device uses delta-encoded coordinates which may need calibration
- **Absolute coordinates**: Some packet types may contain absolute coordinates that aren't fully parsed yet

## How It Works

The driver:

1. Connects to the USB bulk endpoint
2. Receives packets starting with `[]` (0x5B 0x5D) header
3. Parses packet type (0xD6 for touch updates, 0xD7 for other events)
4. Extracts touch coordinates (delta-encoded)
5. Tracks up to 10 simultaneous touches in slots
6. Reports events to the Linux input subsystem
7. Works directly with libinput/Wayland

## Development

### Packet Format

Based on USB traffic analysis:

```
Bytes 0-1:  5B 5D (header "[]")
Byte 2:     Packet sequence number  
Byte 3:     Packet type (D6/D7/09)
Bytes 4-17: Metadata
Bytes 18+:  Touch data (format varies by type)
```

Touch data appears to be delta-encoded signed 8-bit values.

### Contributing

If you have a NextWindow device and can help improve the packet parsing:

1. Enable debug output
2. Capture touch events with different gestures
3. Capture USB traffic with usbmon
4. Share the debug output and describe what you were doing

## Uninstallation

### Automatically: YMMV

```bash
sudo ./uninstall.sh
```

### Or manually:

```bash
sudo rmmod nwfermi
sudo dkms remove -m nwfermi -v 2.1.5 --all
```

## License

GPL v2

## Credits

- Original driver: Daniel Newton
- USB protocol analysis and refactoring: Multiple contributors
- Based on the Linux USB skeleton driver

## Version History

### 2.1.5
- Fix: further spurious touch filtering improvements
- Fix: additional coordinate edge-case handling
- Docs: update README and installation instructions for 2.1.5

### 2.1.4
- Improvement: enhanced delta decoding for certain packet types
- Fix: compatibility fixes for newer kernels
- Perf: reduced input processing latency

### 2.1.3
- Fix: resolved device initialization race on some systems
- Improvement: quieter logging noise in normal operation

### 2.1.2
- Fix: minor packet parsing corrections for some devices
- Improvement: better slot handling for rapid multi-touch
- Tests: added additional debug traces for edge cases

### 2.1.1
- Fix: corrected finger-id tracking for specific packet sequences
- Improvement: reduced latency in multi-touch handling
- Docs: added notes about known hardware variants

### 2.1.0
- Feature: general stability and protocol improvements
- Improvement: more robust delta decoding and filtering
- Fix: address rare initialization race conditions

### 2.0.9
- Fix: improved handling of disconnected devices
- Improvement: better power management behavior

### 2.0.8
- Fix: spurious touch filtering tweaks
- Improvement: compatibility fixes for additional device IDs

### 2.0.7
- Spurious touch filtering

### 2.0.6
- Correct Y Coordinate Bytes
- Inverted Coordinates
- Spurious Touch Filtering

### 2.0.5
- Real Coordinate Parsing
- Kernel Compatibility Fix (Arch 6.18.6+)
- Reduced Logging Noise

### 2.0.4
- **CRITICAL FIX**: Eliminated start/stop flapping during initialization
- URBs now run continuously from probe to disconnect
- Stable operation without repeated start/stop cycles

### 2.0.3
- Removed input device open/close callbacks
- Simplified lifecycle management
- Added disconnected flag for clean shutdown

### 2.0.2
- Complete rewrite with packet parsing
- Direct Wayland/GNOME support
- No daemon required
- Modern multitouch protocol B implementation

### 2.0.0-2.0.1
- Early refactoring attempts
- Device initialization working
- Packet parsing incomplete

### Prior versions
- Original daemon-based implementation
- Character device interface
