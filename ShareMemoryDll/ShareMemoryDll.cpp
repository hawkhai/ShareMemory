#include "pch.h"
#include "ShareMemoryDll.h"
#include "../TestFileLock/RWFileLock.h"
#include <Winbase.h>
#include <shlwapi.h>

using namespace ShareMemoryDll;

const int ShareMemory::getHeadSize() {
    return sizeof(ShareMemoryHeader);
}

ShareMemoryHeader* ShareMemory::getMemoryHeader() {
    if (!m_pBuffer) {
        return nullptr;
    }
    ShareMemoryHeader* header = (ShareMemoryHeader*)m_pBuffer;
    // 估计是版本不对。
    if (header->headSize != getHeadSize()) {
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

// 获取系统启动时间作为前缀
UINT64 ShareMemory::GetBootTime() {
    ULONGLONG uptime = GetTickCount64(); // 获取系统启动后经过的毫秒数
    ULONGLONG bootTime = (ULONGLONG)(time(nullptr)) * 1000 - uptime; // 当前时间 - 运行时间 = 启动时间
    return bootTime;
}

// 清理旧的文件锁（不是当前启动时间的）
void ShareMemory::CleanupOldLockFiles(const std::wstring& prefix, const std::wstring& suffixPattern) {
    static bool firstRun = true;
    
    // 仅在第一次运行时清理
    if (!firstRun) {
        return;
    }
    firstRun = false;
    
    ULONGLONG currentBootTime = GetBootTime();
    std::wstring currentPrefix = std::to_wstring(currentBootTime) + L"_";
    
    TCHAR tempPath[MAX_PATH + 1] = { 0 };
    DWORD res = GetTempPath(MAX_PATH, tempPath);
    if (res > 0 && res < MAX_PATH) {
        if (tempPath[_tcslen(tempPath) - 1] != L'\\') {
            _tcscat_s(tempPath, MAX_PATH, L"\\");
        }
        _tcscat_s(tempPath, MAX_PATH, L"rwfilelock\\");
        
        // 查找所有匹配的文件
        WIN32_FIND_DATA findFileData;
        std::wstring searchPattern = std::wstring(tempPath) + L"*" + prefix + suffixPattern + L"*.rlc";
        
        HANDLE hFind = FindFirstFile(searchPattern.c_str(), &findFileData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                std::wstring fileName = findFileData.cFileName;
                
                // 检查文件名是否以数字和下划线开头
                bool hasBootTimePrefix = false;
                size_t underscorePos = fileName.find(L"_");
                
                if (underscorePos != std::wstring::npos) {
                    std::wstring bootTimeStr = fileName.substr(0, underscorePos);
                    // 检查是否为数字
                    if (!bootTimeStr.empty() && bootTimeStr.find_first_not_of(L"0123456789") == std::wstring::npos) {
                        hasBootTimePrefix = true;
                        // 如果不是当前启动时间，删除文件
                        if (bootTimeStr != std::to_wstring(currentBootTime)) {
                            std::wstring fullPath = std::wstring(tempPath) + fileName;
                            DeleteFile(fullPath.c_str());
                            
                            // 同时删除对应的 .wlc 文件
                            std::wstring wlcFile = fullPath;
                            wlcFile.replace(wlcFile.rfind(L".rlc"), 4, L".wlc");
                            DeleteFile(wlcFile.c_str());
                        }
                    }
                }
                
                // 如果是旧格式文件（没有启动时间前缀）也删除
                if (!hasBootTimePrefix && fileName.find(prefix) != std::wstring::npos) {
                    std::wstring fullPath = std::wstring(tempPath) + fileName;
                    DeleteFile(fullPath.c_str());
                    
                    // 同时删除对应的 .wlc 文件
                    std::wstring wlcFile = fullPath;
                    wlcFile.replace(wlcFile.rfind(L".rlc"), 4, L".wlc");
                    DeleteFile(wlcFile.c_str());
                }
                
            } while (FindNextFile(hFind, &findFileData));
            
            FindClose(hFind);
        }
    }
}

ShareMemory::ShareMemory(LPCWSTR lpName, bool write) : m_write(write) {
    assert(lpName);
    m_lpMapName = L"SMM-";
    
    // 获取系统启动时间作为前缀
    ULONGLONG bootTime = GetBootTime();
    std::wstring bootTimePrefix = std::to_wstring(bootTime) + L"_";
    
    // 将启动时间添加到锁文件名前缀
    m_sLockedFilePath = bootTimePrefix + L"SMLF-";
    if (lpName) {
        m_lpMapName.append(lpName);
        m_sLockedFilePath.append(lpName);
    }
    m_sLockedFilePath.append(L".mdb");
    
    static bool first = true;
    if (first) {
        first = false;
        // 清理旧的锁文件
        CleanupOldLockFiles(L"SMLF-", lpName ? std::wstring(lpName) : L"");
    }

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
    this->m_size = size;
    ShareMemoryHeader header;
    header.headSize = getHeadSize();
    header.contentSize = 0; // 初始为空。
    header.memorySize = size + getHeadSize();

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
    }
}

int ShareMemoryWrite::writeImpl(ShareMemoryData* data, int size) {

    ShareMemoryHeader* header = getMemoryHeader();
    if (!header) {
        return -1;
    }
    if (size > header->getMaxContentSize()) {
        assert(false);
        size = header->getMaxContentSize(); // 截断写入。
    }

    assert(header == (ShareMemoryHeader*)m_pBuffer); // 断言直接指内存。
    header->contentSize = size;
    header->crcCheck = crc64(data, size, 0);
    memcpy(m_pBuffer, header, getHeadSize()); // 重新把头刷进去。
    memcpy(getContentPtr(), data, size);
    return size;
}

int ShareMemoryWrite::write(ShareMemoryData* data, int size) {
    if (!m_pBuffer || !data || size < 0) {
        return -1;
    }

    m_pWriteFileLock->Lock();
    assert(m_pWriteFileLock->isLocked());

    if (size > m_size) {
        // 超出了。
        assert(false);
    }

    int retv = writeImpl(data, size);

    m_pWriteFileLock->Unlock();
    return retv;
}

ShareMemoryRead::ShareMemoryRead(LPCWSTR lpName) : ShareMemory(lpName, false) {
    m_hMap = ::OpenFileMapping(FILE_MAP_READ, 0, m_lpMapName.c_str());
    m_pBuffer = (ShareMemoryData*)::MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
    //assert(m_pBuffer);
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
    //printf("memorySize=%d contentSize=%d check=%d \r\n", //
    //    header->memorySize, header->contentSize,
    //    crcCheck == header->crcCheck);
    if (crcCheck != header->crcCheck) {
        return -1;
    }
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
