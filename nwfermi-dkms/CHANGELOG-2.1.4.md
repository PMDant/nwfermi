# v2.1.4 - BOTH Axes Inverted (180° Rotation)

**Date:** January 29, 2026

## Final Coordinate Fix

v2.1.3 testing revealed the touchscreen is mounted **upside-down** (rotated 180° relative to display):
- Top-left touch → appeared bottom-left
- Top-right touch → appeared near top-middle  
- Pattern shows touchscreen physically rotated 180°

### This Version
**BOTH X AND Y INVERTED:**
- X: raw 250 → screen 1366 (right), raw 8500 → screen 0 (left)
- Y: raw 0 → screen 768 (bottom), raw 5400 → screen 0 (top)

This should make touches appear exactly where you touch!

### Installation
```bash
tar -xzf nwfermi-dkms-2.1.4.tar.gz
cd nwfermi-dkms-2.1.4
sudo dkms remove nwfermi/2.1.3 --all
sudo cp -r . /usr/src/nwfermi-2.1.4/
sudo dkms install nwfermi/2.1.4
sudo modprobe -r nwfermi && sudo modprobe nwfermi
```

### Expected Result
Perfect 1:1 touch tracking:
- Top-left → Top-left ✓
- Top-right → Top-right ✓
- Bottom-left → Bottom-left ✓
- Bottom-right → Bottom-right ✓
- Center → Center ✓

**This should be the final working version!** 🎉
