#include <windows.h>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <assert.h>

typedef unsigned char ShareMemoryData;
typedef unsigned char ShareMemoryTail;

struct ShareMemoryHeader {
    INT32 memorySize = 0; // 整个内存块的大小。
    INT32 headSize = 0; // 头的大小。
    INT32 contentSize = 0; // 内容的大小。可以变化的，不一定要写满。
    INT32 tailSize = 0; // 尾巴的大小。
    INT32 varReserved = 0; // 保留字段。

    INT32 getMaxContentSize() {
        return memorySize - headSize - tailSize;
    }
};

class ShareMemory {
protected:
    int getHeadSize() {
        return sizeof(ShareMemoryHeader);
    }

    int getTailSize() {
        return sizeof(ShareMemoryTail);
    }

    ShareMemoryHeader* getMemoryHeader() {
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

    int getMemorySize() {
        ShareMemoryHeader* header = getMemoryHeader();
        if (!header) {
            return -1;
        }
        return header->memorySize;
    }

    int getContentSize() {
        ShareMemoryHeader* header = getMemoryHeader();
        if (!header) {
            return -1;
        }
        return header->contentSize;
    }

    ShareMemoryData* getContentPtr() {
        if (m_pBuffer == nullptr) {
            return nullptr;
        }
        return m_pBuffer + getHeadSize();
    }

public:
    ShareMemory(LPCWSTR lpName, bool write, int size) : m_write(write), m_size(size) {
        assert(lpName);
        if (lpName) {
            m_lpMapName.append(lpName);
            m_lpReadEventName.append(lpName);
            m_lpWriteEventName.append(lpName);
        }
    }

    virtual ~ShareMemory() {
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

protected:
    std::wstring m_lpReadEventName = L"Global\\ShareMemoryReadEvent-";
    std::wstring m_lpWriteEventName = L"Global\\ShareMemoryWriteEvent-";
    std::wstring m_lpMapName = L"ShareMemoryMap-";
    const bool m_write;
    const int m_size;
    HANDLE m_hReadEvent = nullptr;
    HANDLE m_hWriteEvent = nullptr;
    HANDLE m_hMap = nullptr;
    ShareMemoryData* m_pBuffer = nullptr; // LPVOID
};

class ShareMemoryWrite : public ShareMemory {
public:
    ShareMemoryWrite(LPCWSTR lpName, int size) : ShareMemory(lpName, true, size) {
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
        header.contentSize = 0; // 初始为空。
        header.tailSize = getTailSize();
        header.memorySize = m_size + getHeadSize() + getTailSize();

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

private:
    int writeImpl(ShareMemoryData* data, int size) {

        if (size > m_size) {
            size = m_size; // 截断写入。
        }

        ShareMemoryHeader* header = getMemoryHeader();
        if (!header) {
            return -1;
        }
        if (size > header->getMaxContentSize()) {
            size = header->getMaxContentSize(); // 截断写入。
        }

        assert(header == (ShareMemoryHeader*)m_pBuffer); // 断言直接指内存。
        header->contentSize = size;
        memcpy(getContentPtr(), data, size);
        ShareMemoryTail tail;
        tail = 0; // 把尾巴再刷进去。
        memcpy(getContentPtr() + size, &tail, sizeof(tail));
    }

public:
    int write(ShareMemoryData* data, int size) {
        if (!m_pBuffer || !data || size < 0) {
            return -1;
        }

        WaitForSingleObject(m_hReadEvent, INFINITE);
        ResetEvent(m_hWriteEvent);

        int retv = writeImpl(data, size);

        SetEvent(m_hWriteEvent);
        return retv;
    }
};

class ShareMemoryRead : public ShareMemory {
public:
    ShareMemoryRead(LPCWSTR lpName) : ShareMemory(lpName, false, -1) {
        // dwDesiredAccess, bInheritHandle, lpName
        m_hReadEvent = OpenEvent(EVENT_MODIFY_STATE, TRUE, m_lpReadEventName.c_str());
        m_hWriteEvent = OpenEvent(EVENT_MODIFY_STATE, TRUE, m_lpWriteEventName.c_str());

        m_hMap = ::OpenFileMapping(FILE_MAP_READ, 0, m_lpMapName.c_str());
        m_pBuffer = (ShareMemoryData*)::MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
        assert(m_pBuffer);
    }

private:
    int readImpl(std::vector<ShareMemoryData>& data) {

        ShareMemoryHeader* header = getMemoryHeader();
        if (!header) {
            return -1;
        }

        data.resize(header->contentSize);
        memcpy(&data[0], getContentPtr(), header->contentSize);
    }

public:
    int readdata(std::vector<ShareMemoryData>& data) {

        if (!m_pBuffer) {
            return -1;
        }

        WaitForSingleObject(m_hWriteEvent, INFINITE);
        ResetEvent(m_hReadEvent);

        int retv = readImpl(data);

        SetEvent(m_hReadEvent);
        return retv;
    }
};

int main()
{
    getchar();
    system("pause");
    return 0;
}