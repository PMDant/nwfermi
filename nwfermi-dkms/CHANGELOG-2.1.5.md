# v2.1.5 - Swapped X/Y Byte Coordinates

**Date:** January 29, 2026

## Critical Discovery

v2.1.4 showed touches working but coordinates still wrong:
- Top-left touch: `raw(259,1550)` - LOW first value, MID second value
- Top-right touch: `raw(4127,4871)` - MID first value, HIGH second value

This pattern suggests **X and Y are read from swapped bytes**!

### This Version Tests
**Swapped byte positions:**
- X coordinate: NOW reading bytes 28-29 (was 24-25)
- Y coordinate: NOW reading bytes 24-25 (was 28-29)

**Updated ranges:**
- X: 0-5400 (was 250-8500)
- Y: 250-8500 (was 0-5400)

### Installation
```bash
tar -xzf nwfermi-dkms-2.1.5.tar.gz
cd nwfermi-dkms-2.1.5
sudo dkms remove nwfermi/2.1.4 --all
sudo cp -r . /usr/src/nwfermi-2.1.5/
sudo dkms install nwfermi/2.1.5
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

### Expected Result
With swapped byte reading, touches should now map correctly:
- Top-left →  Top-left
- Top-right → Top-right
- Bottom-left → Bottom-left
- Bottom-right → Bottom-right

If this works, GNOME should also start responding properly!
