# minecraft-carve
Extract minecraft files from filesystem images

## Design Scope
The purpose of this project is to recovered deleted (that is, unlinked) minecraft saved game data.  My personal interest is in an ext4 filesystem.  My initial goal is to recover the .mca and .mcr files, which are in the Anvil and Region file formats respectively.  These make up the bulk of the saved game data.  I hope to recover the content of the files, to verify their integrity, and to deduce their proper filenames.

## Design Limitations

### Filesystem structure and block size
The basic assumption for the filesystem structure is that file data is stored in 4096-byte aligned blocks.  Therefore this tool may also be useful in scenarios involving filesystems other than ext2/3/4.  But whatever the filesystem, this file carving does not attempt to cope with file data that stored in fragmented blocks that are smaller than 4096 bytes.  For convenience, the tool may optionally use libext2fs to read the ext2/3/4 block bitmaps and ignore data blocks that are currently in use.

Note: If we were to adapt this tool for use with a filesystem image having (for example) 1024-byte blocks, then the main requirement would be to alter the scanning to cope with files aligned at 1 KiB boundaries.

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

The type and location of each block should be recorded.

#### Recognizing chunk data offsets
Since the region files contains at most 2^10 chunks, and each chunk has a maximum possible length of (2^8 - 1) 4KiB units, then the maximum possible offset can be no greater than 2^18.  Thus, the upper 4 bits of the 24-bit encoded chunk offsets must always be zero.  This hard constraint can be used to scan and rapidly reject candidate blocks.

Furthermore, the upper byte of the chunk offsets will only be nonzero for Anvil files that are greater than 4 KiB * 2^16 = 256 MiB in size.  If we assume that our Anvil files are less than 512 MiB, we can scan simply by requiring that this upper byte is zero or one.

Once we have a candidate block, we can sort the chunks (offset, length) by offsets and require the lengths to be no greater than the difference between successive offsets.  It is not unusual that the length
is less than the difference between offsets.  In such cases, the region file may contain leftover, unused pieces of chunk data.

We may want an additional round of rejection for extremely sparse files, for example, an apparently legitimate offset block that describes only a single chunk.

Finally, we should record a bitmap of chunks that are present in the file.  This constitutes a 128-byte fingerprint of the anvil file which can be used for reuniting file fragments.

Note that this fingerprint is not useful for highly populated regions, in which all chunks are present.  Furthermore, these regions are quite common in actual practice.  Fortunately, region file fragmentation occurring within the header is probably rare.

#### Recognizing chunk timestamps
We will simply require each uint32_t to fall within a start_time and stop_time range.  The defaults can be Jan-1-2009 and the present date (in the unix epoch).

Then record a bitmap of chunks that are present, in order to match up with the chunk data offsets.

#### Recognizing the first block of chunk data

The maximum length of a chunk data segment is 1 MiB = 2^20 bytes, so the uint32_t which encodes its length must have the upper 12 bits equal to zero.  So the first byte of a chunk data segment should be zero, the second byte should be less than 2^5 = 32, the fifth byte should almost always be equal to 2 (compression type = zlib).  Referring to RFC 1950, the sixth and seventh bytes, CMF and FLG, should have (CMF * 256 + FLG) divisible by 31.  And most likely, we will have CMF=0x78 FLG=0x9c.

Finally, we can attempt to decompress the data segment starting with the sixth byte, and confirm that encodes NBT data.

#### Recognizing other blocks of chunk data

Recognizing other chunk data may be rather challenging in the case of fragmentation.  We can attempt to continue decompressing blocks after the first block, up to the number of blocks corresponding to the length of the data segment indicated, and mark those blocks where it was successful.  In practice, many chunks will consist of only one block.  We can also try to match incomplete chunk data with unallocated blocks, which will have N*M possible matchups.  If we have 2 GiB of free space on the filesystem, then that leaves 500k potential blocks.  Matching them with 1k incomplete chunk data would require 500M potential matchups, which may be practical but expensive to try.

### Pass 2:
Match up blocks to construct chains of blocks.  Ideally, chains will form complete anvil files.  It is reasonable to expect that a fragmented anvil file can be usually reconstructed from two or three chains, assuming that unlinked file data has not been reallocated and overwritten.

#### Method
We read back the type and location of the candidate blocks recorded from Pass 1.  Timestamps and offsets should be matched one-to-one.

Unfragmented blocks can be formed into chains.  Chunk data can be grouped based on which actual (x,z) region it belongs to.  A question is whether we should try to reconstruct file extents.
If we do, it might even help with locating fragmented chunk data.

### Pass 3:
Write out complete anvil files.

### Pass 4:
Group related anvil files together.

Heuristics may take into account:
- timestamps
- NBT version numbers

There should be other information in the chunks that can be used.

## Further Requirements
The .mca files may not be useful without the matching level.dat, most importantly because this contains the world's seed.  Recovering deleted level.dat is not too difficult, but we also need to match it up.  level.dat records both the in-game time in ticks, and LastPlayed, the clock time.  These could be matched up with the chunk's LastUpdate (time in ticks) and with the Anvil timestamps.
