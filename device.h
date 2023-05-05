/*
 * device.h
 *
 *  Modified on: 24/03/2023
 *      Author: Robert Sheehan
 *
 *  N.B. You are not allowed to modify this file.
 */

#define BLOCK_SIZE 64

typedef unsigned char block[BLOCK_SIZE];

/*
 * Read a block of data from the device.
 * blockNumber - the number of the block to read
 * data - the address of the position to put the block data
 * The data must be exactly BLOCK_SIZE long.
 * Returns 0 if successful, -1 if an error occurred.
 */
int blockRead(int blockNumber, unsigned char *data);

/*
 * Write a block of data to the device.
 * blockNumber - the number of the block to write
 * data - the address of the position to get the block data
 * The data must be exactly BLOCK_SIZE long.
 * Returns 0 if successful, -1 if an error occurred.
 */
int blockWrite(int blockNumber, unsigned char *data);

/*
 *  Reports the number of blocks in the device.
 */
int numBlocks();

/*
 * Displays the contents of a block on stdout.
 * Not really a device function, just for debugging and marking.
 */
void displayBlock(int blockNumber);

/*
 * Prints the message and then information about the most recent device error.
 * You should call this whenever a device error is reported.
 */
void printDevError(char *message);
