

/* This is auto-generated code. Edit at your own peril. */
#include <stdio.h>
#include <stdlib.h>

#include "CuTest.h"

extern void TestFormat(CuTest *);
extern void TestRootDirectory(CuTest *);
extern void TestCreateFile(CuTest *);
extern void TestCreateFiles(CuTest *);
extern void TestCreateLotsOfFiles(CuTest *);
extern void TestCreateFileWithDir(CuTest *);
extern void TestCreateFilesWithDir(CuTest *);
extern void TestWriteFile(CuTest *);
extern void TestWriteNulls(CuTest *);
extern void TestReallyBigWrite(CuTest *);
extern void TestReadFile(CuTest *);
extern void TestReadLotsOfFiles(CuTest *);
extern void TestWriteAndReadWithDirectories(CuTest *);
extern void TestSeek(CuTest *);

void RunAllTests(void) {
    CuString *output = CuStringNew();
    CuSuite *suite = CuSuiteNew();

    SUITE_ADD_TEST(suite, TestFormat);
    SUITE_ADD_TEST(suite, TestRootDirectory);
    SUITE_ADD_TEST(suite, TestCreateFile);
    SUITE_ADD_TEST(suite, TestCreateFiles);
    SUITE_ADD_TEST(suite, TestCreateLotsOfFiles);
    SUITE_ADD_TEST(suite, TestCreateFileWithDir);
    SUITE_ADD_TEST(suite, TestCreateFilesWithDir);
    SUITE_ADD_TEST(suite, TestWriteFile);
    SUITE_ADD_TEST(suite, TestWriteNulls);
    SUITE_ADD_TEST(suite, TestReallyBigWrite);
    SUITE_ADD_TEST(suite, TestReadFile);
    SUITE_ADD_TEST(suite, TestReadLotsOfFiles);
    SUITE_ADD_TEST(suite, TestWriteAndReadWithDirectories);
    SUITE_ADD_TEST(suite, TestSeek);

    CuSuiteRun(suite);
    CuSuiteSummary(suite, output);
    CuSuiteDetails(suite, output);
    printf("%s\n", output->buffer);
    CuStringDelete(output);
    CuSuiteDelete(suite);
}

int main(void) {
    RunAllTests();
}
