# NextWindow Fermi Driver - Debugging Guide

## The Problem We're Solving

The original NextWindow driver initialized the USB device but didn't decode the touch packets - it relied on a userspace daemon to do the decoding. We've reverse-engineered the packet format from USB traces, but your specific device might send slightly different packets.

## Step-by-Step Debugging

### Step 1: Verify Device Detection

```bash
# Check if device is plugged in
lsusb | grep -i 1926

# You should see something like:
# Bus 004 Device 003: ID 1926:1846 NextWindow Fermi Touchscreen
```

### Step 2: Load the Driver with Debugging

```bash
# Unload any existing drivers
sudo rmmod nwfermi 2>/dev/null || true

# Load our driver  
sudo modprobe nwfermi

# Enable debug output
sudo bash -c 'echo 8 > /proc/sys/kernel/printk'
sudo bash -c 'echo "module nwfermi +p" > /sys/kernel/debug/dynamic_debug/control'

# Watch kernel messages in real-time
sudo dmesg -w | grep -i fermi
```

You should see:
```
NextWindow Fermi touchscreen probing...
Bulk-in endpoint: 0x81
Started reading touch data
NextWindow Fermi touchscreen registered successfully
```

### Step 3: Check if Packets Are Arriving

When you touch the screen, you should see debug messages like:
```
Packet: len=478 type=0xd6 [5b 5d 23 d6...]
```

If you DON'T see these messages, the problem is USB communication - the driver isn't receiving data.

If you DO see these messages, the problem is packet parsing - we're receiving data but not correctly interpreting it.

### Step 4: Test Touch Input

```bash
# Install evtest
sudo apt-get install evtest

# List devices
sudo evtest

# Look for "Nextwindow Fermi Touchscreen" in the list
# Note its event number (e.g., /dev/input/event5)

# Monitor events from that device
sudo evtest /dev/input/event5
```

Touch the screen. You should see:
```
Event: time 1234567890.123456, type 3 (EV_ABS), code 47 (ABS_MT_SLOT), value 0
Event: time 1234567890.123456, type 3 (EV_ABS), code 57 (ABS_MT_TRACKING_ID), value 1
Event: time 1234567890.123456, type 3 (EV_ABS), code 53 (ABS_MT_POSITION_X), value 12345
Event: time 1234567890.123456, type 3 (EV_ABS), code 54 (ABS_MT_POSITION_Y), value 23456
Event: time 1234567890.123456, -------------- SYN_REPORT ------------
```

### Step 5: Capture Raw USB Traffic

If events aren't working, we need to see what the device is actually sending:

```bash
# Enable USB monitoring
sudo modprobe usbmon

# Find your bus number
lsusb | grep 1926
# Example output: Bus 004 Device 003: ID 1926:1846

# Capture traffic from bus 4 (adjust number as needed)
sudo cat /sys/kernel/debug/usb/usbmon/4u > /tmp/usb_capture.log &
CAPTURE_PID=$!

# Touch the screen a few times
# Do different gestures: tap, drag, multitouch

# Stop capture
sudo kill $CAPTURE_PID

# Analyze the capture
./analyze_packets.py /tmp/usb_capture.log
```

## Common Problems and Solutions

### Problem: No packets arriving

**Symptoms**: Driver loads but no "Packet: len=..." messages when touching

**Solutions**:
1. Check if another driver claimed the device:
   ```bash
   ls -la /sys/bus/usb/devices/*/driver | grep 1926
   ```

2. Try unbinding other drivers:
   ```bash
   # Find the device path
   USB_PATH=$(ls -d /sys/bus/usb/devices/*-* | grep -l "1926:1846" | head -n1)
   
   # Unbind if something else has it
   echo $USB_PATH | sudo tee /sys/bus/usb/drivers/*/unbind
   
   # Reload our driver
   sudo rmmod nwfermi && sudo modprobe nwfermi
   ```

### Problem: Packets arriving but no touch events

**Symptoms**: You see "Packet: len=..." but evtest shows nothing

**Cause**: Packet parsing is wrong for your device

**Solution**: We need to analyze your specific packet format

1. Capture USB traffic (see Step 5)
2. Run the analyzer: `./analyze_packets.py /tmp/usb_capture.log`
3. Look at the packet analysis output
4. We may need to adjust the packet parsing code

### Problem: Touch events but wrong coordinates

**Symptoms**: Events show but coordinates are way off or inverted

**Solutions**:
1. The device might use absolute coordinates instead of deltas
2. The coordinate encoding might be different
3. We need to see your packet data to fix this

### Problem: Touch works but erratic/jumpy

**Cause**: Delta encoding accumulation errors

**Solutions**:
1. Device might send absolute coordinates in some packets we're not parsing
2. We need periodic absolute coordinate "sync" packets
3. May need calibration

## Packet Format Analysis

From USB traces, we know:

```
Standard packet:
[5B 5D] - Header
[XX] - Sequence number
[D6/D7] - Packet type
[... metadata ...]
[... touch data starts around byte 18 ...]
```

Touch data format (best guess):
```
[touch_byte] [dx] [dy] [more...]

touch_byte:
  - Bit 7 (0x80): Touch active flag
  - Bits 0-3: Slot number

dx, dy: Signed 8-bit delta values
  - 0x00 = no movement
  - 0x01-0x7F = positive delta (1 to 127)
  - 0x80-0xFF = negative delta (-128 to -1)
```

But this is just a hypothesis! Your device might be different.

## How to Help Improve the Driver

If your device isn't working:

1. Run through all debugging steps above
2. Capture USB traffic while doing specific gestures:
   - Single tap
   - Single finger drag
   - Two finger tap
   - Two finger drag
3. Share:
   - Your device ID from `lsusb`
   - Kernel debug output from `dmesg`
   - USB capture analyzed with `analyze_packets.py`
   - Description of what you were doing

## Advanced: Comparing with Old Daemon

If you still have the old daemon working:

1. Capture USB traffic with old daemon:
   ```bash
   sudo cat /sys/kernel/debug/usb/usbmon/4u > /tmp/usb_old.log &
   PID=$!
   # Use touchscreen
   sudo kill $PID
   ```

2. Capture with new driver:
   ```bash
   sudo cat /sys/kernel/debug/usb/usbmon/4u > /tmp/usb_new.log &
   PID=$!
   # Use touchscreen
   sudo kill $PID
   ```

3. Compare the two:
   ```bash
   ./analyze_packets.py /tmp/usb_old.log > old_analysis.txt
   ./analyze_packets.py /tmp/usb_new.log > new_analysis.txt
   diff -u old_analysis.txt new_analysis.txt
   ```

## Next Steps if Driver Doesn't Work

The driver is designed to be debuggable. If it doesn't work out of the box:

1. The debug output will tell us if packets are arriving
2. The USB capture will show us the exact format your device uses
3. We can then modify the `fermi_parse_touch_packet()` function in `nwfermi.c`
4. Rebuild and test: `sudo dkms rebuild nwfermi/2.0.2 && sudo modprobe -r nwfermi && sudo modprobe nwfermi`

The packet parsing code in the driver is clearly commented and designed to be modified once we understand your device's specific format.
