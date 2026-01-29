# v2.0.9 Changelog

**Date:** January 28, 2026

## Adjusted Validation Bounds

### Problem in v2.0.8
Testing showed valid touches being filtered:
```
Touch: raw(3864,3844) ✓ Valid
Filtered: raw(3869,5382) ✗ Y just over 5000 limit - FALSE POSITIVE!
```

The Y range was too restrictive (0-5000), missing valid touches near screen edges.

### The Fix
Expanded validation bounds based on observed valid touches:

**OLD (v2.0.8):**
```c
X: 200-9000
Y: 0-5000
```

**NEW (v2.0.9):**
```c
X: 250-9000  (tightened low end - values <250 are garbage)
Y: 0-6000    (expanded - captures full Y range up to 5400)
```

Still blocks all 60000+ garbage values while allowing ALL valid edge touches.

## Installation

```bash
tar -xzf nwfermi-dkms-2.0.9.tar.gz
cd nwfermi-dkms-2.0.9
sudo dkms remove nwfermi/2.0.8 --all
sudo cp -r . /usr/src/nwfermi-2.0.9/
sudo dkms install nwfermi/2.0.9
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

## Expected Result

ALL screen areas should now respond to touch, including:
- Top edge (Y near 0)
- Bottom edge (Y near 5400)
- Left edge (X near 250)
- Right edge (X near 8500)

No more false positives filtering valid edge touches.
