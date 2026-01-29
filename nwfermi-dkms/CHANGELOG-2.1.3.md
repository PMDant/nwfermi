# v2.1.3 - Testing X Normal / Y Inverted

**Date:** January 28, 2026

## Coordinate Axis Test

v2.1.2 showed touches working but coordinates were off:
- Top-left touch appeared 1/4 down, 1/10 right
- Suggests touchscreen panel might be oriented differently

### This Version Tests
**X: NORMAL** (raw 250→screen 0, raw 8500→screen 1366)
**Y: INVERTED** (raw 0→screen 768, raw 5400→screen 0)

Previous version had X inverted, Y normal. This swaps them.

### Installation
```bash
tar -xzf nwfermi-dkms-2.1.3.tar.gz
cd nwfermi-dkms-2.1.3
sudo dkms remove nwfermi/2.1.2 --all
sudo cp -r . /usr/src/nwfermi-2.1.3/
sudo dkms install nwfermi/2.1.3
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

### Test
Touch all 4 corners and check if they match:
- Top-left → Top-left?
- Top-right → Top-right?
- Bottom-left → Bottom-left?  
- Bottom-right → Bottom-right?

If still wrong, we may need to test NO inversion or BOTH axes inverted.
