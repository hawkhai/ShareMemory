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
      DWORD xi_nPollPeriodMs = 1000);
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
    HANDLE m_hReaderWriterLockFile;
    /*!
    \brief Writers lock file.
    */
    HANDLE m_hWriterLockFile;
    /*!
    \brief If it is locked.
    */
    bool m_bIsLocked;
    /*!
    \brief If it is a read lock.
    */
    bool m_bIsReadLock;
    /*!
    \brief Polling period (milliseconds).
    */
    DWORD m_nPollPeriodMs;

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
    CReadFileLock(LPCTSTR xi_cszFilePath, bool xi_bInitialLock = false, DWORD xi_nPollPeriodMs = 1000) :
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
    CWriteFileLock(LPCTSTR xi_cszFilePath, bool xi_bInitialLock = false, DWORD xi_nPollPeriodMs = 1000) :
        CRWFileLock(false, xi_cszFilePath, xi_bInitialLock, xi_nPollPeriodMs) {}
  };

  /*!
  \brief Displays a message.
  \param[in] xi_cszFormat a message format
  */
  void DisplayMsg(LPCTSTR xi_cszFormat, ...);

}