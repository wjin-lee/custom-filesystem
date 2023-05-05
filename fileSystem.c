/*
 * fileSystem.c
 *
 *  Modified on: 03/05/2023
 *      Author: wlee447
 *
 * Complete this file.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device.h"
#include "fileSystem.h"

#define UNALLOCATED 65533 // decimal value for an unallocated block in the file allocation table.
#define END_OF_FILE 65534 // decimal value for EoF in the file allocation table.

/* The file system error number. */
int file_errno = 0;

/* Index for the block containing the root directory file */
int root_block_idx;

struct BlockEntry {
    int idx;
    int value;
};

struct DirectoryEntry {
    int startBlockIdx;
    int filesize;
    int dirFileOffset;
};

#define MAX_OPEN_FILES 1022  // Assignment brief only allows for max 1024 blocks. Each file/dir at least requires 1 block. Sys area takes up atleast 2.
struct FilePointer {
    int startBlockIdx;  // Serves as unique ID
    int offset;
};

// Global file pointer list
struct FilePointer filePointers[MAX_OPEN_FILES*sizeof(struct FilePointer)];
int nOpenFiles = 0;  // Number of open files

void resetBlocks() {
    printf("RESETTING BLOCKS");
    unsigned char buffer[64] = "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO";
    for (int i = 0; i < numBlocks(); i++) {
        blockWrite(i, buffer);
    }
}

/**
 * @brief Returns 1 if a given device is formatted in WFS. Otherwise, return 0.
 * Returns -1 on error.
 *
 * This function only gives an estimate, (only checks the file headers are there) but
 * that was deemed to be enough for the purposes of this assignment.
 *
 * @return int
 */
