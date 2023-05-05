/*
 * test.c
 *
 *  Modified on: 24/03/2023
 *      Author: Robert Sheehan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CuTest.h"
#include "fileSystem.h"

void TestFormat(CuTest *tc) {
    char volName[64];

    char *justRight = "1--------10--------20--------30--------40--------50--------60--";
    CuAssertIntEquals(tc, 0, format(justRight));
    CuAssertIntEquals(tc, 0, volumeName(volName));
    CuAssertStrEquals(tc, justRight, volName);

    char *normal = "regular volume name";
    CuAssertIntEquals(tc, 0, format(normal));
    CuAssertIntEquals(tc, 0, volumeName(volName));
    CuAssertStrEquals(tc, normal, volName);
}

void TestRootDirectory(CuTest *tc) {
    CuAssertIntEquals(tc, 0, format("test root"));
    char listResult[1024];
    char *expectedResult = "/:\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestCreateFile(CuTest *tc) {
    format("test create file");
    CuAssertIntEquals(tc, 0, create("/fileA"));
    char listResult[1024];
    char *expectedResult = "/:\nfileA:\t0\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestCreateFiles(CuTest *tc) {
    format("test create files");
    CuAssertIntEquals(tc, 0, create("/fileA"));
    CuAssertIntEquals(tc, 0, create("/fileB"));
    CuAssertIntEquals(tc, 0, create("/fileC"));
    char listResult[1024];
    char *expectedResult = "/:\nfileA:\t0\nfileB:\t0\nfileC:\t0\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestCreateLotsOfFiles(CuTest *tc) {
    format("test create files");
    CuAssertIntEquals(tc, 0, create("/fileA"));
    CuAssertIntEquals(tc, 0, create("/fileB"));
    CuAssertIntEquals(tc, 0, create("/fileC"));
    CuAssertIntEquals(tc, 0, create("/fileD"));
    CuAssertIntEquals(tc, 0, create("/fileE"));
    CuAssertIntEquals(tc, 0, create("/fileF"));
    CuAssertIntEquals(tc, 0, create("/fileG"));
    CuAssertIntEquals(tc, 0, create("/fileH"));
    char listResult[1024];
    char *expectedResult = "/:\nfileA:\t0\nfileB:\t0\nfileC:\t0\nfileD:\t0\nfileE:\t0\n"
                           "fileF:\t0\nfileG:\t0\nfileH:\t0\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestCreateFileWithDir(CuTest *tc) {
    format("test file with dir");
    CuAssertIntEquals(tc, 0, create("/dir1/fileA"));
    char listResult[1024];
    char *expectedResult = "/dir1:\nfileA:\t0\n";
    list(listResult, "/dir1");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestCreateFilesWithDir(CuTest *tc) {
    format("test file with dir");
    CuAssertIntEquals(tc, 0, create("/dir1/fileA"));
    CuAssertIntEquals(tc, 0, create("/dir1/fileB"));
    char listResult[1024];
    char *expectedResult = "/dir1:\nfileA:\t0\nfileB:\t0\n";
    list(listResult, "/dir1");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestWriteFile(CuTest *tc) {
    format("test file write");
    create("/fileA");
    char listResult[1024];
    CuAssertIntEquals(tc, 0, a2write("/fileA", "hi ", 4));
    char *expectedResult = "/:\nfileA:\t4\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
    char *data = "1--------10--------20--------30--------40--------50--------60--";
    CuAssertIntEquals(tc, 0, a2write("/fileA", data, 64));
    expectedResult = "/:\nfileA:\t68\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestWriteNulls(CuTest *tc) {
    format("test file write nulls");
    create("/fileA");
    char listResult[1024];
    CuAssertIntEquals(tc, 0, a2write("/fileA", "hi ", 4));
    char *expectedResult = "/:\nfileA:\t4\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
    char *data = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
    CuAssertIntEquals(tc, 0, a2write("/fileA", data, 64));
    create("/fileB");
    char readData[65];
    expectedResult = readData;
    memcpy(expectedResult, data, 64);
    char readResult[128];
    CuAssertIntEquals(tc, 0, a2write("/fileB", data, 64));
    CuAssertIntEquals(tc, 0, a2read("/fileB", readResult, 64));
    CuAssertStrEquals(tc, expectedResult, readResult);
    expectedResult = "/:\nfileA:\t68\nfileB:\t64\n";
    list(listResult, "/");
    CuAssertStrEquals(tc, expectedResult, listResult);
}

void TestReallyBigWrite(CuTest *tc) {
    CuAssertIntEquals(tc, 0, format("test file big write"));
    char volName[64];
    create("/direct7/direct8/direct7/direct7/WeirdFi");
    char *data = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    CuAssertIntEquals(tc, 0, a2write("/direct7/direct8/direct7/direct7/WeirdFi", data, 3200));
    char *readResult = malloc(3200);
    CuAssertIntEquals(tc, 0, a2read("/direct7/direct8/direct7/direct7/WeirdFi", readResult, 3200));
    char *expectedResult = malloc(3200);
    memcpy(expectedResult, data, 3200);
    CuAssertStrEquals(tc, expectedResult, readResult);

    CuAssertIntEquals(tc, 0, volumeName(volName));

    char *listResultExpected = "/direct7/direct8/direct7/direct7:\nWeirdFi:\t3200\n";
    char listResult[1024];
    list(listResult, "/direct7/direct8/direct7/direct7");
    CuAssertStrEquals(tc, listResultExpected, listResult);
    free(expectedResult);
}

void TestReadFile(CuTest *tc) {
    format("test file read");
    create("/fileA");
    a2write("/fileA", "hi ", 4);
    char *data = "1--------10--------20--------30--------40--------50--------60--";
    a2write("/fileA", data, 64);
    char *expectedResult = "hi ";
    char readResult[128];
    CuAssertIntEquals(tc, 0, a2read("/fileA", readResult, 4));
    CuAssertStrEquals(tc, expectedResult, readResult);
    char readData[128];
    expectedResult = readData;
    strncpy(expectedResult, data, 128);
    CuAssertIntEquals(tc, 0, a2read("/fileA", readResult, 64));
    CuAssertStrEquals(tc, expectedResult, readResult);
}

void TestReadLotsOfFiles(CuTest *tc) {
    format("test lots of reads");
    create("/fileA");
    create("/fileB");
    a2write("/fileA", "I am in fileA", 14);
    a2write("/fileB", "Whereas I am in fileB", 22);
    char readResult[128];
    char *expectedResult = "I am";
    CuAssertIntEquals(tc, 0, a2read("/fileA", readResult, 4));
    readResult[4] = '\0';
    CuAssertStrEquals(tc, expectedResult, readResult);

    expectedResult = "Whereas";
    CuAssertIntEquals(tc, 0, a2read("/fileB", readResult, 7));
    readResult[7] = '\0';
    CuAssertStrEquals(tc, expectedResult, readResult);

    expectedResult = " in fileA";
    CuAssertIntEquals(tc, 0, a2read("/fileA", readResult, 9));
    readResult[9] = '\0';
    CuAssertStrEquals(tc, expectedResult, readResult);

    expectedResult = " I am";
    CuAssertIntEquals(tc, 0, a2read("/fileB", readResult, 5));
    readResult[5] = '\0';
    CuAssertStrEquals(tc, expectedResult, readResult);
}

void TestWriteAndReadWithDirectories(CuTest *tc) {
    format("test write and read with directories");
    create("/dir1/dir2/fileA");
    a2write("/dir1/dir2/fileA", "I am in /dir1/dir2/fileA", 25);
    char listResult[1024];
    char *expectedResult = "/dir1/dir2:\nfileA:\t25\n";
    list(listResult, "/dir1/dir2");
    CuAssertStrEquals(tc, expectedResult, listResult);
    char readResult[128];
    expectedResult = "I am in /dir1/dir2/fileA";
    CuAssertIntEquals(tc, 0, a2read("/dir1/dir2/fileA", readResult, 25));
    CuAssertStrEquals(tc, expectedResult, readResult);
}

void TestSeek(CuTest *tc) {
    format("test seek");
    create("/fileA");
    a2write("/fileA", "aaaaabbbbbwhereddddd", 21);
    CuAssertIntEquals(tc, 0, seek("/fileA", 10));
    char *expectedResult = "where";
    char readResult[128];
    CuAssertIntEquals(tc, 0, a2read("/fileA", readResult, 5));
    readResult[5] = '\0';
    CuAssertStrEquals(tc, expectedResult, readResult);
}
