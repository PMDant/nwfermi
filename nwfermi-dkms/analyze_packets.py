#!/usr/bin/env python3
"""
NextWindow Fermi USB Packet Analyzer

This tool helps debug the touchscreen by capturing and analyzing USB packets.
Run this to understand what your specific device is sending.
"""

import sys
import struct
import argparse
from datetime import datetime

def parse_packet(data):
    """Parse a NextWindow Fermi packet and print analysis"""
    if len(data) < 4:
        return None
    
    # Check header
    if data[0] != 0x5B or data[1] != 0x5D:
        return None
    
    packet_info = {
        'header': f"{data[0]:02x} {data[1]:02x}",
        'seq': data[2],
        'type': data[3],
        'length': len(data),
        'hex': ' '.join(f"{b:02x}" for b in data[:min(32, len(data))])
    }
    
    return packet_info

def analyze_usb_log(filename):
    """Analyze a usbmon capture file"""
    print(f"Analyzing {filename}...\n")
    
    with open(filename, 'r') as f:
        for line in f:
            # Parse usbmon format
            # Example: ffff8d01c3575c00 972820965 C Bi:4:003:1 0 2471 = 5b5d2209 9f020201...
            if ' = ' not in line:
                continue
            
            parts = line.split(' = ')
            if len(parts) != 2:
                continue
            
            meta = parts[0].split()
            if len(meta) < 7:
                continue
            
            # Extract packet data
            hex_data = parts[1].strip().split()
            
            # Convert hex bytes to binary
            packet_bytes = []
            for hex_word in hex_data:
                # Each word is 4 bytes (8 hex chars)
                for i in range(0, len(hex_word), 2):
                    if i + 1 < len(hex_word):
                        packet_bytes.append(int(hex_word[i:i+2], 16))
            
            if len(packet_bytes) < 4:
                continue
            
            # Analyze packet
            info = parse_packet(packet_bytes)
            if info:
                print(f"Seq {info['seq']:3d} Type 0x{info['type']:02x} Len {info['length']:4d}: {info['hex']}")
                
                # Detailed analysis for touch packets
                if info['type'] in [0xD6, 0xD7] and len(packet_bytes) >= 20:
                    print(f"  Touch packet analysis:")
                    print(f"    Bytes 4-7:  {' '.join(f'{packet_bytes[i]:02x}' for i in range(4, min(8, len(packet_bytes))))}")
                    print(f"    Bytes 18+:  {' '.join(f'{packet_bytes[i]:02x}' for i in range(18, min(30, len(packet_bytes))))}")
                    
                    # Look for touch data patterns
                    for i in range(18, len(packet_bytes) - 2):
                        byte = packet_bytes[i]
                        if byte & 0x80:  # Possible touch indicator
                            dx = packet_bytes[i+1]
                            dy = packet_bytes[i+2] if i+2 < len(packet_bytes) else 0
                            
                            # Convert to signed
                            dx_signed = dx if dx < 128 else dx - 256
                            dy_signed = dy if dy < 128 else dy - 256
                            
                            slot = byte & 0x0F
                            print(f"    Offset {i:2d}: touch_byte=0x{byte:02x} slot={slot} dx={dx_signed:+4d} dy={dy_signed:+4d}")
                
                print()

def hex_dump(filename):
    """Simple hex dump of a binary file"""
    with open(filename, 'rb') as f:
        data = f.read()
    
    print(f"Hex dump of {filename} ({len(data)} bytes):\n")
    
    for i in range(0, len(data), 16):
        # Offset
        print(f"{i:08x}  ", end='')
        
        # Hex bytes
        for j in range(16):
            if i + j < len(data):
                print(f"{data[i+j]:02x} ", end='')
            else:
                print("   ", end='')
            
            if j == 7:
                print(" ", end='')
        
        # ASCII representation
        print(" |", end='')
        for j in range(16):
            if i + j < len(data):
                byte = data[i + j]
                if 32 <= byte < 127:
                    print(chr(byte), end='')
                else:
                    print('.', end='')
        print("|")

def main():
    parser = argparse.ArgumentParser(
        description='Analyze NextWindow Fermi USB packets',
        epilog='''
Examples:
  # Analyze a usbmon capture
  %(prog)s usb_capture.log
  
  # Hex dump a binary file
  %(prog)s --hex-dump raw_data.bin
        '''
    )
    parser.add_argument('file', help='File to analyze')
    parser.add_argument('--hex-dump', action='store_true',
                       help='Show hex dump instead of parsing')
    
    args = parser.parse_args()
    
    try:
        if args.hex_dump:
            hex_dump(args.file)
        else:
            analyze_usb_log(args.file)
    except FileNotFoundError:
        print(f"Error: File '{args.file}' not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
