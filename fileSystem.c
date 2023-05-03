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

#include "device.h"
#include "fileSystem.h"

#define UNALLOCATED 65533 // decimal value for an unallocated block in the file allocation table.
#define END_OF_FILE 65534 // decimal value for EoF in the file allocation table.

/* The file system error number. */
int file_errno = 0;

/* Index for the block containing the root directory file */
int root_block_idx;

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
    char data[64];

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
int _getDecoded(char c1, char c0) {
    int result = (unsigned char)c1 * 256 + (unsigned char)c0 - 1;

    if (result < 0 || result > 65534) {
        file_errno = EOTHER;
        return -1;
    }

    return result;
}

int _getRootSize() {
    if (isFormatted() != 0) {
        file_errno = EOTHER;
        return -1;
    }

    char buffer[64];
    if (blockRead(1, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    return _getDecoded(buffer[3], buffer[4]);
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
int _encode(int n, char *result) {
    if (n < 0 || n > 65534) {
        file_errno = EOTHER;
        return -1;
    }

    n++; // Avoid null terminator

    result[0] = n % 256;
    result[1] = (int)(n - n % 256) / 256;

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
    // Check block number validity - atleast one block must be free after formatting to be considered 'valid'
    int reserved_blocks = 1 + ceil((5 + numBlocks() * 2) / BLOCK_SIZE);
    if ((reserved_blocks - numBlocks()) <= 0) {
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
    strncpy(buffer, volumeName, 64);

    if (blockWrite(0, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    buffer[0] = '\0'; // Clear buffer
    root_block_idx = reserved_blocks; // index of root = last_reserved_block_idx - 1 | Equiv. to count of reserved blocks

    // Assign WFS header (used to quickly check if a device has been formatted before)
    buffer[0] = 'W';
    buffer[1] = 'F';
    buffer[2] = 'S';

    // Initialise root directory size to 0 (1 maps to 0 when encoded)
    buffer[3] = (char)1;
    buffer[4] = (char)1;

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
        char encoded[2];

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
    if (isFormatted() != 0) {
        file_errno = EOTHER;
        return -1;
    }

    if (blockRead(1, result) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    return 0;
}

struct BlockEntry {
    int idx;
    int value;
};

int _loadFAT(unsigned char *fat) {
    // Read FAT
    int fatLength = 5 + numBlocks() * 2;
    int remainingLength = fatLength;
    while (remainingLength > 0) {
        // Read block
        char readBuffer[BLOCK_SIZE];
        if (blockRead(1, readBuffer) == -1) {
            file_errno = EBADDEV;
            return -1;
        }

        // Get rid of FS header & root size
        if (remainingLength == fatLength) {
            if (remainingLength > BLOCK_SIZE) {
                strncat(fat, readBuffer, BLOCK_SIZE - 5);
                remainingLength -= BLOCK_SIZE;
            } else {
                strncat(fat, readBuffer, remainingLength - 5);
                remainingLength -= remainingLength;
            }
        }

        // Do others normally.
        if (remainingLength > BLOCK_SIZE) {
            strncat(fat, readBuffer, BLOCK_SIZE);
            remainingLength -= BLOCK_SIZE;
        } else {
            strncat(fat, readBuffer, remainingLength);
            remainingLength -= remainingLength;
        }
    }

    return 0;
}

struct BlockEntry getBlockEntry(int block) {
    struct BlockEntry entry = {-1, -1};
    char c1, c0;

    int c1_block = (4+2*(entry.idx))/BLOCK_SIZE + 1;
    int c1_offset = (4+2*(entry.idx)) % BLOCK_SIZE;

    int c0_block = (5+2*(entry.idx))/BLOCK_SIZE + 1;
    int c0_offset = (5+2*(entry.idx)) % BLOCK_SIZE;

    // Read C1
    char readBuffer[BLOCK_SIZE];
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
    entry.value = _getDecoded(2 * block, 2 * block + 1);

    return entry;
}

int setBlockEntry(struct BlockEntry entry) {
    // Determine block to change
    int c1_block = (4+2*(entry.idx))/BLOCK_SIZE + 1;
    int c1_offset = (4+2*(entry.idx)) % BLOCK_SIZE;

    int c0_block = (5+2*(entry.idx))/BLOCK_SIZE + 1;
    int c0_offset = (5+2*(entry.idx)) % BLOCK_SIZE;

    char encoded[2];
    _encode(entry.value, encoded);

    // Modify C1
    char readBuffer[BLOCK_SIZE];
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

/**
 * @brief Reads a specified amount of data starting at the specified block.
 *
 * @param startBlockIdx
 * @param length amount of characters to read (excl. null terminator)
 * @param result
 * @return int 0 for success, -1 for error.
 */
int _read(int startBlockIdx, int length, void *result) {
    int remainingLength = length;
    int blockIdx = startBlockIdx;

    do {
        // Read block
        char readBuffer[BLOCK_SIZE];
        if (blockRead(blockIdx, readBuffer) == -1) {
            file_errno = EBADDEV;
            return -1;
        }

        if (remainingLength > BLOCK_SIZE) {
            strncat(result, readBuffer, BLOCK_SIZE);
            remainingLength -= BLOCK_SIZE;
        } else {
            strncat(result, readBuffer, remainingLength);
            remainingLength -= remainingLength;
        }

        // Lookup next block in FAT
        blockIdx = getBlockEntry(blockIdx).value;

    } while (blockIdx != END_OF_FILE && blockIdx != UNALLOCATED);

    if (remainingLength != 0) {
        file_errno = EOTHER;
        return -1;
    }
}

int _allocateNewBlock(int lastBlockIdx, int *newBlockIdx) {
    // Search for free block
    unsigned char fat = malloc(2*numBlocks());
    *newBlockIdx = -1;
    _loadFAT(fat);

    for (int i = 0; i < numBlocks(); i++) {
        struct BlockEntry entry = getBlockEntry(i);

        if (entry.value == UNALLOCATED) {
            // Set new block -> END_OF_FILE
            entry.value = END_OF_FILE;
            if (setBlockEntry(entry) != 0) {
                return -1;
            }

            *newBlockIdx = entry.idx;

            if (lastBlockIdx > 0) {
                // Set last block - > new block
                struct BlockEntry updatedLastBlockEntry = {lastBlockIdx, entry.idx};
                if (setBlockEntry(updatedLastBlockEntry) != 0) {
                    return -1;
                }
            }
        }
    }

    if (*newBlockIdx > 0) {
        return 0;
    } else {
        file_errno = ENOROOM;
        return -1;
    }
}

int _append(int startBlockIdx, int currentLength, unsigned char *rawData, int dataLength) {
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

    // Ensure there is a null terminator at the end of data
    unsigned char *data;
    if (rawData[dataLength-1] != '\0') {
        data = malloc(dataLength + 1);
        strncpy(data, rawData, dataLength);
        data[dataLength] = '\0';

        dataLength++;

    } else {
        data = malloc(dataLength); 
        strncpy(data, rawData, dataLength);
    }

    // Start appending
    int bufferPos = currentLength % BLOCK_SIZE - 1;  // We want to overwrite the prev. null terminator
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
        }

        buffer[bufferPos] = data[dataPos];
        dataPos++;
    }

    // Commit last block
    if (blockWrite(blockIdx, buffer)) {
        file_errno = EBADDEV;
        return -1;
    }

    return 0;
}

struct DirectoryEntry {
    int startBlockIdx;
    int filesize;
    int dirFileOffset;
};

/**
 * @brief Get the DirectoryEntry from given directory file data
 *
 * @param cwdData char array of directory file data.
 * @param targetName file/directory name to retrieve the address for.
 * @param type 'F' for file, 'D' for directory
 * @return DirectoryEntry struct with all values -1 if not found, block index (address) and its length otherwise.
 */
struct DirectoryEntry
_getAddressFromDirectoryFile(char *cwdData, int cwdLength, char *targetName, char type) {
    // Get target
    char target[9];
    strncpy(target, targetName, 7);
    target[7] = type;
    target[8] = '\0';

    // Parse & search
    struct DirectoryEntry addr = {-1, -1};
    for (int i = 0; i < cwdLength; i += 12) {
        if (strncmp(target, cwdData + i, 8) == 0) {
            // Found target file/directory - return addr
            addr.startBlockIdx = _getDecoded(cwdData[i + 8], cwdData[i + 9]);
            addr.length = _getDecoded(cwdData[i + 10], cwdData[i + 11]);
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
getAddressFromDirectory(int cwd, int cwdLength, char *targetName, char type) {
    // Read directory
    char data = malloc(cwdLength);
    if (_read(cwd, cwdLength, data) != 0) {
        file_errno = EOTHER;
        struct DirectoryEntry addr = {-1, -1};
        return addr;
    }

    return _getAddressFromDirectoryFile(data, cwdLength, targetName, type);
}

int _createFile(unsigned char *fileName, unsigned char type, int parentDirBlock, int parentDirLength) {
    // Allocate new block
    int newBlockIdx;
    if (_allocateNewBlock(-1, &newBlockIdx) != 0) {
        return -1;
    }

    // Append to directory file
    unsigned char directoryEntry[12] = {' ', ' ', ' ', ' ', ' ', ' ', ' '};
    strncat(directoryEntry, fileName, 7);
    // Set type
    directoryEntry[7] = type;
    // Set start block
    unsigned char encoded[2];
    _encode(newBlockIdx, encoded);
    directoryEntry[8] = encoded[1];
    directoryEntry[9] = encoded[0];
    // Set file size (initially 0)
    directoryEntry[10] = (unsigned char)1;
    directoryEntry[11] = (unsigned char)1;
    
    if (_append(parentDirBlock, parentDirLength, directoryEntry, 12) != 0) {
        return -1;
    }
}

int _updateFileSize(unsigned char *fileName, unsigned char type, int dirBlock, int dirLength, int fileSize) {
    // Allocate new block
    int newBlockIdx;
    if (_allocateNewBlock(-1, &newBlockIdx) != 0) {
        return -1;
    }

    // Append to directory file
    unsigned char directoryEntry[12] = {' ', ' ', ' ', ' ', ' ', ' ', ' '};
    strncat(directoryEntry, fileName, 7);
    // Set type
    directoryEntry[7] = type;
    // Set start block
    unsigned char encoded[2];
    _encode(newBlockIdx, encoded);
    directoryEntry[8] = encoded[1];
    directoryEntry[9] = encoded[0];
    // Set file size (initially 0)
    directoryEntry[10] = (unsigned char)1;
    directoryEntry[11] = (unsigned char)1;
    
    if (_append(parentDirBlock, parentDirLength, directoryEntry, 12) != 0) {
        return -1;
    }
}

/**
 * Start with fullPath + 1 and diraddr=rootidx and dirlength =root length
*/
int _updateDirectorySizes(unsigned char *path, int dirAddr, int dirLength) {
    unsigned char targetChildName[7];
    unsigned char *c;
    for (c = path; c != '\\'; c++) {
        if (c == '\0') {
            return -1;
        } else {
            strncat(targetChildName, c, 1);
        }
    }

    // Find & update target child if it is a directory
    if (targetChildName[0] != '\0') {
        struct DirectoryEntry targetChild = getAddressFromDirectory(dirAddr, dirLength, targetChildName, 'D');
        int childSum = _updateDirectorySizes(c+1, targetChild.startBlockIdx, targetChild.length);
        if (targetChild.) {

        }
    }
    

    // Update my sum
    for () {

    }
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
    char nameBuffer[8];
    int cwdAddress = root_block_idx;
    int cwdLength = _getRootSize();

    // Navigate and create requested directories.
    for (int i = 1; i != '\0'; i++) {
        char c = pathName[i];

        if (c == '\\') {
            // (Create if necessary and) Navigate to the directory specified.

            // Read cwd file
            char data = malloc(cwdLength);
            if (_read(cwdAddress, cwdLength, data) != 0) {
                return -1;
            }

            // Get new directory file address
            struct DirectoryEntry newDirAddr = _getAddressFromDirectoryFile(data, cwdLength, nameBuffer, 'D');
            if (newDirAddr.startBlockIdx == -1) {
                // Create directory
                if (_createFile(nameBuffer, 'F', cwdAddress, cwdLength) != 0) {
                    return -1;
                }
                
            } else {
                // 'Navigate' to directory
                cwdAddress = newDirAddr.startBlockIdx;
                cwdLength = newDirAddr.length;
            }

            nameBuffer[0] = '\0'; // Clear name buffer
        } else {
            // Still processing name. Add to buffer and keep going.
            strncat(nameBuffer, c, 1);
        }
    }

    // File creation requested, create file.
    if (nameBuffer != '\0') {
        if (_createFile(nameBuffer, 'D', cwdAddress, cwdLength) != 0) {
            return -1;
        }
    }

    // Update file & directory sizes

    return -1;
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
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length) {
    return -1;
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
    return -1;
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
    return -1;
}
