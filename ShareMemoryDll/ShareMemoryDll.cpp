#include "pch.h"
#include "ShareMemoryDll.h"

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
    // �����ǰ汾���ԡ�
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
    if (lpName) {
        m_lpMapName.append(lpName);
        m_lpReadEventName.append(lpName);
        m_lpWriteEventName.append(lpName);
    }
}

ShareMemory::~ShareMemory() {
    if (m_hReadEvent) {
        CloseHandle(m_hReadEvent);
        m_hReadEvent = nullptr;
    }
    if (m_hWriteEvent) {
        CloseHandle(m_hWriteEvent);
        m_hWriteEvent = nullptr;
    }
    if (m_pBuffer) {
        ::UnmapViewOfFile(m_pBuffer);
        m_pBuffer = nullptr;
    }
    if (m_hMap) {
        ::CloseHandle(m_hMap);
        m_hMap = nullptr;
    }
}


ShareMemoryWrite::ShareMemoryWrite(LPCWSTR lpName, int size) : ShareMemory(lpName, true) {
    assert(size >= 0);
    if (size < 0) {
        size = 0;
    }
    // lpEventAttributes, bManualReset, bInitialState, lpName
    m_hReadEvent = CreateEvent(NULL, TRUE, TRUE, m_lpReadEventName.c_str());
    if (m_hReadEvent == nullptr) {
        // dwDesiredAccess, bInheritHandle, lpName
        m_hReadEvent = OpenEvent(EVENT_MODIFY_STATE, TRUE, m_lpReadEventName.c_str());
    }
    m_hWriteEvent = CreateEvent(NULL, TRUE, TRUE, m_lpWriteEventName.c_str());
    if (m_hWriteEvent == nullptr) {
        m_hWriteEvent = OpenEvent(EVENT_MODIFY_STATE, TRUE, m_lpWriteEventName.c_str());
    }

    ShareMemoryHeader header;
    header.headSize = getHeadSize();
    header.contentSize = 0; // ��ʼΪ�ա�
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
    if (m_pBuffer) { // ��ͷˢ��ȥ��
        memcpy(m_pBuffer, &header, sizeof(header));
        ShareMemoryTail tail;
        tail = 0; // ��β����ˢ��ȥ��
        memcpy(getContentPtr(), &tail, sizeof(tail));
    }
}

int ShareMemoryWrite::writeImpl(ShareMemoryData* data, int size) {

    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }
    if (size > header->getMaxContentSize()) {
        size = header->getMaxContentSize(); // �ض�д�롣
    }

    assert(header == (ShareMemoryHeader*)m_pBuffer); // ����ֱ��ָ�ڴ档
    header->contentSize = size;
    memcpy(getContentPtr(), data, size);
    ShareMemoryTail tail;
    tail = 0; // ��β����ˢ��ȥ��
    memcpy(getContentPtr() + size, &tail, sizeof(tail));
    return size;
}

int ShareMemoryWrite::write(ShareMemoryData* data, int size) {
    if (!m_pBuffer || !data || size < 0) {
        return -1;
    }

    WaitForSingleObject(m_hReadEvent, INFINITE);
    ResetEvent(m_hWriteEvent);

    int retv = writeImpl(data, size);

    SetEvent(m_hWriteEvent);
    return retv;
}

ShareMemoryRead::ShareMemoryRead(LPCWSTR lpName) : ShareMemory(lpName, false) {
    // dwDesiredAccess, bInheritHandle, lpName
    m_hReadEvent = OpenEvent(EVENT_MODIFY_STATE, TRUE, m_lpReadEventName.c_str());
    m_hWriteEvent = OpenEvent(EVENT_MODIFY_STATE, TRUE, m_lpWriteEventName.c_str());

    m_hMap = ::OpenFileMapping(FILE_MAP_READ, 0, m_lpMapName.c_str());
    m_pBuffer = (ShareMemoryData*)::MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
    assert(m_pBuffer);
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

int ShareMemory::readImpl(std::vector<ShareMemoryData>& data) {

    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }

    data.resize(header->contentSize + 1);
    memcpy(&data[0], getContentPtr(), header->contentSize);
    data[header->contentSize] = 0;
    return header->contentSize;
}

int ShareMemory::read(std::vector<ShareMemoryData>& data) {

    if (!m_pBuffer) {
        return -1;
    }

    WaitForSingleObject(m_hWriteEvent, INFINITE);
    ResetEvent(m_hReadEvent);

    int retv = readImpl(data);

    SetEvent(m_hReadEvent);
    return retv;
}