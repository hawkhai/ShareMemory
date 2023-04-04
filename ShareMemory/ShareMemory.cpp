#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include "../ShareMemoryDll/ShareMemoryDll.h"

using namespace ShareMemoryDll;

#define SHARE_MEMORY_NAME L"SHARE_MEMORY_TEST"
#define SHARE_MEMORY_SIZE (1024*1024) // 1MB
#define TEST_DATA_SIZE 100

struct TestData {
    char buffer[TEST_DATA_SIZE] = { 0 };
    uint64 crc = 0;

    TestData(bool init) {
        if (init) {
            for (int i = 0; i < TEST_DATA_SIZE; i++) {
                buffer[i] = rand();
            }
            crc = ShareMemory::crc64((const uchar*)buffer, TEST_DATA_SIZE, 0);
        }
    }
};

void testread() {
    printf("testread \r\n");
    ShareMemoryRead sharememory(SHARE_MEMORY_NAME);
    if (!sharememory.check()) {
        return;
    }

    CShareMemoryCallback callback;
    for (int i = 0; i < 1000 * 60; i++) {
        Sleep(1); // dwMilliseconds

        ShareMemoryData* data = nullptr;
        int datasize = 0;
        if ((datasize = sharememory.read(data, &callback)) >= 1) {
            TestData* testData = (TestData*)&data[0];
            uint64 crc = sharememory.crc64((const uchar*)testData->buffer, TEST_DATA_SIZE, 0);
            assert(crc == testData->crc);
            assert(datasize == sizeof(TestData));
            printf("read %d. %llu \r\n", i, crc);
        }
        if (data) {
            callback.free(data);
        }
    }
}

void testwrite() {
    printf("testwrite \r\n");
    ShareMemoryWrite sharememory(SHARE_MEMORY_NAME, SHARE_MEMORY_SIZE);
    if (!sharememory.check()) {
        return;
    }

    for (int i = 0; i < 1000 * 60; i++) {
        Sleep(1); // dwMilliseconds

        TestData testData(true);
        sharememory.write((ShareMemoryData*)&testData, sizeof(testData));
        printf("write %d. %llu \r\n", i, testData.crc);
    }
}

int main(int argc, char* argv[])
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "read") == 0) {
            testread();
            break;
        }
        if (strcmp(argv[i], "write") == 0) {
            testwrite();
            break;
        }
    }
    // getchar();
    system("pause");
    return 0;
}