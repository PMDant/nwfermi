# v2.0.4 - Length-Based Touch Detection

## Key Discovery
The breakthrough! Touch events correlate with **packet length changes**, not type changes:
- **Idle packets**: 478-486 bytes (cycling pattern)
- **Touch packets**: 488-493 bytes (8-15 bytes longer)

## Changes
1. **Removed D6/D7 packet type detection** - those types don't exist!
2. **Implemented length-based touch detection**:
   - Packets < 487 bytes = idle (suppress logging)
   - Packets ≥ 488 bytes = touch event
3. **Added diagnostic logging**:
   - Hex dump of touch packets
   - Extract 16-bit values from multiple offsets
   - Statistics tracking (idle/touch packet counts)
4. **Temporary test output**: Generates touch at screen center when touch detected

## Next Steps
Once we see the hex dumps from actual touches, we can decode:
- Which bytes contain X coordinate
- Which bytes contain Y coordinate  
- Coordinate encoding format (16-bit big/little endian, etc.)

## Testing
```bash
sudo ./install.sh
# Touch the screen and check dmesg for hex dumps
dmesg | grep -A 20 "TOUCH PACKET"
```
