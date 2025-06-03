#include "pch.h"
#include "ShareMemoryDll.h"
#include "../TestFileLock/RWFileLock.h"
#include <shlwapi.h>
#include <string>

using namespace ShareMemoryDll;


// 全局锁文件注册表和管理函数
namespace {
    // 使用进程内单例管理锁文件引用计数
    class LockFileRegistry {
    public:
        static LockFileRegistry& getInstance() {
            static LockFileRegistry instance;
            return instance;
        }

        // 注册锁文件
        void registerFile(const std::wstring& filePath) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lockFiles[filePath]++;
        }

        // 取消注册锁文件
        void unregisterFile(const std::wstring& filePath) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_lockFiles.find(filePath);
            if (it != m_lockFiles.end()) {
                if (--it->second <= 0) {
                    m_lockFiles.erase(it);
                    cleanupLockFile(filePath);
                }
            }
        }

        // 清理所有不再使用的锁文件
        void cleanupStaleFiles() {
            try {
                TCHAR tempPath[MAX_PATH] = { 0 };
                GetTempPath(MAX_PATH, tempPath);
                std::wstring lockBasePath = tempPath;
                if (lockBasePath.back() != L'\\') {
                    lockBasePath += L"\\";
                }
                lockBasePath += L"rwfilelock\\";

                // 确保目录存在
                if (!PathFileExists(lockBasePath.c_str())) {
                    return;
                }

                WIN32_FIND_DATA findData;
                HANDLE hFind = FindFirstFile((lockBasePath + L"*.wlc").c_str(), &findData);

                if (hFind != INVALID_HANDLE_VALUE) {
                    do {
                        // 跳过目录
                        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                            continue;
                        }

                        std::wstring fullPath = lockBasePath + findData.cFileName;

                        // 检查文件是否被使用中
                        bool inUse = false;
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            std::wstring baseName = findData.cFileName;
                            // 移除.wlc扩展名，得到基本文件名
                            size_t extPos = baseName.rfind(L".wlc");
                            if (extPos != std::wstring::npos) {
                                baseName = baseName.substr(0, extPos);
                            }

                            // 检查任何以这个基本名称开头的锁文件是否在使用中
                            for (const auto& entry : m_lockFiles) {
                                if (entry.first.find(baseName) != std::wstring::npos) {
                                    inUse = true;
                                    break;
                                }
                            }
                        }

                        if (!inUse) {
                            // 安全地尝试打开并删除未使用的文件
                            HANDLE hFile = CreateFile(fullPath.c_str(),
                                GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

                            if (hFile != INVALID_HANDLE_VALUE) {
                                CloseHandle(hFile);
                                DeleteFile(fullPath.c_str());

                                // 同时删除对应的.rlc文件
                                std::wstring rlcPath = fullPath;
                                size_t extPos = rlcPath.rfind(L".wlc");
                                if (extPos != std::wstring::npos) {
                                    rlcPath = rlcPath.substr(0, extPos) + L".rlc";
                                    DeleteFile(rlcPath.c_str());
                                }
                            }
                        }
                    } while (FindNextFile(hFind, &findData));

                    FindClose(hFind);
                }
            }
            catch (...) {
                // 忽略清理过程中的任何异常
            }
        }

    private:
        LockFileRegistry() {
            // 注册进程退出回调，确保程序退出时清理锁文件
            std::atexit([]() {
                LockFileRegistry::getInstance().cleanupStaleFiles();
                });
        }

        ~LockFileRegistry() {
            cleanupStaleFiles();
        }

        void cleanupLockFile(const std::wstring& filePath) {
            // 防止多线程同时清理同一个文件
            std::lock_guard<std::mutex> lock(m_mutex);
            try {
                // 构造锁文件路径
                TCHAR tempPath[MAX_PATH] = { 0 };
                GetTempPath(MAX_PATH, tempPath);
                std::wstring lockBasePath = tempPath;
                if (lockBasePath.back() != L'\\') {
                    lockBasePath += L"\\";
                }
                lockBasePath += L"rwfilelock\\";

                // 获取基本文件名
                std::wstring filename = filePath;
                size_t pos = filename.find_last_of(L"\\/");
                if (pos != std::wstring::npos) {
                    filename = filename.substr(pos + 1);
                }

                // 构造读写锁文件路径
                std::wstring readerWriterLockPath = lockBasePath + filename + L".rlc";
                std::wstring writerLockPath = lockBasePath + filename + L".wlc";

                // 安全地删除文件
                // 尝试以独占方式打开文件，确保没有其他进程正在使用
                HANDLE hFile = CreateFile(readerWriterLockPath.c_str(),
                    GENERIC_READ | GENERIC_WRITE,
                    0, // 不共享，确保独占访问
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

                if (hFile != INVALID_HANDLE_VALUE) {
                    CloseHandle(hFile);
                    DeleteFile(readerWriterLockPath.c_str());
                }

                hFile = CreateFile(writerLockPath.c_str(),
                    GENERIC_READ | GENERIC_WRITE,
                    0, // 不共享，确保独占访问
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

                if (hFile != INVALID_HANDLE_VALUE) {
                    CloseHandle(hFile);
                    DeleteFile(writerLockPath.c_str());
                }
            }
            catch (...) {
                // 忽略清理过程中的任何异常
            }
        }

        std::mutex m_mutex;
        std::map<std::wstring, int> m_lockFiles; // 文件路径到引用计数的映射
    };
}

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

