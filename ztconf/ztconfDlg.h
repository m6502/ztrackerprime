// ztconfDlg.h : header file
//

#if !defined(AFX_ZTCONFDLG_H__FD64FF37_F222_46BE_B10C_D030B0D2424B__INCLUDED_)
#define AFX_ZTCONFDLG_H__FD64FF37_F222_46BE_B10C_D030B0D2424B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CZtconfDlg dialog

class CZtconfDlg : public CDialog
{
// Construction
public:
	CZtconfDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CZtconfDlg)
	enum { IDD = IDD_ZTCONF_DIALOG };
	CListBox	m_VideoListBox;
	BOOL	m_bFullscreen;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CZtconfDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CZtconfDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnButton1();
	afx_msg void OnClose();
	virtual void OnCancel();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ZTCONFDLG_H__FD64FF37_F222_46BE_B10C_D030B0D2424B__INCLUDED_)
