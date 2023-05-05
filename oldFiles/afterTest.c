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
    // printFat();
    test();
    char data[128];
    // a2read("/fileA", data, 6);
    // printf("Data from /fileA: %s\n", data);

    // a2read("/dir1/fileA", data, 7);
    // printf("Data from /dir1/fileA: %s\n", data);

    // char listResult[1024];
    // list(listResult, "/");
    // printf("\n%s", listResult);
    // list(listResult, "/dir1");
    // printf("\n%s", listResult);

    // char volName[64];
    // volumeName(volName);
    // printf("\n%s\n", volName);

    // char *readResult = malloc(3200);
    // a2read("/direct7/direct8/direct7/direct7/WeirdFi", readResult, 3200);

    // printf("\n%s\n", readResult);

    // list(listResult, "/direct7/direct8/direct7/direct7");
    // printf("\n%s\n", listResult);
    return EXIT_SUCCESS;
}
