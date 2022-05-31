// TestFileLockDlg.h : header file
//

#pragma once

#include "RWFileLock.h"

// CTestFileLockDlg dialog
class CTestFileLockDlg : public CDialog
{
// Construction
public:
	CTestFileLockDlg(CWnd* pParent = NULL);	// standard constructor
  ~CTestFileLockDlg(void);

// Dialog Data
	enum { IDD = IDD_TESTFILELOCK_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
  CString m_sLockedFilePath;
  int m_nLockType;
public:
  afx_msg void OnBnClickedButtonLock();
  afx_msg void OnBnClickedButtonUnlock();
private:
  NMt::CReadFileLock* m_pReadFileLock;
  NMt::CWriteFileLock* m_pWriteFileLock;
public:
  afx_msg void OnBnClickedButtonInit();
};
