/*
 * afterTest.c
 *
 *  Modified on: 24/03/2023
 *      Author: Robert Sheehan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileSystem.h"

int main(void) {
	char data[128];
	a2read("/fileA", data, 6);
	printf("Data from /fileA: %s\n", data);

	a2read("/dir1/fileA", data, 7);
	printf("Data from /dir1/fileA: %s\n", data);

	char listResult[1024];
	list(listResult, "/");
	printf("\n%s", listResult);
	list(listResult, "/dir1");
	printf("\n%s", listResult);
	return EXIT_SUCCESS;
}
