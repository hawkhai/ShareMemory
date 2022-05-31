// LOCAL INCLUDES
//#include "StdAfx.h"
#ifdef AFX_MSG_BOX
#include "StdAfx.h"
#endif
#include "RWFileLock.h" // declarations
#include <assert.h>

/////////////////////////////// PUBLIC of CRWFileLock ///////////////////////////////////////

NMt::CRWFileLock::CRWFileLock(bool xi_bIsReadLock, LPCTSTR xi_cszFilePath, bool xi_bInitialLock/* = false*/, 
                              DWORD xi_nPollPeriodMs/* = 1000*/) :
  m_bIsReadLock(xi_bIsReadLock), m_bIsLocked(false),
  m_hReaderWriterLockFile(0), m_hWriterLockFile(0), m_nPollPeriodMs(xi_nPollPeriodMs)
{
    wchar_t fpath[MAX_PATH + 1] = { 0 };
    DWORD res = GetTempPath(MAX_PATH, fpath);
    if (res <= 0 || res >= MAX_PATH) {
        // ignore...
    }

    if (fpath[0] && fpath[wcslen(fpath) - 1] != L'\\') {
        wcscat_s(fpath, L"\\");
    }
    if (!PathFileExists(fpath)) {
        CreateDirectory(fpath, NULL);
    }

    wcscat_s(fpath, MAX_PATH, L"rwfilelock");
    if (fpath[0] && fpath[wcslen(fpath) - 1] != L'\\') {
        wcscat_s(fpath, L"\\");
    }
    if (!PathFileExists(fpath)) {
        CreateDirectory(fpath, NULL);
    }

    int start = wcslen(fpath);
    wcscat_s(fpath, xi_cszFilePath);
    int endx = wcslen(fpath);
    for (int i = start; i < endx; i++) {
        if (fpath[i] == L':' ||
            fpath[i] == L'\\' ||
            fpath[i] == L'/') {
            assert(false);
            fpath[i] = L'_';
        }
    }

  CString l_sFilePath = fpath;
  if (!l_sFilePath.IsEmpty())
  {
    m_sReaderWriterLockFilePath = l_sFilePath + ".rlc";
    m_sWriterLockFilePath = l_sFilePath + ".wlc";
  }
  if (xi_bInitialLock)
  {
    Lock();
  }
}

NMt::CRWFileLock::~CRWFileLock()
{
  Unlock();
}

void
NMt::CRWFileLock::Lock()
{
  if (m_sReaderWriterLockFilePath.IsEmpty() || m_bIsLocked)
  {
    return;
  }
  if (m_bIsReadLock)
  {
    // prevent writers from starvation
    while (true)
    {
      // try to open in shared mode
      m_hWriterLockFile = ::CreateFile(m_sWriterLockFilePath, 
                                       GENERIC_READ, 
                                       FILE_SHARE_READ,
                                       NULL, // default security
                                       OPEN_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);
      if (INVALID_HANDLE_VALUE == m_hWriterLockFile)
      {
        DWORD l_nErr = ::GetLastError();
        if (ERROR_SHARING_VIOLATION == l_nErr)
        {
          // locked by writer, wait
          Sleep(m_nPollPeriodMs);
          continue;
        }
        DisplayMsg("Cannot create a writer lock file %s: %d", m_sWriterLockFilePath, l_nErr);
        break;
      }
      // succeeded to open - no writers claimed access
      // close it to allow writers to open it
      if (0 == ::CloseHandle(m_hWriterLockFile))
      {
        DisplayMsg("Cannot close a writer lock file %s: %d", m_sWriterLockFilePath, ::GetLastError());
      }
      break;
    }

    while (true)
    {
      // lock writers, allow readers to share
      m_hReaderWriterLockFile = ::CreateFile(m_sReaderWriterLockFilePath, 
                                             GENERIC_READ, 
                                             FILE_SHARE_READ,
                                             NULL, // default security
                                             OPEN_ALWAYS,
                                             FILE_ATTRIBUTE_NORMAL,
                                             NULL);
      if (INVALID_HANDLE_VALUE == m_hReaderWriterLockFile)
      {
        DWORD l_nErr = ::GetLastError();
        if (ERROR_SHARING_VIOLATION == l_nErr)
        {
          // locked by writer, wait
          Sleep(m_nPollPeriodMs);
          continue;
        }
        DisplayMsg("Cannot create a reader/writer lock file %s: %d", m_sReaderWriterLockFilePath, l_nErr);
        break;
      }
      // succeeded to lock
      break;
    }
  }
  else
  {
    // prevent readers from entering, writers open this file in exclusive mode
    while (true)
    {
      m_hWriterLockFile = ::CreateFile(m_sWriterLockFilePath, 
                                       GENERIC_READ | GENERIC_WRITE, 
                                       0, // exclusive
                                       NULL, // default security
                                       OPEN_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);
      if (INVALID_HANDLE_VALUE == m_hWriterLockFile)
      {
        DWORD l_nErr = ::GetLastError();
        if (ERROR_SHARING_VIOLATION == l_nErr)
        {
          // locked by writers, wait
          Sleep(m_nPollPeriodMs);
          continue;
        }
        DisplayMsg("Cannot create a writer lock file %s: %d", m_sWriterLockFilePath, l_nErr);
        break;
      }
      // succeeded to lock
      break;
    }

    // lock readers/writers
    while (true)
    {
      m_hReaderWriterLockFile = ::CreateFile(m_sReaderWriterLockFilePath, 
                                             GENERIC_READ | GENERIC_WRITE, 
                                             0, // exclusive access
                                             NULL, // default security
                                             OPEN_ALWAYS,
                                             FILE_ATTRIBUTE_NORMAL,
                                             NULL);
      if (INVALID_HANDLE_VALUE == m_hReaderWriterLockFile)
      {
        DWORD l_nErr = ::GetLastError();
        if (ERROR_SHARING_VIOLATION == l_nErr)
        {
          // locked by readers/writers, wait
          Sleep(m_nPollPeriodMs);
          continue;
        }
        DisplayMsg("Cannot create a reader/writer lock file %s: %d", m_sReaderWriterLockFilePath, l_nErr);
        break;
      }
      // succeeded to lock
      break;
    }

  }
  m_bIsLocked = true;
}

void 
NMt::CRWFileLock::Unlock()
{
  if (m_sReaderWriterLockFilePath.IsEmpty() || !m_bIsLocked)
  {
    return;
  }
  if (!m_bIsReadLock) // write lock
  {
    // release readers
    if (0 == ::CloseHandle(m_hWriterLockFile))
    {
      DisplayMsg("Cannot close a writer lock file %s: %d", m_sWriterLockFilePath, ::GetLastError());
    }
  }
  // release readers/writers
  if (0 == ::CloseHandle(m_hReaderWriterLockFile))
  {
    DisplayMsg("Cannot close a reader/writer lock file %s: %d", m_sReaderWriterLockFilePath, ::GetLastError());
  }
  m_bIsLocked = false;
}

void
NMt::DisplayMsg(LPCSTR xi_cszFormat, ...)
{
    CStringA l_sMsg;
    va_list l_ArgList;
    va_start(l_ArgList, xi_cszFormat);
    l_sMsg.FormatV(xi_cszFormat, l_ArgList);
    va_end(l_ArgList);
#ifdef AFX_MSG_BOX
    ::AfxMessageBox(l_sMsg);
#else
    printf(l_sMsg.GetString());
#endif
}

void
NMt::DisplayMsg(LPCWSTR xi_cszFormat, ...)
{
}
