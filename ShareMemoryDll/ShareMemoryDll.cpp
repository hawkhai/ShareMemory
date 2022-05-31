#include "pch.h"
#include "ShareMemoryDll.h"
#include "../TestFileLock/RWFileLock.h"

using namespace ShareMemoryDll;

const int ShareMemory::getHeadSize() {
    return sizeof(ShareMemoryHeader);
}

const int ShareMemory::getTailSize() {
    return sizeof(ShareMemoryTail);
}

ShareMemoryHeader* ShareMemory::getMemoryHeader() {
    if (!m_pBuffer) {
        return nullptr;
    }
    ShareMemoryHeader* header = (ShareMemoryHeader*)m_pBuffer;
    // 估计是版本不对。
    if (header->headSize != getHeadSize() || header->tailSize != getTailSize()) {
        return nullptr;
    }
    return header;
}

int ShareMemory::getMemorySize() {
    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }
    return header->memorySize;
}

int ShareMemory::getContentSize() {
    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }
    return header->contentSize;
}

ShareMemoryData* ShareMemory::getContentPtr() {
    if (m_pBuffer == nullptr) {
        return nullptr;
    }
    return m_pBuffer + getHeadSize();
}

ShareMemory::ShareMemory(LPCWSTR lpName, bool write) : m_write(write) {
    assert(lpName);
    m_lpMapName = L"ShareMemoryMap-";
    m_sLockedFilePath = L"ShareMemoryLockedFile-";
    if (lpName) {
        m_lpMapName.append(lpName);
        m_sLockedFilePath.append(lpName);
    }
    m_sLockedFilePath.append(L".mdb");

    m_pReadFileLock = new NMt::CReadFileLock(m_sLockedFilePath.c_str());
    m_pWriteFileLock = new NMt::CWriteFileLock(m_sLockedFilePath.c_str());
}

ShareMemory::~ShareMemory() {
    if (m_pBuffer) {
        ::UnmapViewOfFile(m_pBuffer);
        m_pBuffer = nullptr;
    }
    if (m_hMap) {
        ::CloseHandle(m_hMap);
        m_hMap = nullptr;
    }
    delete m_pReadFileLock;
    delete m_pWriteFileLock;
}

ShareMemoryWrite::~ShareMemoryWrite() {
    int a = 0;
}

ShareMemoryWrite::ShareMemoryWrite(LPCWSTR lpName, int size) : ShareMemory(lpName, true) {
    assert(size >= 0);
    if (size < 0) {
        size = 0;
    }

    ShareMemoryHeader header;
    header.headSize = getHeadSize();
    header.contentSize = 0; // 初始为空。
    header.tailSize = getTailSize();
    header.memorySize = size + getHeadSize() + getTailSize();

    m_hMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, //
        NULL, //
        PAGE_READWRITE, //
        0, header.memorySize, //
        m_lpMapName.c_str());
    if (m_hMap == nullptr) {
        m_hMap = ::OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, m_lpMapName.c_str());
    }

    m_pBuffer = (ShareMemoryData*)::MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    assert(m_pBuffer);
    if (m_pBuffer) { // 把头刷进去。
        memcpy(m_pBuffer, &header, sizeof(header));
        ShareMemoryTail tail;
        tail = 0; // 把尾巴再刷进去。
        memcpy(getContentPtr(), &tail, sizeof(tail));
    }
}

int ShareMemoryWrite::writeImpl(ShareMemoryData* data, int size) {

    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }
    if (size > header->getMaxContentSize()) {
        size = header->getMaxContentSize(); // 截断写入。
    }

    assert(header == (ShareMemoryHeader*)m_pBuffer); // 断言直接指内存。
    header->contentSize = size;
    header->crcCheck = crc64(data, size, 0);
    memcpy(m_pBuffer, header, getHeadSize()); // 重新把头刷进去。
    memcpy(getContentPtr(), data, size);
    ShareMemoryTail tail;
    tail = 0; // 把尾巴再刷进去。
    memcpy(getContentPtr() + size, &tail, sizeof(tail));
    return size;
}

int ShareMemoryWrite::write(ShareMemoryData* data, int size) {
    if (!m_pBuffer || !data || size < 0) {
        return -1;
    }

    m_pWriteFileLock->Lock();

    int retv = writeImpl(data, size);

    m_pWriteFileLock->Unlock();
    return retv;
}

ShareMemoryRead::ShareMemoryRead(LPCWSTR lpName) : ShareMemory(lpName, false) {
    m_hMap = ::OpenFileMapping(FILE_MAP_READ, 0, m_lpMapName.c_str());
    m_pBuffer = (ShareMemoryData*)::MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
    assert(m_pBuffer);
}

ShareMemoryRead::~ShareMemoryRead() {

}

// Computes 64-bit "cyclic redundancy check" sum, as specified in ECMA-182
uint64 ShareMemory::crc64(const uchar* data, size_t size, uint64 crcx)
{
    static uint64 table[256];
    static bool initialized = false;

    if (!initialized)
    {
        for (int i = 0; i < 256; i++)
        {
            uint64 c = i;
            for (int j = 0; j < 8; j++)
                c = ((c & 1) ? CV_BIG_UINT(0xc96c5795d7870f42) : 0) ^ (c >> 1);
            table[i] = c;
        }
        initialized = true;
    }

    uint64 crc = ~crcx;
    for (size_t idx = 0; idx < size; idx++) {
        crc = table[(uchar)crc ^ data[idx]] ^ (crc >> 8);
    }
    return ~crc;
}

int ShareMemory::readImpl(ShareMemoryData*& data, IShareMemoryInterface* callback) {

    assert(data == nullptr);
    if (!m_pBuffer || !callback) {
        return -1;
    }

    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }

    int contentSize = header->contentSize;
    data = callback->alloc(contentSize + 1);
    memcpy(&data[0], getContentPtr(), contentSize);
    data[contentSize] = 0;
    auto crcCheck = crc64(data, contentSize, 0);
    printf("memorySize=%d contentSize=%d check=%d \r\n", //
        header->memorySize, header->contentSize,
        crcCheck == header->crcCheck);
    return contentSize;
}

int ShareMemory::read(ShareMemoryData*& data, IShareMemoryInterface* callback) {

    if (!m_pBuffer || !callback) {
        return -1;
    }

    m_pReadFileLock->Lock();

    int retv = readImpl(data, callback);

    m_pReadFileLock->Unlock();
    return retv;
}
