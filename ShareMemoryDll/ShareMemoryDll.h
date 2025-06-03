#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <assert.h>

namespace NMt {
    class CReadFileLock;
    class CWriteFileLock;
};

#ifdef SHAREMEMORYDLL_EXPORTS
#define SHARE_MEMORY_DLLEXPORT __declspec(dllexport)
#else
#define SHARE_MEMORY_DLLEXPORT //__declspec(dllimport)
#endif

namespace ShareMemoryDll
{
    typedef uint32_t        uint;
    typedef signed char     schar;
    typedef unsigned char   uchar;
    typedef unsigned short  ushort;
    typedef int64_t         int64;
    typedef uint64_t        uint64;
#define CV_BIG_INT(n)   n##LL
#define CV_BIG_UINT(n)  n##ULL

    typedef unsigned char ShareMemoryData;

    struct ShareMemoryHeader {
        INT32 memorySize = 0; // 整个内存块的大小。
        INT32 headSize = 0; // 头的大小。
        INT32 contentSize = 0; // 内容的大小。可以变化的，不一定要写满。
        UINT64 crcCheck = 0;
        INT32 varReserved = 0; // 保留字段。

        INT32 getMaxContentSize() {
            return memorySize - headSize;
        }
    };

    __interface IShareMemoryInterface
    {
        virtual ShareMemoryData* alloc(int size) = 0;
        virtual void free(ShareMemoryData* data) = 0;
    };

    class CShareMemoryCallback : public IShareMemoryInterface {
    public:
        virtual ShareMemoryData* alloc(int size) {
            auto retv = new ShareMemoryData[size + 1]; // 多弄一个。
            retv[size] = 0;
            return retv;
        }
        virtual void free(ShareMemoryData* data) {
            delete[] data;
        }
    };

    class SHARE_MEMORY_DLLEXPORT ShareMemory {
    protected:
        const int getHeadSize();

        ShareMemoryHeader* getMemoryHeader();

        int getMemorySize();

        int getContentSize();

        ShareMemoryData* getContentPtr();

    public:
        ShareMemory(LPCWSTR lpName, bool write);

        virtual ~ShareMemory();

    private:
        int readImpl(ShareMemoryData*& data, IShareMemoryInterface* callback);

    public:
        int read(ShareMemoryData*& data, IShareMemoryInterface* callback);
        // Computes 64-bit "cyclic redundancy check" sum, as specified in ECMA-182
        static uint64 crc64(const uchar* data, size_t size, uint64 crcx);

        bool check() {
            return m_pReadFileLock && m_pWriteFileLock && m_hMap && m_pBuffer;
        }

    protected:
        std::wstring m_sLockedFilePath = L"ShareMemoryLockedFile-.mdb";
        std::wstring m_lpMapName = L"ShareMemoryMap-";
        bool m_write = false;
        NMt::CReadFileLock* m_pReadFileLock = nullptr;
        NMt::CWriteFileLock* m_pWriteFileLock = nullptr;
        HANDLE m_hMap = nullptr;
        ShareMemoryData* m_pBuffer = nullptr; // LPVOID
    };

    class SHARE_MEMORY_DLLEXPORT ShareMemoryWrite : public ShareMemory {
    public:
        ShareMemoryWrite(LPCWSTR lpName, int size);
        ~ShareMemoryWrite();
        int getSize() {
            return m_size;
        }
        
    private:
        int writeImpl(ShareMemoryData* data, int size);

    public:
        int write(ShareMemoryData* data, int size);

    private:
        int m_size = 0;
    };

    class SHARE_MEMORY_DLLEXPORT ShareMemoryRead : public ShareMemory {
    public:
        ShareMemoryRead(LPCWSTR lpName);
        ~ShareMemoryRead();
    };
};
