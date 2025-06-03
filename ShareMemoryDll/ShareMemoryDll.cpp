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

// 使用WMI获取系统真实启动时间
UINT64 ShareMemory::GetBootTime() {
    static UINT64 cachedBootTime = 0;
    
    // 只计算一次以确保始终返回相同的值
    if (cachedBootTime == 0) {
        // 使用WMI获取系统启动时间
        HRESULT hres;
        
        // 初始COM库
        hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (SUCCEEDED(hres)) {
            // 初始COM安全性
            hres = CoInitializeSecurity(
                NULL,
                -1,
                NULL,
                NULL,
                RPC_C_AUTHN_LEVEL_DEFAULT,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL,
                EOAC_NONE,
                NULL
            );
            
            if (SUCCEEDED(hres) || hres == RPC_E_TOO_LATE) {
                // 创建WMI实例
                IWbemLocator* pLoc = NULL;
                hres = CoCreateInstance(
                    CLSID_WbemLocator,
                    0,
                    CLSCTX_INPROC_SERVER,
                    IID_IWbemLocator,
                    (LPVOID*)&pLoc
                );
                
                if (SUCCEEDED(hres) && pLoc) {
                    IWbemServices* pSvc = NULL;
                    
                    // 连接到WMI
                    hres = pLoc->ConnectServer(
                        _bstr_t(L"ROOT\\CIMV2"),
                        NULL,
                        NULL,
                        0,
                        NULL,
                        0,
                        0,
                        &pSvc
                    );
                    
                    if (SUCCEEDED(hres) && pSvc) {
                        // 设置代理安全级别
                        hres = CoSetProxyBlanket(
                            pSvc,
                            RPC_C_AUTHN_WINNT,
                            RPC_C_AUTHZ_NONE,
                            NULL,
                            RPC_C_AUTHN_LEVEL_CALL,
                            RPC_C_IMP_LEVEL_IMPERSONATE,
                            NULL,
                            EOAC_NONE
                        );
                        
                        if (SUCCEEDED(hres)) {
                            // 查询Win32_OperatingSystem类以获取最后一次启动时间
                            IEnumWbemClassObject* pEnumerator = NULL;
                            hres = pSvc->ExecQuery(
                                bstr_t("WQL"),
                                bstr_t("SELECT LastBootUpTime FROM Win32_OperatingSystem"),
                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                NULL,
                                &pEnumerator
                            );
                            
                            if (SUCCEEDED(hres) && pEnumerator) {
                                IWbemClassObject* pclsObj = NULL;
                                ULONG uReturn = 0;
                                
                                // 获取第一个对象
                                hres = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                                
                                if (SUCCEEDED(hres) && uReturn > 0) {
                                    VARIANT vtProp;
                                    VariantInit(&vtProp);
                                    
                                    // 获取LastBootUpTime属性
                                    hres = pclsObj->Get(L"LastBootUpTime", 0, &vtProp, 0, 0);
                                    
                                    if (SUCCEEDED(hres) && vtProp.vt == VT_BSTR) {
                                        // 处理WMI时间格式: YYYYMMDDHHMMSS.MMMMMM+UUU
                                        // 例如: 20250602143000.000000+480
                                        std::wstring wmiTimeStr(vtProp.bstrVal);
                                        
                                        if (wmiTimeStr.length() >= 14) {
                                            // 提取日期时间部分
                                            int year = _wtoi(wmiTimeStr.substr(0, 4).c_str());
                                            int month = _wtoi(wmiTimeStr.substr(4, 2).c_str());
                                            int day = _wtoi(wmiTimeStr.substr(6, 2).c_str());
                                            int hour = _wtoi(wmiTimeStr.substr(8, 2).c_str());
                                            int minute = _wtoi(wmiTimeStr.substr(10, 2).c_str());
                                            int second = _wtoi(wmiTimeStr.substr(12, 2).c_str());
                                            
                                            // 转换为UNIX时间戳
                                            struct tm tmTime;
                                            memset(&tmTime, 0, sizeof(tmTime));
                                            tmTime.tm_year = year - 1900; // 年份从1900算起
                                            tmTime.tm_mon = month - 1;   // 月份从0算起
                                            tmTime.tm_mday = day;
                                            tmTime.tm_hour = hour;
                                            tmTime.tm_min = minute;
                                            tmTime.tm_sec = second;
                                            
                                            // 转换为时间戳
                                            time_t bootTime = mktime(&tmTime);
                                            if (bootTime != -1) {
                                                cachedBootTime = (UINT64)bootTime;
                                            }
                                        }
                                    }
                                    
                                    VariantClear(&vtProp);
                                    pclsObj->Release();
                                }
                                
                                pEnumerator->Release();
                            }
                        }
                        pSvc->Release();
                    }
                    pLoc->Release();
                }
            }
            CoUninitialize();
        }
        
        // 如果 WMI 方法失败，尝试使用注册表
        if (cachedBootTime == 0) {
            HKEY hKey;
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Windows", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
                DWORD systemStartTime = 0;
                DWORD dataSize = sizeof(DWORD);
                DWORD dataType = 0;
                if (RegQueryValueEx(hKey, L"SystemStartTime", NULL, &dataType, (LPBYTE)&systemStartTime, &dataSize) == ERROR_SUCCESS) {
                    if (dataType == REG_DWORD) {
                        cachedBootTime = systemStartTime;
                    }
                }
                RegCloseKey(hKey);
            }
        }
        
        // 如果上述方法都失败，创建一个基于当前进程启动时间的标识符
        if (cachedBootTime == 0) {
            // 注意：到这里就是真的获取不到系统启动时间
            // 使用当前时间作为标识符，这样即使不准确，也可以保证在同一个进程中的一致性
            time_t currentTime;
            time(&currentTime);
            cachedBootTime = (UINT64)currentTime;
        }
    }
    
    return cachedBootTime;
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