ShareMemory::ShareMemory(LPCWSTR lpName, bool write) : m_write(write) {
    assert(lpName);
    m_lpMapName = L"ShareMemoryMap-";
    m_sLockedFilePath = L"ShareMemoryLockedFile-";
    if (lpName) {
        m_lpMapName.append(lpName);
        m_sLockedFilePath.append(lpName);
    }
    m_sLockedFilePath.append(L".mdb");

    // 创建读写锁
    m_pReadFileLock = new NMt::CReadFileLock(m_sLockedFilePath.c_str());
    m_pWriteFileLock = new NMt::CWriteFileLock(m_sLockedFilePath.c_str());

    // 注册锁文件到自动清理系统
    registerLockFile();
}

ShareMemory::~ShareMemory() {
    // 确保所有资源正确释放
    try {
        // 确保锁按照获取的相反顺序释放，避免死锁风险
        // 先释放写锁，再释放读锁
        if (m_pWriteFileLock && m_pWriteFileLock->isLocked()) {
            m_pWriteFileLock->Unlock();
        }
        if (m_pReadFileLock && m_pReadFileLock->isLocked()) {
            m_pReadFileLock->Unlock();
        }

        // 取消注册锁文件，在没有活跃引用时会被清理
        unregisterLockFile();

        // 释放内存映射资源
        if (m_pBuffer) {
            ::UnmapViewOfFile(m_pBuffer);
            m_pBuffer = nullptr;
        }
        if (m_hMap) {
            ::CloseHandle(m_hMap);
            m_hMap = nullptr;
        }
    }
    catch (...) {
        // 确保即使发生异常，也能完成清理
    }

    delete m_pReadFileLock;
    delete m_pWriteFileLock;
}

ShareMemoryWrite::~ShareMemoryWrite() {
    // 基类析构函数会处理资源清理
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
    try {
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
    catch (const std::exception&) {
        // 捕获异常，防止内存操作失败导致程序崩溃
        return -1;
    }
}

int ShareMemoryWrite::write(ShareMemoryData* data, int size) {
    if (!m_pBuffer || !data || size < 0) {
        return -1;
    }

    // 使用RAII锁守卫确保锁总是被释放
    class WriteLockGuard {
    public:
        WriteLockGuard(NMt::CWriteFileLock* lock) : m_lock(lock) {
            if (m_lock) m_lock->Lock();
        }
        ~WriteLockGuard() {
            if (m_lock) m_lock->Unlock();
        }
    private:
        NMt::CWriteFileLock* m_lock;
    };

    // 自动管理锁的生命周期
    WriteLockGuard guard(m_pWriteFileLock);
    assert(m_pWriteFileLock->isLocked());

    if (size > m_size) {
        // 超出大小限制
        assert(false);
    }

    // 写入操作现在被锁保护
    int retv = writeImpl(data, size);
    return retv;
}

ShareMemoryRead::ShareMemoryRead(LPCWSTR lpName) : ShareMemory(lpName, false) {
    m_hMap = ::OpenFileMapping(FILE_MAP_READ, 0, m_lpMapName.c_str());
    if (m_hMap != nullptr) {
        m_pBuffer = (ShareMemoryData*)::MapViewOfFile(m_hMap, FILE_MAP_READ, 0, 0, 0);
        // m_pBuffer可能为空，但不做额外处理，保持接口简洁
    }
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
    try {
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
    catch (const std::exception&) {
        // 捕获异常，防止内存操作失败导致程序崩溃
        return -1;
    }
}

int ShareMemory::read(ShareMemoryData*& data, IShareMemoryInterface* callback) {
    if (!m_pBuffer || !callback) {
        return -1;
    }

    // 使用RAII锁守卫确保锁总是被释放
    class ReadLockGuard {
    public:
        ReadLockGuard(NMt::CReadFileLock* lock) : m_lock(lock) {
            if (m_lock) m_lock->Lock();
        }
        ~ReadLockGuard() {
            if (m_lock) m_lock->Unlock();
        }
    private:
        NMt::CReadFileLock* m_lock;
    };

    // 自动管理锁的生命周期
    ReadLockGuard guard(m_pReadFileLock);
    assert(m_pReadFileLock->isLocked()); // 验证锁已成功获取

    // 读取操作现在被锁保护
    int retv = readImpl(data, callback);
    return retv;
}

// 注册锁文件到中央注册表
void ShareMemory::registerLockFile() {
    try {
        if (!m_sLockedFilePath.empty()) {
            LockFileRegistry::getInstance().registerFile(m_sLockedFilePath);
        }
    }
    catch (const std::exception&) {
        // 即使注册失败也不影响主要功能
    }
}

// 从中央注册表取消注册锁文件
void ShareMemory::unregisterLockFile() {
    try {
        if (!m_sLockedFilePath.empty()) {
            LockFileRegistry::getInstance().unregisterFile(m_sLockedFilePath);
        }
    }
    catch (const std::exception&) {
        // 即使取消注册失败也继续进行资源释放
    }
}
