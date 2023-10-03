/*!
\file RWFileLock.h
\brief Header file for a Read/Write File Lock classes.

Implements inter computer read/write locks.
\note These classes are not protected from multithreaded access
      because they are intended to be created on stack or as members of other classes
      that are created on stack.
\author Andriy Brozgol
*/
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atlstr.h>

#define POLL_PERIOD_DEFAULT 15 // 基本上能达到 60 帧的水平。

/*!
\brief Multi threading.
*/
namespace NMt
{
  /*!
  \brief A Read/Write File Lock implementation base class.
  */
  class CRWFileLock
  {
  public:
    // LIFECYCLE
    /*!
    \brief Constructor.
    \param[in] xi_bIsReadLock if it is a read lock
    \param[in] xi_cszFilePath a path to a file to be accessed
    \param[in] xi_bInitialLock if it is initially locked
    \param[in] xi_nPollPeriodMs polling period (milliseconds)
    */
    CRWFileLock(bool xi_bIsReadLock, LPCTSTR xi_cszFilePath, bool xi_bInitialLock = false, 
      DWORD xi_nPollPeriodMs = POLL_PERIOD_DEFAULT);
    /*!
    \brief Destructor.
    */
    ~CRWFileLock();

    // OPERATIONS
    /*!
    \brief Locks access to the file.
    */
    void Lock();
    /*!
    \brief Unlocks access to the file.
    */
    void Unlock();

    bool isLocked() {
        return m_bIsLocked;
    }

  protected:
    // DATA MEMBERS
    /*!
    \brief Readers/Writers lock file path.
    */
    CString m_sReaderWriterLockFilePath;
    /*!
    \brief Writers lock file path.
    */
    CString m_sWriterLockFilePath;
    /*!
    \brief Readers/Writers lock file.
    */
    HANDLE m_hReaderWriterLockFile = 0;
    /*!
    \brief Writers lock file.
    */
    HANDLE m_hWriterLockFile = 0;
    /*!
    \brief If it is locked.
    */
    bool m_bIsLocked = false;
    /*!
    \brief If it is a read lock.
    */
    bool m_bIsReadLock = false;
    /*!
    \brief Polling period (milliseconds).
    */
    DWORD m_nPollPeriodMs = 0;

  };

  /*!
  \brief Read File Lock class.
  */
  class CReadFileLock : public CRWFileLock
  {
  public:
    // LIFECYCLE
    /*!
    \brief Constructor.
    \param[in] xi_cszFilePath a path to a file to be accessed
    \param[in] xi_bInitialLock if it is initially locked
    \param[in] xi_nPollPeriodMs polling period (milliseconds)
    */
    CReadFileLock(LPCTSTR xi_cszFilePath, bool xi_bInitialLock = false, DWORD xi_nPollPeriodMs = POLL_PERIOD_DEFAULT) :
        CRWFileLock(true, xi_cszFilePath, xi_bInitialLock, xi_nPollPeriodMs) {}
  };
  /*!
  \brief Write File Lock class.
  */
  class CWriteFileLock : public CRWFileLock
  {
  public:
    // LIFECYCLE
    /*!
    \brief Constructor.
    \param[in] xi_cszFilePath a path to a file to be accessed
    \param[in] xi_bInitialLock if it is initially locked
    \param[in] xi_nPollPeriodMs polling period (milliseconds)
    */
    CWriteFileLock(LPCTSTR xi_cszFilePath, bool xi_bInitialLock = false, DWORD xi_nPollPeriodMs = POLL_PERIOD_DEFAULT) :
        CRWFileLock(false, xi_cszFilePath, xi_bInitialLock, xi_nPollPeriodMs) {}
  };

  /*!
  \brief Displays a message.
  \param[in] xi_cszFormat a message format
  */
  void DisplayMsg(LPCSTR xi_cszFormat, ...);
  void DisplayMsg(LPCWSTR xi_cszFormat, ...);
}
