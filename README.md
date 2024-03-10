An implementation of a custom file system based on FAT.

## System Area
The system area consists of the following sections:

### Block 0
System Volume Name (63 bytes + 1 byte for null terminator)

### Block BLOCK 1 -> n
(Where n is `ceil(5 + File_allocation_table_size) / BLOCK_SIZE`)

- File System Identifier (in this version, it is 'WFS' - 3 bytes)
- Root Directory Size (2 bytes)
  - Encoded in base 256 - can store between 0 -> 65535 (inclusive)
- File Allocation Table (`2*NUM_BLOCKS bytes`)
  - The file allocation table is a series of base-256 encoded byte pairs with each pair representing the next block in a linked list that forms a file. As these are contiguous in storage, corresponding block values are implicitly defined. (I.e. first byte pair corresponds to block 0's status, etc.).
  - Possible values are:
    - Unallocated (Int value of 65534, hex ff fe)
    - Allocated, End Of File (Int value of 65535, hex, ff ff)
    - Allocated, Next Block Address (Int value of 0 -> 65533 inclusive, hex 00, 00 -> ff, fd)
  - As this section stores the status for each block, it naturally grows with block size (File allocation table size will be `2*NUM_BLOCKS` bytes long as it requires 2 bytes per block)
  - System area sections (except for the root block) are not accessed through the file allocation table and thus are padded with the End_of_file value to prevent the system from allocating these blocks after formatting. (They are accessed through direct block/byte offset calculations when changes are required.)

![image-808e3b24-4cf9-47b7-b2bc-a2394be7d2d2](https://github.com/wjin-lee/custom-filesystem/assets/100455176/add26fd9-6e3f-4de2-b1af-957776cfd6e0)

### BLOCK n+1
- This block holds the root directory's metadata and is positioned just after the file allocation table.
  - The block index can be calculated by: `1 + (ceil(5 + File_allocation_table_size) / BLOCK_SIZE)`
    - I.e. 1 + ((5 + 2*NUM_BLOCKS + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

- The root metadata block functions like any other directory metadata section and will grow as needed. (12 bytes per 'entry' - i.e. per file or directory stored immediately within the directory. Does not include grandchildren entries nor their sizes.)
  - It does differ from the usual directories in two ways:
    - Its position is always known given the number of blocks allocated and block size are known.
    - The byte size of the directory metadata section, stored in base 256 encoded string, always exists at a fixed place - byte positions 3, and 4 (0-indexed) of block 1 (0-indexed).
  - Each entry represents a child file or directory and are made up of the following columns in mentioned order:
    - File/Directory Name (7 bytes)
    - Type of object (Either 'F' for file or 'D' for directory - 1 byte)
    - Start block index - base 256 encoded (2 bytes)
    - Filesize in bytes - base 256 encoded (2 bytes)
      - The meaning of this differs slightly depending on the type of object being stored.
        - For files, this refers to the filesize of the object.
        - For directories, this refers to the size of their directory metadata section. NOT including their nested file/directory
          - Nested directory sizes are calculated on-demand only for requested sections (Notably in the list() function)
            
![image-97e01cb9-0ecc-4e8b-8fb1-5992bcaabb07](https://github.com/wjin-lee/custom-filesystem/assets/100455176/8503f7ce-21d5-4d87-8d84-fc553743b64f)

## Directory Handling

A directory is essentially a file with a special format - the directory metadata section (DMS)  format. Excluding the root directory which has a couple differences mentioned above, it is formatted as follows:
- Each entry represents a child file or directory and are made up of the following columns in mentioned order:
  - File/Directory Name (7 bytes)
  - Type of object (Either 'F' for file or 'D' for directory - 1 byte
  - Start block index - base 256 encoded (2 bytes)
  - Filesize in bytes - base 256 encoded (2 bytes)
    - The meaning of this differs slightly depending on the type of object being stored
      - For files, this refers to the filesize of the object.
      - For directories, this refers to the size of their directory metadata section. NOT including their nested file/directory sizes.
        - Nested directory sizes are calculated on-demand only for requested sections (Notably in the list() function)![image-a211b988-41db-4a7c-8936-39088825ffc7](https://github.com/wjin-lee/custom-filesystem/assets/100455176/e0c42148-4029-4c4c-973c-2a014dfc29de)

- Each entry - either file or directory therefore consumes 12 bytes.
- Just like a file, a DMS can grow to occupy multiple non-contiguous blocks with relationships between the multiple blocks maintained through the file allocation table as a linked list.
- Just like a file, each directory's metadata section must be defined in a new block.
  - In the screenshot, Block 2 holds the root directory's MDS with it having 1 entry - the dir1, which is labeled a directory (D) with a start block index of 00 03 (int 3) and size of 00 0c (int 12)
  - This makes sense if we look at block 3 which we know is dir1's MDS which holds another entry for dir2 totalling 12 bytes long.
    - Note how it only counts the length of dir1's MDS only. NOT any nested structures or files such as fileA's MDS entry under dir2.

![image-a211b988-41db-4a7c-8936-39088825ffc7](https://github.com/wjin-lee/custom-filesystem/assets/100455176/8b69629a-ca2d-4bc5-88e0-53fa6537fd44)

- To find the starting block of a specific directory's DMS, one must traverse the directory structure starting from the root.
  - E.g. Going to /dir1/dir2/fileA may look like:
    - Traverse to root given fixed starting index for DMS and size counters.
    - Look in root's DMS to find 'dir1' and traverse to dir2's DMS start block.
    - (given that dir2's DMS is longer than 1 block) use file allocation table to traverse through dir2's DMS to find 'fileA'




