# v2.1.2 Changelog

**Date:** January 28, 2026

## Enhanced Touch Event Debugging

### Changes
- Added explicit "TOUCH DOWN" and "TOUCH UP" logging to see complete touch lifecycle
- Fixed BTN_TOUCH=0 release to only fire when touches were actually active
- Changed touch/release logging from dev_dbg to dev_info for visibility

### Purpose
This version helps diagnose if touches are being properly detected and released:
- "TOUCH DOWN: slot=0" = Touch started
- "TOUCH UP: slot=0" = Touch ended
- BTN_TOUCH toggle should trigger clicks

### Installation

```bash
tar -xzf nwfermi-dkms-2.1.2.tar.gz
cd nwfermi-dkms-2.1.2
sudo dkms remove nwfermi/2.1.1 --all
sudo cp -r . /usr/src/nwfermi-2.1.2/
sudo dkms install nwfermi/2.1.2
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

### Debug
Check dmesg for TOUCH DOWN/UP pairs:
```bash
sudo dmesg -w | grep -E "(TOUCH DOWN|TOUCH UP)"
```

Each tap should show:
```
TOUCH DOWN: slot=0
TOUCH UP: slot=0
```

If you see DOWN without UP, the touch isn't being released properly.
If you see both but clicks still don't work, the issue is in GNOME's touch handling.
