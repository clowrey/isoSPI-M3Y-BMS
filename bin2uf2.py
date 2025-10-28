#!/usr/bin/env python3
"""
Simple script to convert .bin to .uf2 for RP2040/RP2350
Based on Microsoft's UF2 format specification
"""

import struct
import sys

# UF2 format constants
UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157  # Randomly selected
UF2_MAGIC_END = 0x0AB16F30     # Randomly selected

UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000

# RP2040/RP2350 family ID
RP2040_FAMILY_ID = 0xe48bff56

# Flash address for RP2040/RP2350
FLASH_START = 0x10000000

# UF2 block size
BLOCK_SIZE = 256

def create_uf2_block(block_no, num_blocks, addr, data):
    """Create a single UF2 block"""
    assert len(data) <= BLOCK_SIZE
    
    # Pad data to 256 bytes
    data = data + b'\x00' * (BLOCK_SIZE - len(data))
    
    # UF2 block structure:
    # 0-3: Magic Start 0
    # 4-7: Magic Start 1
    # 8-11: Flags
    # 12-15: Target address
    # 16-19: Payload size
    # 20-23: Block number
    # 24-27: Total blocks
    # 28-31: Family ID
    # 32-287: Data (256 bytes)
    # 476-479: Magic End
    
    block = struct.pack('<IIIIIIII',
        UF2_MAGIC_START0,
        UF2_MAGIC_START1,
        UF2_FLAG_FAMILY_ID_PRESENT,
        addr,
        BLOCK_SIZE,
        block_no,
        num_blocks,
        RP2040_FAMILY_ID
    )
    
    block += data
    # UF2 block is 512 bytes total
    # We have 32 bytes header + 256 bytes data + padding + 4 bytes end magic
    # 512 - 32 - 256 - 4 = 220 bytes padding
    block += b'\x00' * 220
    block += struct.pack('<I', UF2_MAGIC_END)
    
    assert len(block) == 512, f"Block size is {len(block)}, expected 512"
    return block

def bin_to_uf2(bin_path, uf2_path):
    """Convert binary file to UF2 format"""
    # Read binary file
    with open(bin_path, 'rb') as f:
        bin_data = f.read()
    
    # Calculate number of blocks
    num_blocks = (len(bin_data) + BLOCK_SIZE - 1) // BLOCK_SIZE
    
    print(f"Converting {bin_path} to {uf2_path}")
    print(f"Binary size: {len(bin_data)} bytes")
    print(f"Number of blocks: {num_blocks}")
    
    # Create UF2 blocks
    uf2_data = b''
    for i in range(num_blocks):
        offset = i * BLOCK_SIZE
        chunk = bin_data[offset:offset + BLOCK_SIZE]
        addr = FLASH_START + offset
        
        block = create_uf2_block(i, num_blocks, addr, chunk)
        uf2_data += block
    
    # Write UF2 file
    with open(uf2_path, 'wb') as f:
        f.write(uf2_data)
    
    print(f"✓ Created {uf2_path} ({len(uf2_data)} bytes)")
    print(f"\nTo flash:")
    print(f"  1. Hold BOOTSEL button while connecting USB")
    print(f"  2. Copy {uf2_path} to the RPI-RP2 drive")
    print(f"  3. Device will reboot automatically")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.uf2>")
        sys.exit(1)
    
    bin_path = sys.argv[1]
    uf2_path = sys.argv[2]
    
    bin_to_uf2(bin_path, uf2_path)

