# NextWindow Fermi Driver - Quick Start

## What This Package Contains

A complete rewrite of the NextWindow Fermi touchscreen driver that works directly with Wayland/GNOME without needing a userspace daemon.

## Installation on Your Touch PC

```bash
# Extract the package
sudo tar xzf nwfermi-dkms-2.0.2.tar.gz -C /usr/src/

# Go to the directory
cd /usr/src/nwfermi-dkms-2.0.2/

# Install
sudo ./install.sh

# Load the driver
sudo modprobe nwfermi
```

## Testing

```bash
# Check if it loaded
lsmod | grep nwfermi

# Check kernel messages
dmesg | tail -20 | grep -i fermi

# Test touch events
sudo apt-get install evtest
sudo evtest
# Select your touchscreen and touch the screen
```

## Important Notes

### This is Version 2.0.2 - Initial Release

This driver has **packet parsing code based on USB traffic analysis**. It should work, but the packet format was reverse-engineered and may need tuning for your specific device.

### If It Doesn't Work Immediately

**Don't worry!** This is expected. Follow the debugging guide:

1. Read `DEBUGGING.md` - it has complete step-by-step instructions
2. Run with debug enabled to see what's happening
3. Capture USB traffic to understand your device's specific format
4. Use the included `analyze_packets.py` tool to analyze the packets

The driver is designed to be easily debuggable and modifiable.

### What We Know

- **Device initialization works** - your previous version proved this
- **USB communication works** - the device sends data
- **Packet format is partially understood** - but may vary by device

### What May Need Adjustment

The packet parsing in `nwfermi.c` around line 100-200. Once we see what your device sends, we can adjust:
- Touch data offset in packets
- Delta vs. absolute coordinates
- Touch state indicators
- Slot assignment logic

## Removing the Python Shim

Once this driver works, you can remove the Python shim:

```bash
# Find shim process
ps aux | grep nwfermi

# Kill it
sudo killall python3  # or specific PID

# Disable autostart if configured
sudo systemctl disable nwfermi-shim
```

## Getting Help

If you need help getting this working:

1. Enable debug output (see DEBUGGING.md)
2. Capture USB traffic
3. Run `./analyze_packets.py` on the capture
4. Share the output along with what happens when you touch the screen

## Files in This Package

- `nwfermi.c` - Main driver source with packet parsing
- `Makefile` - Build configuration
- `dkms.conf` - DKMS integration
- `install.sh` - Automated installation
- `uninstall.sh` - Removal script
- `README.md` - Full documentation
- `DEBUGGING.md` - Step-by-step debugging guide
- `analyze_packets.py` - USB packet analysis tool

## Success Indicators

If it's working, you'll see:
1. Driver loads without errors
2. Kernel messages show "Started reading touch data"
3. `evtest` shows touch events when you touch the screen
4. GNOME responds to your touches
5. No more need for the Python shim!

## License

GPL v2 (same as Linux kernel)
