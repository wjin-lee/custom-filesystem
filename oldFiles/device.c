/*
 * device.c
 *
 *  Modified on: 24/03/2023
 *      Author: Robert Sheehan
 *
 *  N.B. You are not allowed to modify this file, except as mentioned below.
 */

#include "device.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define NUMBLOCKS 1024 // you are allowed to change this value for further testing

#define EBADBLOCK 1
#define EOPENINGDEVICE 2
#define ECREATINGMMAP 3
#define EINSTALLATEXIT 4

/* The device is really just an array of blocks. */
block *deviceBlocks;

/* The device error number. */
int dev_errno = 0;

/*
 * Prints the message and then information about the most recent error.
 */
void printDevError(char *message) {
    fprintf(stderr, "%s ERROR: ", message);
    switch (dev_errno) {
    case EBADBLOCK:
        fprintf(stderr, "bad block number\n");
        break;
    case EOPENINGDEVICE:
        fprintf(stderr, "unable to open device\n");
        break;
    case ECREATINGMMAP:
        fprintf(stderr, "unable to memory map device\n");
        break;
    case EINSTALLATEXIT:
        fprintf(stderr, "unable to install atexit handler\n");
        break;
    default:
        fprintf(stderr, "unknown error\n");
    }
}

/*
 * Ensures the data is flushed back to disk when the program exits.
 */
void removeDevice() {
    if (munmap(deviceBlocks, NUMBLOCKS * BLOCK_SIZE) == -1) {
        perror("ERROR removing mmap");
    }
}

/*
 * Connects an area of memory with the file representing the device.
 * Only called once each time the program is run.
 */
int connectDevice() {
    int dev_fd;
    struct stat fileStatus;

    if ((dev_fd = open("device_file", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
        dev_errno = EOPENINGDEVICE;
        return -1;
    }
    if (fstat(dev_fd, &fileStatus)) {
        dev_errno = EOPENINGDEVICE;
        return -1;
    }
    int size = fileStatus.st_size;
    unsigned char data[BLOCK_SIZE]; // could be garbage until formatted
    while (size < NUMBLOCKS * BLOCK_SIZE) {
        if (write(dev_fd, data, BLOCK_SIZE) == -1) {
            dev_errno = EOPENINGDEVICE;
            return -1;
        }
        size += BLOCK_SIZE;
    }
    deviceBlocks = (block *)mmap(NULL, NUMBLOCKS * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
    if ((long)deviceBlocks == -1) {
        dev_errno = ECREATINGMMAP;
        return -1;
    }
    if (atexit(removeDevice)) {
        dev_errno = EINSTALLATEXIT;
        return -1;
    }
    return 0;
}

/*
 * Checks to see if the device is attached.
 * If not it tries to connect it.
 * Returns -1 if there is a problem, 0 otherwise.
 */
int badDevice() {
    static int unconnected = 1;
    if (unconnected) {
        if (connectDevice()) {
            return -1;
        }
        unconnected = 0;
    }
    return 0;
}

/*
 * Read a block of data from the device.
 * blockNumber - the number of the block to read
 * data - the address of the position to put the block data
 * The data must be exactly BLOCK_SIZE long.
 * Returns 0 if successful, -1 if an error occurred.
 */
int blockRead(int blockNumber, unsigned char *data) {
    if (badDevice())
        return -1;
    if (blockNumber < 0 || blockNumber > NUMBLOCKS - 1) {
        dev_errno = EBADBLOCK;
        return -1;
    }
    memcpy(data, deviceBlocks[blockNumber], BLOCK_SIZE);
    return 0;
}

/*
 * Write a block of data to the device.
 * blockNumber - the number of the block to write
 * data - the address of the position to get the block data
 * The data must be exactly BLOCK_SIZE long.
 * Returns 0 if successful, -1 if an error occurred.
 */
int blockWrite(int blockNumber, unsigned char *data) {
    displayBlock(blockNumber);
    if (badDevice())
        return -1;
    if (blockNumber < 0 || blockNumber > NUMBLOCKS - 1) {
        dev_errno = EBADBLOCK;
        return -1;
    }
    memcpy(deviceBlocks[blockNumber], data, BLOCK_SIZE);
    return 0;
}

/*
 * Reports the number of blocks in the device.
 */
int numBlocks() {
    return NUMBLOCKS;
}

/*
 * Displays the contents of a block on stdout.
 * Not really a device function, just for debugging and marking.
 */
void displayBlock(int blockNumber) {
    block b;
    if (blockRead(blockNumber, b)) {
        printDevError("-- displayBlock --");
    } else {
        printf("\nBlock %d\n", blockNumber);
        printf("==========\n");
        int row;
        int col;
        const int number_per_row = 16;
        unsigned char *byte = b;
        for (row = 0; row < BLOCK_SIZE / number_per_row; ++row) {
            unsigned char *byte2 = byte;
            for (col = 0; col < number_per_row; ++col) {
                printf("%02x ", *byte++);
            }
            printf("\t");
            for (col = 0; col < number_per_row; ++col) {
                unsigned char c = *byte2++;
                if (c < 32 || c > 126)
                    c = '-';
                printf("%c", c);
            }
            printf("\n");
        }
    }
}