int isFormatted() {
    unsigned char data[64];

    if (blockRead(1, data) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    return (data[0] == 'W') && (data[1] == 'F') && (data[2] == 'S');
}

/**
 * @brief decodes the given characters from the base256 storage format.
 * NOTE: valid literal values range from 1 to 65535 (mapping to 0 to 65534) since 00000000 is the NULL terminator.
 *
 * @return decoded integer or -1 if error.
 */
int _getDecoded(unsigned char c1, unsigned char c0) {
    int result = (unsigned char)c1 * 256 + (unsigned char)c0;

    if (result < 0 || result > 65535) {
        file_errno = EOTHER;
        return -1;
    }

    return result;
}

/**
 * @brief encodes the given integer in range 0 -> 65534 to characters in the base 256 storage format.
 * NOTE: valid literal values range from 1 to 65535 (mapping to 0 to 65534) since 00000000 is the NULL terminator.
 *
 * @param n integer to convert
 * @param result pointer to an  char array of length 2
 *
 * @return 0 if successful, -1 if error.
 */
int _encode(int n, unsigned char *result) {
    if (n < 0 || n > 65535) {
        file_errno = EOTHER;
        return -1;
    }

    result[0] = (n % 256);
    result[1] = (int)((n - (n % 256)) / 256);

    return 0;
}

int _getRootSize() {
    if (isFormatted() != 1) {
        file_errno = EOTHER;
        return -5;
    }

    unsigned char buffer[64];
    if (blockRead(1, buffer) == -1) {
        file_errno = EBADDEV;
        return -2;
    }
    printf("Getting root size: %i\n", _getDecoded(buffer[3], buffer[4]));
    return _getDecoded(buffer[3], buffer[4]);
}

int _setRootSize(int size) {
    printf("Setting root size: %i\n", size);
    unsigned char buffer[64];
    if (blockRead(1, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    unsigned char encoded[2];
    _encode(size, encoded);

    buffer[3] = encoded[1];
    buffer[4] = encoded[0];

    if (blockWrite(1, buffer)) {
        file_errno = EBADDEV;
        return -1;
    }

    return 0;
}

int _loadFAT(unsigned char *fat) {
    fat[0] = '\0';

    // Read FAT
    int fatLength = 5 + numBlocks() * 2;
    int remainingLength = fatLength;
    while (remainingLength > 0) {
        // Read block
        unsigned char readBuffer[BLOCK_SIZE] = {'\0'};
        if (blockRead(1, readBuffer) == -1) {
            file_errno = EBADDEV;
            return -1;
        }

        // Get rid of FS header & root size
        if (remainingLength == fatLength) {
            if (remainingLength > BLOCK_SIZE) {
                memcpy((char *)fat, (char *)(readBuffer + 4), BLOCK_SIZE - 5);
                remainingLength -= BLOCK_SIZE;
            } else {
                memcpy((char *)fat, (char *)(readBuffer + 4), remainingLength - 5);
                remainingLength -= remainingLength;
            }
        }

        // Do others normally.
        if (remainingLength > BLOCK_SIZE) {
            memcpy((char *)fat, (char *)readBuffer, BLOCK_SIZE);
            remainingLength -= BLOCK_SIZE;
        } else {
            memcpy((char *)fat, (char *)readBuffer, remainingLength);
            remainingLength -= remainingLength;
        }
    }

    return 0;
}

struct BlockEntry getBlockEntry(int block) {
    struct BlockEntry entry = {-1, -1};
    unsigned char c1, c0;

    int c1_block = (5 + 2 * (block)) / BLOCK_SIZE + 1;
    int c1_offset = (5 + 2 * (block)) % BLOCK_SIZE;

    int c0_block = (6 + 2 * (block)) / BLOCK_SIZE + 1;
    int c0_offset = (6 + 2 * (block)) % BLOCK_SIZE;

    // printf("Entry for block %i: %i %i %i %i \n", block, c1_block, c1_offset, c0_block, c0_offset);

    // Read C1
    unsigned char readBuffer[BLOCK_SIZE];
    if (blockRead(c1_block, readBuffer) == -1) {
        file_errno = EBADDEV;
        return entry;
    }
    c1 = readBuffer[c1_offset];

    if (c1_block == c0_block) {
        c0 = readBuffer[c0_offset];
    } else {
        // Read c0
        if (blockRead(c0_block, readBuffer) == -1) {
            file_errno = EBADDEV;
            return entry;
        }
        c0 = readBuffer[c0_offset];
    }

    entry.idx = block;
    entry.value = _getDecoded(c1, c0);

    return entry;
}

int setBlockEntry(struct BlockEntry entry) {
    // Determine block to change
    int c1_block = (5 + 2 * (entry.idx)) / BLOCK_SIZE + 1;
    int c1_offset = (5 + 2 * (entry.idx)) % BLOCK_SIZE;

    int c0_block = (6 + 2 * (entry.idx)) / BLOCK_SIZE + 1;
    int c0_offset = (6 + 2 * (entry.idx)) % BLOCK_SIZE;

    // printf("Entry for block %i: %i %i %i %i \n", entry.idx, c1_block, c1_offset, c0_block, c0_offset);

    unsigned char encoded[2];
    _encode(entry.value, encoded);

    // Modify C1
    unsigned char readBuffer[BLOCK_SIZE];
    if (blockRead(c1_block, readBuffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }
    readBuffer[c1_offset] = encoded[1];

    // Maybe modify c0 as well.
    if (c1_block == c0_block) {
        readBuffer[c0_offset] = encoded[0];
    }

    // Write C1
    if (blockWrite(c1_block, readBuffer)) {
        file_errno = EBADDEV;
        return -1;
    }

    // Modify and write c0
    if (c1_block != c0_block) {
        if (blockRead(c0_block, readBuffer) == -1) {
            file_errno = EBADDEV;
            return -1;
        }
        readBuffer[c0_offset] = encoded[0];
        if (blockWrite(c0_block, readBuffer)) {
            file_errno = EBADDEV;
            return -1;
        }
    }

    return 0;
}

/*
 * Formats the device for use by this file system.
 * The volume name must be < 64 bytes long.
 * All information previously on the device is lost.
 * Also creates the root directory "/".
 * Returns 0 if no problem or -1 if the call failed.
 */
int format(char *volumeName) {
    resetBlocks();

    // Clear file pointers
    memset(filePointers, 0, sizeof(filePointers));

    // Check block number validity - atleast one block must be free after formatting to be considered 'valid'
    int reserved_blocks = 1 + ((5 + numBlocks() * 2 + (BLOCK_SIZE - 1)) / BLOCK_SIZE); // always round up
    if ((numBlocks() - reserved_blocks) <= 0) {
        file_errno = ENOROOM;
        return -1;
    }

    // Check volume name validiity
    if (strlen(volumeName) > (BLOCK_SIZE - 1)) {
        file_errno = EBADVOLNAME;
        return -1;
    }

    // Set volume name to first block
    unsigned char buffer[64];
    strncpy((char *)buffer, volumeName, 64);

    if (blockWrite(0, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    root_block_idx = reserved_blocks; // index of root = last_reserved_block_idx - 1 | Equiv. to count of reserved blocks

    // Assign WFS header (used to quickly check if a device has been formatted before)
    buffer[0] = 'W';
    buffer[1] = 'F';
    buffer[2] = 'S';

    // Initialise root directory size to 0
    buffer[3] = (unsigned char)0;
    buffer[4] = (unsigned char)0;

    if (blockWrite(1, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    /**
     * Create FAT - every 2 characters represents both an 'allocated' flag and the next block value in linked list.
     *
     * ==============
     *    ENCODING
     * --------------
     * ALL VALUES INCLUSIVE.
     *
     * 0           :    -    : INVALID VALUE (NOTHING POINTS TO VOLUME NAME AS NEXT)
     * 1-65533     : 0-65532 : NEXT BLOCK INDEX
     * 65534       :  65533  : UNALLOCATED BLOCK
     * 65535       :  65534  : END OF FILE
     * ==============
     */
    for (int i = 0; i < numBlocks(); i++) {
        struct BlockEntry entry = {i, UNALLOCATED};

        // Set to allocated if it is a reserved block or the root (i.e. last reserved block idx + 1).
        if (i <= reserved_blocks) {
            entry.value = END_OF_FILE;
        }

        setBlockEntry(entry);
    }

    return 0;
}

/*
 * Returns the volume's name in the result.
 * Returns 0 if no problem or -1 if the call failed.
 */
int volumeName(char *result) {
    if (isFormatted() != 1) {
        file_errno = EOTHER;
        return -1;
    }

    if (blockRead(0, (unsigned char *)result) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    return 0;
}

int min(int n0, int n1) {
    if (n0 < n1) {
        return n0;
    } else {
        return n1;
    }
}

/**
 * @brief Reads a specified amount of data starting at the specified block.
 *
 * @param startBlockIdx
 * @param length amount of characters to read (excl. null terminator)
 * @param result
 * @return int 0 for success, -1 for error.
 */
int _read(int startBlockIdx, int length, int offset, unsigned char *result) {
    printf("READING FROM BLOCK: %i with offset: %i for length %i\n", startBlockIdx, offset, length);
    result[0] = '\0';

    int remainingLength = length;
    int remainingOffset = offset;
    int blockIdx = startBlockIdx;

    do {
        printf("Reading block %i. Remaining: %i\n", blockIdx, remainingLength);
        // Read block
        unsigned char readBuffer[BLOCK_SIZE] = {'\0'};
        if (blockRead(blockIdx, readBuffer) == -1) {
            file_errno = EBADDEV;
            return -1;
        }



        if (remainingOffset > BLOCK_SIZE) {
            printf("whole block offset\n");
            // Offset by whole block
            remainingOffset -= BLOCK_SIZE;
        } else {
            printf("partial offset\n");
            // Copy partial block with offset
            memcpy((char *)result + (length - remainingLength), (char *)readBuffer+remainingOffset, min(BLOCK_SIZE-remainingOffset, remainingLength));
            remainingLength -= min(BLOCK_SIZE-remainingOffset, remainingLength);
            remainingOffset -= remainingOffset;
        }
        // } else if (remainingLength > BLOCK_SIZE) {
        //     printf("whole block read\n");
        //     // Copy whole block
        //     memcpy((char *)result + (length - remainingLength), (char *)readBuffer, BLOCK_SIZE);
        //     remainingLength -= BLOCK_SIZE;
        // } else {
        //     printf("partial block read w/o offset\n");
        //     // Copy partial block without offset
        //     memcpy((char *)result + (length - remainingLength), (char *)readBuffer, remainingLength);
        //     remainingLength -= remainingLength;
        // }

        // Lookup next block in FAT
        blockIdx = getBlockEntry(blockIdx).value;

    } while (remainingLength > 0 && (blockIdx != END_OF_FILE && blockIdx != UNALLOCATED));

    if (remainingOffset > 0) {
        printf("EoF Reached.\n");
        // Not an error - user must seek 0.

    } else if (remainingLength != 0) {
        printf("ERROR! Remaining length: %i\n", remainingLength);
        file_errno = EOTHER;
        return -1;
    }

    return 0;
}

int _allocateNewBlock(int lastBlockIdx, int *newBlockIdx) {
    // Search for free block
    unsigned char *fat = malloc(2 * numBlocks() + 1);
    *newBlockIdx = -1;
    if (_loadFAT(fat) != 0) {
        file_errno = EOTHER;
        free(fat);
        return -1;
    }

    for (int i = 0; i < numBlocks(); i++) {

        struct BlockEntry entry = getBlockEntry(i);
        printf("ITERATING BLOCK: %i. UNALLOCATED = %i\n", i, entry.value == UNALLOCATED);

        if (entry.value == UNALLOCATED) {
            // Set new block -> END_OF_FILE
            entry.value = END_OF_FILE;
            if (setBlockEntry(entry) != 0) {
                free(fat);
                return -1;
            }

            *newBlockIdx = entry.idx;

            if (lastBlockIdx > 0) {
                // Set last block - > new block
                struct BlockEntry updatedLastBlockEntry = {lastBlockIdx, entry.idx};
                if (setBlockEntry(updatedLastBlockEntry) != 0) {
                    free(fat);
                    return -1;
                }
            }

            printf("Allocated new block: %i\n", *newBlockIdx);
            free(fat);
            return 0;
        }
    }

    free(fat);
    file_errno = ENOROOM;
    return -1;
}

int _append(int startBlockIdx, int currentLength, unsigned char *data, int dataLength) {
    printf("Appending %s to file starting at %i\n", data, startBlockIdx);
    // Traverse to last block
    int blockIdx = startBlockIdx;
    while (getBlockEntry(blockIdx).value != END_OF_FILE) {
        blockIdx = getBlockEntry(blockIdx).value;
    }

    unsigned char buffer[64];
    if (blockRead(blockIdx, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }
    printf("READ block:\n");
    for (int i = 0; i < 64; i++)
        printf("%c", data[i]);
    printf("\n");

    // Start appending
    int bufferPos = currentLength % BLOCK_SIZE;
    int dataPos = 0;
    while (dataPos < dataLength) {
        // Check if block is full
        if (bufferPos > BLOCK_SIZE - 1) {
            // Commit current block
            if (blockWrite(blockIdx, buffer)) {
                file_errno = EBADDEV;
                return -1;
            }

            // Assign new block
            int newBlockIdx;
            if (_allocateNewBlock(blockIdx, &newBlockIdx) != 0) {
                return -1;
            }
            blockIdx = newBlockIdx;
            bufferPos = 0;
        }

        printf("Set block pos %i to data pos %i (%c - %i)\n", bufferPos, dataPos, data[dataPos], (int)data[dataPos]);
        buffer[bufferPos] = data[dataPos];
        dataPos++;
        bufferPos++;
    }

    // Commit last block
    printf("Writing block:\n");
    if (blockWrite(blockIdx, buffer)) {
        printf("BLOCK WRITE FAILED");
        file_errno = EBADDEV;
        return -1;
    }

    return 0;
}

/**
 * @brief Get the DirectoryEntry from given directory file data
 *
 * @param cwdData char array of directory file data.
 * @param targetName file/directory name to retrieve the address for.
 * @param type 'F' for file, 'D' for directory
 * @return DirectoryEntry struct with all values -1 if not found, block index (address) and its length otherwise.
 */
struct DirectoryEntry
_getAddressFromDirectoryFile(unsigned char *cwdData, int cwdLength, unsigned char *targetName, char type) {
    // Get target
    char target[9];
    strncpy(target, (char *)targetName, 7);
    target[7] = type;
    target[8] = '\0';

    // Parse & search
    struct DirectoryEntry addr = {-1, -1, -1};
    for (int i = 0; i < cwdLength; i += 12) {
        if (strncmp(target, (char *)cwdData + i, 8) == 0) {
            // Found target file/directory - return addr
            addr.startBlockIdx = _getDecoded(cwdData[i + 8], cwdData[i + 9]);
            addr.filesize = _getDecoded(cwdData[i + 10], cwdData[i + 11]);
            addr.dirFileOffset = i;
            return addr;
        }
    }

    // Not found
    return addr;
}

/**
 * @brief Get the DirectoryEntry From directory
 *
 * @param cwd start index of the block containing the working directory file
 * @param targetName file/directory name to retrieve the address for.
 * @param type 'F' for file, 'D' for directory
 * @return DirectoryEntry struct with all values -1 if not found, block index (address) and its length otherwise.
 */
struct DirectoryEntry
getAddressFromDirectory(int cwd, int cwdLength, unsigned char *targetName, char type) {
    // Read directory
    if (cwdLength > 0) {
        unsigned char *data = malloc(cwdLength);
        if (_read(cwd, cwdLength, 0, data) != 0) {
            file_errno = EOTHER;
            struct DirectoryEntry addr = {-1, -1, -1};
            free(data);
            return addr;
        }

        struct DirectoryEntry result = _getAddressFromDirectoryFile(data, cwdLength, targetName, type);
        free(data);
        return result;
    }

    // empty directory - not found.
    struct DirectoryEntry addr = {-1, -1, -1};
    return addr;
}

/**
 * Mode is either C for create or F for find
*/
int getDirectory(char * path, char type, struct DirectoryEntry result, char mode) {
    unsigned char nameBuffer[8] = {'\0'};
    int cwdAddress = root_block_idx;
    int cwdLength = _getRootSize();

    unsigned char parentNameBuffer[8];
    int cwdParentAddress; // Start block number of cwd parent
    int cwdParentLength;  // Length of dir file for cwd parent

    // Navigate to file
    for (int i = 1; path[i] != '\0'; i++) {
        unsigned char c = path[i];

        if (c == '/') {
            // Get directory file address
            struct DirectoryEntry dirAddr = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, type);
            if (dirAddr.startBlockIdx == -1) {
                if (mode == 'F') {
                    // Find mode - no such file.
                    file_errno = ENOSUCHFILE;
                    return -1;
                } else {
                    // Create mode - create directory.
                    printf("Creating dir: %s\n", nameBuffer);
                    int newDirStartIdx;
                    if (_createFile(nameBuffer, 'D', cwdAddress, cwdLength, &newDirStartIdx) != 0) {
                        return -2;
                    }

                    // We must update the cwd parent's dir file to increase the filesize record of cwd
                    // If it is root, we know where filesize is.
                    if (cwdAddress == root_block_idx) {
                        _setRootSize(cwdLength + 12);
                    } else {
                        if (_updateFilesize(cwdParentAddress, cwdParentLength, parentNameBuffer, 'D', cwdLength + 12) != 0) {
                            return -3;
                        }
                    }

                    // Navigate to new directory
                    cwdParentAddress = cwdAddress;
                    cwdParentLength = cwdLength+12;

                    cwdAddress = newDirStartIdx;
                    cwdLength = 0;
                }
                
                
            } else {
                printf("Setting CWD address and length");
                // 'Navigate' to directory
                cwdAddress = dirAddr.startBlockIdx;
                cwdLength = dirAddr.filesize;
            }

            strncpy((char *)parentNameBuffer, (char *)nameBuffer, 7);
            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat((char *)nameBuffer, (char *)&c, 1);
        }
    }

    result.startBlockIdx = cwdAddress;
    result.filesize - cwdLength;
    
    return 0;
}


/**
 * @brief Allocates a new block for the given file and makes the directory file entry.
 *
 * NOTE: Does not increase the size of the parent's directory size in grandparent directory file.
 *
 * @param fileName
 * @param type
 * @param parentDirBlock
 * @param parentDirLength
 * @return int
 */
int _createFile(unsigned char *fileName, unsigned char type, int parentDirBlock, int parentDirLength, int *newBlockIdx) {
    // Allocate new block
    if (_allocateNewBlock(-1, newBlockIdx) != 0) {
        return -1;
    }

    // Append to directory file
    unsigned char directoryEntry[12] = {' ', ' ', ' ', ' ', ' ', ' ', ' '};
    strncpy((char *)directoryEntry, (char *)fileName, 7);
    // Set type
    directoryEntry[7] = type;
    // Set start block
    unsigned char encoded[2];
    _encode(*newBlockIdx, encoded);
    directoryEntry[8] = encoded[1];
    directoryEntry[9] = encoded[0];
    // Set file size (initially 0)
    directoryEntry[10] = (unsigned char)0;
    directoryEntry[11] = (unsigned char)0;

    if (_append(parentDirBlock, parentDirLength, directoryEntry, 12) != 0) {
        printf("Append failed");
        return -1;
    }

    return 0;
}

int _updateFilesize(int dirStartBlock, int dirLength, unsigned char *targetName, char type, int filesize) {
    // Read directory
    unsigned char *data = malloc(dirLength);
    if (_read(dirStartBlock, dirLength, 0, data) != 0) {
        file_errno = EOTHER;
        free(data);
        return -1;
    }

    // Get target
    char target[9];
    strncpy(target, (char *)targetName, 7);
    target[7] = type;

    // Parse & search
    for (int i = 0; i < dirLength; i += 12) {
        if (strncmp(target, (char *)data + i, 8) == 0) {
            // Found target file/directory - calculate block & offset
            unsigned char encoded[2];
            _encode(filesize, encoded);

            int c1_block = dirStartBlock + (i + 10) / BLOCK_SIZE;
            int c1_offset = (i + 10) % BLOCK_SIZE;

            int c0_block = dirStartBlock + (i + 11) / BLOCK_SIZE;
            int c0_offset = (i + 11) % BLOCK_SIZE;

            // Make change
            // Modify C1
            unsigned char readBuffer[BLOCK_SIZE];
            if (blockRead(c1_block, readBuffer) == -1) {
                file_errno = EBADDEV;
                free(data);
                return -1;
            }
            readBuffer[c1_offset] = encoded[1];

            // Maybe modify c0 as well.
            if (c1_block == c0_block) {
                readBuffer[c0_offset] = encoded[0];
            }

            // Write C1
            if (blockWrite(c1_block, readBuffer)) {
                file_errno = EBADDEV;
                free(data);
                return -1;
            }

            // Modify and write c0
            if (c1_block != c0_block) {
                if (blockRead(c0_block, readBuffer) == -1) {
                    file_errno = EBADDEV;
                    free(data);
                    return -1;
                }
                readBuffer[c0_offset] = encoded[0];
                if (blockWrite(c0_block, readBuffer)) {
                    file_errno = EBADDEV;
                    free(data);
                    return -1;
                }
            }

            free(data);
            return 0;
        }
    }

    free(data);

    // Not found
    file_errno = EOTHER;
    return -1;
}

/*
 * Makes a file with a fully qualified pathname starting with "/".
 * It automatically creates all intervening directories.
 * Pathnames can consist of any printable ASCII characters (0x20 - 0x7e)
 * including the space character.
 * The occurrence of "/" is interpreted as starting a new directory
 * (or the file name).
 * Each section of the pathname must be between 1 and 7 bytes long (not
 * counting the "/"s).
 * The pathname cannot finish with a "/" so the only way to create a directory
 * is to create a file in that directory. This is not true for the root
 * directory "/" as this needs to be created when format is called.
 * The total length of a pathname is limited only by the size of the device.
 * Returns 0 if no problem or -1 if the call failed.
 */
int create(char *pathName) {
    printf("CREATE FILE CALL: %s\n", pathName);
    unsigned char nameBuffer[8] = {'\0'};
    int cwdAddress = root_block_idx;
    printf("ROOT ADDR: %i | %d", root_block_idx, (5 + numBlocks() * 2 + (BLOCK_SIZE - 1)) / BLOCK_SIZE);
    int cwdLength = _getRootSize();

    unsigned char parentNameBuffer[8];
    int cwdParentAddress; // Start block number of cwd parent
    int cwdParentLength;  // Length of dir file for cwd parent

    // Navigate and create requested directories.
    for (int i = 1; pathName[i] != '\0'; i++) {
        unsigned char c = pathName[i];

        if (c == '/') {
            // (Create if necessary and) Navigate to the directory specified.
            struct DirectoryEntry newDirAddr;
            // Read cwd file
            if (cwdLength > 0) {
                unsigned char *data = malloc(cwdLength);
                if (_read(cwdAddress, cwdLength, 0, data) != 0) {
                    free(data);
                    return -9;
                }

                // Get new directory file address
                newDirAddr = _getAddressFromDirectoryFile(data, cwdLength, nameBuffer, 'D');
                free(data);
            } else {
                newDirAddr.startBlockIdx = -1;
            }

            if (newDirAddr.startBlockIdx == -1) {
                // Create directory
                printf("Creating dir: %s\n", nameBuffer);
                int newDirStartIdx;
                if (_createFile(nameBuffer, 'D', cwdAddress, cwdLength, &newDirStartIdx) != 0) {
                    return -2;
                }

                // We must update the cwd parent's dir file to increase the filesize record of cwd
                // If it is root, we know where filesize is.
                if (cwdAddress == root_block_idx) {
                    _setRootSize(cwdLength + 12);
                } else {
                    if (_updateFilesize(cwdParentAddress, cwdParentLength, parentNameBuffer, 'D', cwdLength + 12) != 0) {
                        return -3;
                    }
                }

                // Navigate to new directory
                cwdParentAddress = cwdAddress;
                cwdParentLength = cwdLength+12;

                cwdAddress = newDirStartIdx;
                cwdLength = 0;

            } else {
                // 'Navigate' to directory
                printf("Navigating to dir: %s\n", nameBuffer);
                cwdParentAddress = cwdAddress;
                cwdParentLength = cwdLength;

                cwdAddress = newDirAddr.startBlockIdx;
                cwdLength = newDirAddr.filesize;
            }

            
            strncpy((char *)parentNameBuffer, (char *)nameBuffer, 7);
            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat((char *)nameBuffer, (char *)&c, 1);
        }
    }

    // File creation requested, create file.
    printf("Creating file: %s at %i\n", nameBuffer, cwdAddress);
    if (nameBuffer != '\0') {
        int _; // Throwaway variable
        if (_createFile(nameBuffer, 'F', cwdAddress, cwdLength, &_) != 0) {
            return -4;
        }
        // We must update the cwd parent's dir file to increase the filesize record of cwd
        // If it is root, we know where filesize is.
        if (cwdAddress == root_block_idx) {
            _setRootSize(cwdLength + 12);
        } else {
            if (_updateFilesize(cwdParentAddress, cwdParentLength, parentNameBuffer, 'D', cwdLength + 12) != 0) {
                return -5;
            }
        }
    }

    // Update file & directory sizes
    // printf("UPDATE DIR SIZES | %i", _getRootSize());
    // _updateDirectorySizes((unsigned char *)pathName, root_block_idx, _getRootSize());

    return 0;
}

/**
 * Recurse down the directory tree and retrieve the size
 */
int _getDirectorySize(int dirAddr, int dirLength) {
    int sum = dirLength;

    // Read request dir
    if (dirLength > 0) {
        unsigned char *directory = malloc(dirLength);
        if (_read(dirAddr, dirLength, 0, directory) != 0) {
            file_errno = EOTHER;
            free(directory);
            return -1;
        }

        for (int i = 0; i < dirLength; i += 12) {
            if (directory[i+7] == 'D') {
                sum += _getDirectorySize(_getDecoded(directory[i+8], directory[i+9]), _getDecoded(directory[i+10], directory[i+11]));
            } else {
                sum += _getDecoded(directory[i+10], directory[i+11]);
            }
        }

        free(directory);
    }

    return sum;
}

/*
 * Returns a list of all files in the named directory.
 * The "result" string is filled in with the output.
 * The result looks like this

/dir1:
file1	42
file2	0

 * The fully qualified pathname of the directory followed by a colon and
 * then each file name followed by a tab "\t" and then its file size.
 * Each file on a separate line.
 * The directoryName is a full pathname.
 */
void list(char *result, char *directoryName) {
    result[0] = '\0';
    unsigned char nameBuffer[8] = {'\0'};
    int cwdAddress = root_block_idx;
    int cwdLength = _getRootSize();

    // Navigate requested directory.
    for (int i = 1; ((i < strlen(directoryName)) || (directoryName[i] != '\0')); i++) {
        unsigned char c = directoryName[i];
        printf("PROCESSING : %c\n", c);

        if (c == '/') {
            printf("BREAK & PROCESS: %s\n", nameBuffer);

            // Get directory file address
            struct DirectoryEntry dirAddr = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');
            printf("RES: %i\n", dirAddr.startBlockIdx);
            if (dirAddr.startBlockIdx == -1) {
                file_errno = ENOSUCHFILE;
                return;
            } else {
                printf("Setting CWD address and length");
                // 'Navigate' to directory
                cwdAddress = dirAddr.startBlockIdx;
                cwdLength = dirAddr.filesize;
            }

            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat((char *)nameBuffer, (char *)&c, 1);
        }
    }

    // Navigate to request directory
    if (nameBuffer[0] != '\0') {
        struct DirectoryEntry dirAddr = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');
        printf("RES: %i\n", dirAddr.startBlockIdx);
        if (dirAddr.startBlockIdx == -1) {
            file_errno = ENOSUCHFILE;
            return;
        } else {
            printf("Setting CWD address and length");
            // 'Navigate' to directory
            cwdAddress = dirAddr.startBlockIdx;
            cwdLength = dirAddr.filesize;
        }
    }
    

    // Print dir contents
    strncat(result, directoryName, strlen(directoryName) + 1);
    strcat(result, ":\n");

    // Read request dir
    if (cwdLength > 0) {
        unsigned char *data = malloc(cwdLength);
        printf("READING REQUEST DIR\n");
        if (_read(cwdAddress, cwdLength, 0, data) != 0) {
            free(data);
            return;
        }

        for (int i = 0; i < cwdLength; i += 12) {
            strncat(result, (char *)data + i, 7); // directory/file name
            strcat(result, ":\t");
            int filesize = _getDecoded(data[i+10], data[i+11]);
            char filesizeFormatted[6]; // max filesize is 256^2 -> I.e. 5 chars max.
            if (data[i+7] == 'D') {
                sprintf(filesizeFormatted, "%i", _getDirectorySize(_getDecoded(data[i+8], data[i+9]), filesize));
            } else {
                sprintf(filesizeFormatted, "%i", filesize);
            }
            strncat(result, filesizeFormatted, 5); // size
            strcat(result, "\n");     // each file on newline
        }
    }

    strncat(result, "\0", 1);

    printf("========================= OUT ========================\n");
    printf("%s", result);
    printf("======================================================\n");
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length) {
    unsigned char nameBuffer[8] = {'\0'};
    int cwdAddress = root_block_idx;
    int cwdLength = _getRootSize();

    // Navigate to file
    for (int i = 1; fileName[i] != '\0'; i++) {
        unsigned char c = fileName[i];
        printf("PROCESSING : %c\n", c);

        if (c == '/') {
            printf("BREAK & PROCESS: %s\n", nameBuffer);

            // Get directory file address
            struct DirectoryEntry dirAddr = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');
            printf("RES: %i\n", dirAddr.startBlockIdx);
            if (dirAddr.startBlockIdx == -1) {
                file_errno = ENOSUCHFILE;
                return -1;
            } else {
                printf("Setting CWD address and length");
                // 'Navigate' to directory
                cwdAddress = dirAddr.startBlockIdx;
                cwdLength = dirAddr.filesize;
            }

            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat((char *)nameBuffer, (char *)&c, 1);
        }
    }

    struct DirectoryEntry file = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'F');
    if (file.startBlockIdx == -1) {
        file_errno = ENOSUCHFILE;
        return -1;
    }

    printf("=== WRITE === %s %i %i\n",nameBuffer, length, file.filesize);

    // File exists, append data
    if (_append(file.startBlockIdx, file.filesize, data, length) != 0) {
        printf("Append failed");
        return -1;
    }

    // Update file size for file
    _updateFilesize(cwdAddress, cwdLength, nameBuffer, 'F', file.filesize+length);

    return 0;
}

/*
 * Reads data from the start of the file.
 * Maintains a file position so that subsequent reads continue
 * from where the last read finished.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2read(char *fileName, void *data, int length) {
    if (length == 0) {
        return 0;
    }

    unsigned char nameBuffer[8] = {'\0'};
    int cwdAddress = root_block_idx;
    int cwdLength = _getRootSize();

    // Navigate to file
    for (int i = 1; fileName[i] != '\0'; i++) {
        unsigned char c = fileName[i];

        if (c == '/') {
            // Get directory file address
            struct DirectoryEntry dirAddr = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');
            printf("RES: %i\n", dirAddr.startBlockIdx);
            if (dirAddr.startBlockIdx == -1) {
                file_errno = ENOSUCHFILE;
                return -1;
            } else {
                printf("Setting CWD address and length");
                // 'Navigate' to directory
                cwdAddress = dirAddr.startBlockIdx;
                cwdLength = dirAddr.filesize;
            }

            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat((char *)nameBuffer, (char *)&c, 1);
        }
    }

    struct DirectoryEntry fileMetadata = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');

    int offset = 0;
    int fpIdx;
    // Check for any file pointers
    for (int i = 0; i < nOpenFiles; i++) {
        printf("ITERATION: %i", i);

        struct FilePointer fp = filePointers[i];
        if (fp.startBlockIdx == fileMetadata.startBlockIdx) {
            offset = fp.offset;
            fpIdx = i;
        }
    }

    if (_read(fileMetadata.startBlockIdx, length, offset, data) != 0) {
        printf("asdasda");
        return -1;
    }

    // Save file pointer 
    if (offset == 0) {
        struct FilePointer fp;
        fp.startBlockIdx = fileMetadata.startBlockIdx;
        fp.offset = length;

        filePointers[nOpenFiles] = fp;
        nOpenFiles++;

    } else {
        filePointers[fpIdx].startBlockIdx = fileMetadata.startBlockIdx;
        filePointers[fpIdx].offset = length;
    }

    return 0;
}

/*
 * Repositions the file pointer for the file at the specified location.
 * All positive integers are byte offsets from the start of the file.
 * 0 is the beginning of the file.
 * If the location is after the end of the file, move the location
 * pointer to the end of the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int seek(char *fileName, int location) {



    unsigned char nameBuffer[8] = {'\0'};
    int cwdAddress = root_block_idx;
    int cwdLength = _getRootSize();

    // Navigate to file
    for (int i = 1; fileName[i] != '\0'; i++) {
        unsigned char c = fileName[i];

        if (c == '/') {
            // Get directory file address
            struct DirectoryEntry dirAddr = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');
            printf("RES: %i\n", dirAddr.startBlockIdx);
            if (dirAddr.startBlockIdx == -1) {
                file_errno = ENOSUCHFILE;
                return -1;
            } else {
                printf("Setting CWD address and length");
                // 'Navigate' to directory
                cwdAddress = dirAddr.startBlockIdx;
                cwdLength = dirAddr.filesize;
            }

            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat((char *)nameBuffer, (char *)&c, 1);
        }
    }

    struct DirectoryEntry fileMetadata = getAddressFromDirectory(cwdAddress, cwdLength, nameBuffer, 'D');

    return -1;
}


