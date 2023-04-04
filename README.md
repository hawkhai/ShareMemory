## sharememory

Windows 共享内存，跨进程内存读写，同步机制。

[TestFileLock.zip](https://www.codeproject.com/Articles/49670/Inter-Computer-Read-Write-File-Lock)
[共享内存，同步机制](https://sunocean.life/blog/blog/2022/05/31/Share-Momery)
<http://www.cnblogs.com/dongsheng/p/4460944.html>
<http://blog.chinaunix.net/uid-26833883-id-3230564.html>
<https://blog.csdn.net/shuilan0066/article/details/87979315>

Windows 下进程的地址空间在逻辑上是相互隔离的，但在物理上却是重叠的。所谓的重叠是指同一块内存区域可能被多个进程同时使用。
共享内存，各个进程可以共享同一块物理内存，进程可以直接读写内存，不需要数据拷贝。
由于多个进程共享一块内存，所以也需要同步机制。

1. 本进程创建
    1. 创建内存映射文件对象 CreateFileMapping，
        Windows 即在物理内存申请一块指定大小的内存区域，返回文件映射对象的句柄 hMap。
    2. 将内存对象映射在进程地址空间 MapViewOfFile。
2. 其它进程访问，其它进程访问这个内存对象时，
    1. OpenFileMapping 获取对象句柄。
    2. 调用 MapViewOfFile 映射在自己的进程空间。

完整代码：github.com/hawkhai/ShareMemory.git
共享内存，写入只要不释放，就可以实现跨进程读取，避免文件内容落地。
写入的函数负责共享内存块的创建。
这样，同一块内存块被映射到了不同的进程空间，从而达到多个进程共享同一个内存块的目的。

```cpp
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <assert.h>
#include "../ShareMemoryDll/ShareMemoryDll.h"

using namespace ShareMemoryDll;

#define SHARE_MEMORY_NAME L"SHARE_MEMORY_TEST"
#define SHARE_MEMORY_SIZE (1024*1024) // 1MB

void testread() {
    ShareMemoryRead sharememory(SHARE_MEMORY_NAME);
    if (!sharememory.check()) {
        return;
    }

    CShareMemoryCallback callback;
    ShareMemoryData* data = nullptr;
    int datasize = 0;
    if ((datasize = sharememory.read(data, &callback)) >= 1) {
        TestData* testData = (TestData*)&data[0];
    }
    if (data) {
        callback.free(data);
    }
}

void testwrite() {
    ShareMemoryWrite sharememory(SHARE_MEMORY_NAME, SHARE_MEMORY_SIZE);
    if (!sharememory.check()) {
        return;
    }

    TestData testData(true);
    sharememory.write((ShareMemoryData*)&testData, sizeof(testData));
}
```
