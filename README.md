# minecraft-carve
Extract minecraft files from filesystem images

## Design Scope
The purpose of this project is to recovered deleted (that is, unlinked) minecraft saved game data.  The filesystem in question is an ext4 filesystem.  My initial goal is to recover the .mca and .mcr files, which are in the Anvil and Region file formats respectively.  These make up the bulk of the saved game data.  I hope to recover the content of the files, to verify their integrity, and to deduce their proper filenames.

## Design Limitations

### Filesystem structure and block size
The only assumption for the filesystem structure is that file data is stored in 4096-byte aligned blocks.  Therefore this tool may also be useful in scenarios involving filesystems other than ext2/3/4.  But whatever the filesystem, this file carving does not attempt to cope with file data that stored in fragmented blocks that are smaller than 4096 bytes.  For convenience, the tool may optionally use libext2fs to read the ext2/3/4 block bitmaps and ignore data blocks that are currently in use.

### File metadata
This tool will not attempt to recover file metadata from the filesystem.  Information about the files will have to come from their contents only.

### Minecraft versions
I am attempting to recover Minecraft saved data from versions 1.12 to 1.16.  The Anvil file format and Chunk data format is fairly stable between Minecraft versions, so earlier and later versions may work fine, but they are not an objective of my testing.

## Notes on the Anvil file format
Each anvil file stores the data for one region.  Each region is divided into 1024 chunks, arranged in a 32 x 32 X-Z grid.  A chunk may be present or not present in the data file.  Each chunk has dimension 16 x 256 x 16, therefore the entire region has dimension 512 x 512 in X-Z.

Values in the Anvil file are big-endian.

The overall structure of the Anvil file is:

Offset (bytes) | Length (bytes) | Data
| :--- | :--- | :---:
0  |  4096 | uint32_t chunk_offset[1024]
4096 | 4096 | uint32_t chunk_timestamp[1024]
(2 + offset[i]) * 4096 | length[i] * 4096 | char *chunk_data[i]

The high 24 bits of a chunk_offset[i] represent, in units of 4KiB, the offset of the chunk_data[i] within the region file.  If zero, the chunk data is not present.

The low 8 bits of a chunk_offset[i] represent, in units of 4KiB, the length of the chunk_data[i] within the region file.

The chunk_data[i] begins with a uint32_t representing the length of data in bytes (not including these initial four bytes).
The next byte describes the enocoding type, and is one of {1, 2, 3}.
The remaining bytes are zlib-compressed data which decodes to an NBT structure describing the chunk.

The NBT structure for a chunk contains a DataVersion tag describing the version of the NBT structure.  This should be useful in determining the version of the Minecraft instance that generated the saved world.

The NBT structure also contains xPos and zPos values for the chunk as measured from the origin.  This should make it possible to identify the proper name of the region file that the chunk belongs to.  It should also determine which chunk the structure belongs to within the region file (useful in the case of file fragmentation).

The NBT structure contains biome information that should help us to determing if the region file belongs to the overworld, the nether, or the end.

## Strategy
The carving and reconstruction will be carried out in multiple passes.  Here is a rough outline of what the passes should accomplish:

### Pass 1:
Identify candidate blocks from anvil files.  There are four types:
1. The chunk data offsets block
2. The chunk data timestamps block
3. The first block of chunk data
4. Any other blocks of chunk data

