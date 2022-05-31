// TestFileLockDlg.cpp : implementation file
//

#include "stdafx.h"
#include "TestFileLock.h"
#include "TestFileLockDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTestFileLockDlg dialog




CTestFileLockDlg::CTestFileLockDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTestFileLockDlg::IDD, pParent)
  , m_sLockedFilePath(_T(""))
  , m_nLockType(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
  m_pReadFileLock = NULL;
  m_pWriteFileLock = NULL;
}

CTestFileLockDlg::~CTestFileLockDlg(void)
{
  delete m_pReadFileLock;
  delete m_pWriteFileLock;
}

void CTestFileLockDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Text(pDX, IDC_EDIT_LOCKED_FILE_PATH, m_sLockedFilePath);
  DDX_Radio(pDX, IDC_RADIO_LOCK_READ, m_nLockType);
}

BEGIN_MESSAGE_MAP(CTestFileLockDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
  ON_BN_CLICKED(IDC_BUTTON_LOCK, &CTestFileLockDlg::OnBnClickedButtonLock)
  ON_BN_CLICKED(IDC_BUTTON_UNLOCK, &CTestFileLockDlg::OnBnClickedButtonUnlock)
  ON_BN_CLICKED(IDC_BUTTON_INIT, &CTestFileLockDlg::OnBnClickedButtonInit)
END_MESSAGE_MAP()


// CTestFileLockDlg message handlers

BOOL CTestFileLockDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_sLockedFilePath = ".\\Test.mdb";
  UpdateData(FALSE);
  m_pReadFileLock = new NMt::CReadFileLock(m_sLockedFilePath);
  m_pWriteFileLock = new NMt::CWriteFileLock(m_sLockedFilePath);

  CString l_sText;
  l_sText.Format("Initialized: %s\n", m_sLockedFilePath);
  TRACE(l_sText);
  GetDlgItem(IDC_STATIC_TEXT_INIT)->SetWindowText(l_sText);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTestFileLockDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTestFileLockDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CTestFileLockDlg::OnBnClickedButtonLock()
{
  UpdateData();
  CString l_sText;
  l_sText.Format("Pending Lock %s: %s\n", (0 == m_nLockType) ? "Read" : "Write", m_sLockedFilePath);
  TRACE(l_sText);
  GetDlgItem(IDC_STATIC_TEXT_LOCK)->SetWindowText(l_sText);
  CWaitCursor l_WaitCursor;
  if (0 == m_nLockType) // read lock
  {
    m_pReadFileLock->Lock();
  }
  else // write lock
  {
    m_pWriteFileLock->Lock();
  }
  l_sText.Format("Locked %s: %s\n", (0 == m_nLockType) ? "Read" : "Write", m_sLockedFilePath);
  TRACE(l_sText);
  // ::AfxMessageBox(l_sText);
  GetDlgItem(IDC_STATIC_TEXT_LOCK)->SetWindowText(l_sText);
}

void CTestFileLockDlg::OnBnClickedButtonUnlock()
{
  UpdateData();
  CString l_sText;
  l_sText.Format("Pending Unlock %s: %s\n", (0 == m_nLockType) ? "Read" : "Write", m_sLockedFilePath);
  TRACE(l_sText);
  GetDlgItem(IDC_STATIC_TEXT_LOCK)->SetWindowText(l_sText);
  if (0 == m_nLockType) // read lock
  {
    m_pReadFileLock->Unlock();
  }
  else // write lock
  {
    m_pWriteFileLock->Unlock();
  }
  l_sText.Format("Unlocked %s: %s\n", (0 == m_nLockType) ? "Read" : "Write", m_sLockedFilePath);
  TRACE(l_sText);
  // ::AfxMessageBox(l_sText);
  GetDlgItem(IDC_STATIC_TEXT_LOCK)->SetWindowText(l_sText);
}


void CTestFileLockDlg::OnBnClickedButtonInit()
{
  UpdateData();
  delete m_pReadFileLock;
  delete m_pWriteFileLock;
  m_pReadFileLock = new NMt::CReadFileLock(m_sLockedFilePath);
  m_pWriteFileLock = new NMt::CWriteFileLock(m_sLockedFilePath);

  CString l_sText;
  l_sText.Format("Initialized: %s\n", m_sLockedFilePath);
  TRACE(l_sText);
  GetDlgItem(IDC_STATIC_TEXT_INIT)->SetWindowText(l_sText);
}
