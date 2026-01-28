# Installation Guide - NextWindow Fermi v2.0.5

## 🎉 This version has REAL coordinate tracking!

Your touchscreen will work immediately after installation with accurate position tracking across the entire screen.

## Prerequisites

You need:
- Linux kernel headers for your running kernel
- DKMS (Dynamic Kernel Module Support)
- Basic build tools

### Install Prerequisites

**Arch Linux:**
```bash
sudo pacman -S linux-headers dkms base-devel
```

**Ubuntu/Debian:**
```bash
sudo apt install dkms build-essential linux-headers-$(uname -r)
```

**Fedora:**
```bash
sudo dnf install dkms kernel-devel kernel-headers gcc make
```

## Installation Steps

### 1. Extract the Archive

```bash
tar -xzf nwfermi-dkms-2.0.5.tar.gz
cd nwfermi-dkms-2.0.5
```

### 2. Run the Installation Script

```bash
sudo ./install.sh
```

The script will:
- Copy driver files to `/usr/src/nwfermi-2.0.5/`
- Build the kernel module via DKMS
- Load the module automatically
- Create udev rules for device access

### 3. Verify Installation

```bash
# Check module is loaded
lsmod | grep nwfermi

# Should show: nwfermi  16384  0

# Check device is recognized
dmesg | grep nwfermi | tail -10

# Should show driver initialization and device detection
```

### 4. Test the Touchscreen

Touch anywhere on the screen - it should work immediately!

Open an app like GIMP, Firefox, or Files and interact with the touchscreen.

## Upgrading from v2.0.4

If you have v2.0.4 installed:

```bash
# Uninstall old version
cd /path/to/nwfermi-dkms-2.0.4
sudo ./uninstall.sh

# Install new version
cd /path/to/nwfermi-dkms-2.0.5
sudo ./install.sh
```

## Configuration

### Screen Resolution

**Default:** 1366x768

If your screen has a different resolution, edit `nwfermi.c` before installing:

```bash
nano nwfermi.c
```

Find these lines (around line 57):
```c
#define SCREEN_WIDTH           1366
#define SCREEN_HEIGHT          768
```

Change to your resolution and save. Then run `sudo ./install.sh`.

### Common Resolutions

- **1920x1080 (Full HD):** Change to 1920, 1080
- **2560x1440 (QHD):** Change to 2560, 1440  
- **1280x800:** Change to 1280, 800
- **1024x768:** Change to 1024, 768

## Troubleshooting Installation

### "Module not found" errors

Make sure kernel headers are installed:
```bash
ls /lib/modules/$(uname -r)/build
```

If this directory doesn't exist, install headers as shown in Prerequisites.

### Build failures on Arch Linux 6.18.6+

v2.0.5 includes fixes for kernel 6.18.6. If you still have issues:

1. Check you're using the v2.0.5 source (not v2.0.4)
2. Look for specific error messages in the build output
3. Verify DKMS is up to date: `sudo pacman -S dkms`

### Permission denied errors

Make sure to run with `sudo`:
```bash
sudo ./install.sh
```

### Device not detected after installation

1. Unplug and replug the USB device
2. Check USB connection: `lsusb | grep -i next`
3. Check dmesg for errors: `sudo dmesg | tail -30`

## Uninstallation

To remove the driver:

```bash
cd nwfermi-dkms-2.0.5
sudo ./uninstall.sh
```

This will:
- Unload the module
- Remove it from DKMS
- Delete installed files
- Remove udev rules

The touchscreen will stop working until you reinstall or revert to another driver.

## Persistence Across Reboots

The driver automatically:
- Loads on boot via udev rules
- Rebuilds when kernel is updated (via DKMS)
- Detects the device when plugged in

No additional configuration needed!

## Getting Debug Information

If something doesn't work, collect this information:

```bash
# System info
uname -a
lsb_release -a

# Module status
lsmod | grep nwfermi
dkms status

# Device detection
lsusb | grep -i next
ls -la /dev/input/by-id/ | grep -i fermi

# Recent messages
sudo dmesg | grep -E "(nwfermi|USB)" | tail -50
```

## Next Steps

After installation:
- Read `TESTING.md` for calibration and configuration
- Check `CHANGELOG_2.0.5.md` for what's new
- See `README.md` for technical details

Enjoy your working touchscreen!

---

**Having issues?** Check TESTING.md for troubleshooting steps.
