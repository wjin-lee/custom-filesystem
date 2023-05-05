/*
 * display.c
 *
 *  Modified on: 23/03/2023
 *      Author: Robert Sheehan
 *
 *      This is used to display the blocks in the device.
 *      If run from the command line without a parameter it
 *      displays all blocks in the device.
 *      If run with an integer parameter it shows that many
 *      blocks from the device.
 */

#include <stdlib.h>
#include <stdio.h>
#include "device.h"

int main(int argc, char *argv[]) {
	int i;
	int numBlocksToDisplay;
	if (argc < 2) {
		numBlocksToDisplay = numBlocks();
	} else {
		numBlocksToDisplay = atoi(argv[1]);
		if (numBlocksToDisplay < 1 || numBlocksToDisplay > numBlocks())
			numBlocksToDisplay = numBlocks();
	}
    for(i = 0; i < numBlocksToDisplay; ++i){
        displayBlock(i);
    }
    return EXIT_SUCCESS;
}
