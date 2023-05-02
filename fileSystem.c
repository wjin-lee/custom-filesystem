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

#define END_OF_FILE 32768 // the decimal value for EoF in the file allocation table.

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

/**
 * @brief encodes the given block number (0 -> 32766 incl.) to the base256 storage format in the file allocation table.
 *
 * ==============
 *    ENCODING
 * --------------
 * ALL VALUES INCLUSIVE.
 *
 * 0           : INVALID VALUE (NULL TERMINATOR)
 * 1-32767     : FREE BLOCK INDEX
 * 32768       : END OF FILE
 * 32769-65535 : OCCUPIED (Block index mapping to 1-32767)
 * ==============
 *
 * @param n block number
 * @param allocated 0 if free, 1 if allocated.
 * @param result pointer to an  char array of length 2
 * @return 0 if successful, -1 if error.
 */
int _encodeBlockNumber(int n, int allocated, char *result) {
    if (n < 0 || n > 32766) {
        file_errno = EOTHER;
        return -1;
    }

    int raw_decimal = n + 1; // 1-indexed index in range 1 -> 32767 (both inclusive)

    if (allocated == 1) {
        raw_decimal += 32768;
    }

    result[0] = raw_decimal % 256;
    result[1] = (int)(raw_decimal - raw_decimal % 256) / 256;
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
    char buffer[64];
    strncpy(buffer, volumeName, 64);

    if (blockWrite(0, buffer) == -1) {
        file_errno = EBADDEV;
        return -1;
    }

    char *system_area = malloc(5 + numBlocks() * 2);
    root_block_idx = reserved_blocks; // index of root = last_reserved_block_idx - 1 | Equiv. to count of reserved blocks

    // Assign WFS header (used to quickly check if a device has been formatted before)
    system_area[0] = 'W';
    system_area[1] = 'F';
    system_area[2] = 'S';

    // Initialise root directory size to 0 (1 maps to 0 when encoded)
    system_area[3] = (char)1;
    system_area[4] = (char)1;

    // Create FAT - every 2 characters represents both an 'allocated' flag and the next block value in linked list.
    for (int i = 0; i < numBlocks(); i++) {
        char encoded[2];
        int allocated = 0;

        // Set to allocated if it is a reserved block or the root (i.e. last reserved block idx + 1).
        if (i <= reserved_blocks) {
            allocated = 1;
        }

        if (_encodeBlockNumber(i, allocated, encoded)) {
            file_errno = EOTHER;
            return -1;
        }

        strncat(system_area, encoded, 2);
    }

    // TODO: Write

    free(system_area);

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
    int allocated; // 1 for true, 0 for false
    int next;      // next block index
};

int _loadFAT(char *fat) {
    // Read FAT
    int length = 5 + numBlocks() * 2;
    while (length > 0) {
        // Read block
        char readBuffer[BLOCK_SIZE];
        if (blockRead(1, readBuffer) == -1) {
            file_errno = EBADDEV;
            return -1;
        }

        // Get rid of FS header & root size
        if (i == 0) {
            if (length > BLOCK_SIZE) {
                strncat(fat, readBuffer, BLOCK_SIZE - 5);
                length -= BLOCK_SIZE;
            } else {
                strncat(fat, readBuffer, length - 5);
                length -= length;
            }
        }

        // Do others normally.
        if (length > BLOCK_SIZE) {
            strncat(fat, readBuffer, BLOCK_SIZE);
            length -= BLOCK_SIZE;
        } else {
            strncat(fat, readBuffer, length);
            length -= length;
        }
    }

    return 0;
}

struct BlockEntry getBlockEntry(int block) {
    char *fat = malloc(length - 5);
    _loadFAT(fat);

    struct BlockEntry entry;

    int raw_decimal = _getDecoded(2 * block, 2 * block + 1);
    entry.allocated = raw_decimal < 32768;
    entry.next = raw_decimal - 32768;

    free(fat);

    return entry;
}

int setBlockEntry(int block, struct BlockEntry entry) {
    char *fat = malloc(length - 5);
    _loadFAT(fat);

    // WRITE

    free(fat);

    return entry;
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
    // Load FAT

    int remainingLength = length;
    int nextBlockIdx = startBlockIdx;

    do {
        // Read block
        char readBuffer[BLOCK_SIZE];
        if (blockRead(1, readBuffer) == -1) {
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
        nextBlockIdx = _getDecoded()

    } while (nextBlockIdx != END_OF_FILE);

    if (remainingLength != 0) {
        file_errno = EOTHER;
        return -1;
    }
}

struct Address {
    int startIdx;
    int length;
};

/**
 * @brief Get the Address from given directory file data
 *
 * @param cwdData char array of directory file data.
 * @param targetName file/directory name to retrieve the address for.
 * @param type 'F' for file, 'D' for directory
 * @return Address struct with all values -1 if not found, block index (address) and its length otherwise.
 */
struct Address
_getAddressFromDirectoryFile(char *cwdData, int cwdLength, char *targetName, char type) {
    // Get target
    char target[9];
    strncpy(target, targetName, 7);
    target[7] = type;
    target[8] = '\0';

    // Parse & search
    struct Address addr = {-1, -1};
    for (int i = 0; i < cwdLength; i += 12) {
        if (strncmp(target, cwdData + i, 8) == 0) {
            // Found target file/directory - return addr
            addr.startIdx = _getDecoded(cwdData[i + 8], cwdData[i + 9]);
            addr.length = _getDecoded(cwdData[i + 10], cwdData[i + 11]);
            return addr;
        }
    }

    // Not found
    return addr;
}

/**
 * @brief Get the Address From directory
 *
 * @param cwd start index of the block containing the working directory file
 * @param targetName file/directory name to retrieve the address for.
 * @param type 'F' for file, 'D' for directory
 * @return Address struct with all values -1 if not found, block index (address) and its length otherwise.
 */
struct Address
getAddressFromDirectory(int cwd, int cwdLength, char *targetName, char type) {
    // Read directory
    char data = malloc(cwdLength);
    if (_read(cwd, cwdLength, data) != 0) {
        file_errno = EOTHER;
        struct Address addr = {-1, -1};
        return addr;
    }

    return _getAddressFromDirectoryFile(data, cwdLength, targetName, type);
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
                file_errno = EOTHER;
                return -1;
            }

            // Get new directory file address
            struct Address newDirAddr = _getAddressFromDirectoryFile(data, cwdLength, nameBuffer, 'D');
            if (newDirAddr.startIdx == -1) {
                // Create directory

            } else {
                // 'Navigate' to directory
                cwdAddress = newDirAddr.startIdx;
                cwdLength = newDirAddr.length;
            }

            nameBuffer = '\0'; // Clear name buffer

        } else {
            // Still processing name. Add to buffer and keep going.
            strncat(nameBuffer, c, 1);
        }
    }

    // File creation requested, create file.
    if (nameBuffer != '\0') {
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
