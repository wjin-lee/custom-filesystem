/*
 * beforeTest.c
 *
 *  Modified on: 24/03/2023
 *      Author: Robert Sheehan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileSystem.h"

int main(void) {
	format("Permanent Data");
	create("/fileA");
	a2write("/fileA", "aaaaa", 6);
	create("/fileB");
	a2write("/fileB", "bbbbb", 6);
	create("/dir1/fileA");
	a2write("/dir1/fileA", "1a1a1a", 7);
	char listResult[1024];
	list(listResult, "/");
	printf("%s\n", listResult);
	list(listResult, "/dir1");
	printf("%s\n", listResult);
	return EXIT_SUCCESS;
}
