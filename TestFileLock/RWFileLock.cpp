// LOCAL INCLUDES
#include "StdAfx.h"
#include "RWFileLock.h" // declarations

/////////////////////////////// PUBLIC of CRWFileLock ///////////////////////////////////////

NMt::CRWFileLock::CRWFileLock(bool xi_bIsReadLock, LPCTSTR xi_cszFilePath, bool xi_bInitialLock/* = false*/, 
                              DWORD xi_nPollPeriodMs/* = 1000*/) :
  m_bIsReadLock(xi_bIsReadLock), m_bIsLocked(false),
  m_hReaderWriterLockFile(0), m_hWriterLockFile(0), m_nPollPeriodMs(xi_nPollPeriodMs)
{
  CString l_sFilePath = xi_cszFilePath;
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
NMt::DisplayMsg(LPCTSTR xi_cszFormat, ...)
{
  CString l_sMsg;
  va_list l_ArgList;
  va_start(l_ArgList, xi_cszFormat);
  l_sMsg.FormatV(xi_cszFormat, l_ArgList);
  va_end(l_ArgList);
  ::AfxMessageBox(l_sMsg);
}