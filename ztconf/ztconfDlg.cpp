// ztconfDlg.cpp : implementation file
//

#include "stdafx.h"
#include "ztconf.h"
#include "ztconfDlg.h"
#include "sdl.h"
#include "../conf.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CZtconfDlg dialog

CZtconfDlg::CZtconfDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CZtconfDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CZtconfDlg)
	m_bFullscreen = FALSE;
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CZtconfDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CZtconfDlg)
	DDX_Control(pDX, IDC_LIST1, m_VideoListBox);
	DDX_Check(pDX, IDC_CHECK1, m_bFullscreen);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CZtconfDlg, CDialog)
	//{{AFX_MSG_MAP(CZtconfDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CZtconfDlg message handlers

ZTConf ztconf;

BOOL CZtconfDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Rect **modes;

    modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);

    /* Check is there are any modes available */
    if(modes == (SDL_Rect **)0){
    	m_VideoListBox.AddString("No modes available");
    }

    /* Check if or resolution is restricted */
    
    int res = ztconf.load();
    if (res == -1)  {
        MessageBox("Unable to load zt.conf");
        exit(-1);
    }
    if (ztconf.full_screen)
        m_bFullscreen = true;
    else
        m_bFullscreen = false;
    UpdateData(false);
    if(modes == (SDL_Rect **)-1){
    	m_VideoListBox.AddString("All modes available (?)");
    }
    else{
      /* Print valid modes */
        for(int i=0;modes[i];++i) {
            char str[50];
            if (modes[i]->w >= 640 && modes[i]->h >= 480) {
                sprintf(str,"%d x %d",modes[i]->w, modes[i]->h);
                m_VideoListBox.AddString(str);
            }
        }
    }        
    for (int i=0;i<m_VideoListBox.GetCount();i++) {
        CString c;
        int w,h;
        m_VideoListBox.GetText(i,c);
        LPSTR buf = c.GetBuffer(128);
        sscanf(buf, "%d x %d", &w, &h);
        if (w == ztconf.screen_width && h==ztconf.screen_height)
            m_VideoListBox.SetCurSel(i);  // .SetSel(i);
        c.ReleaseBuffer();
    }
    return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CZtconfDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

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

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CZtconfDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CZtconfDlg::OnButton1() 
{
	// TODO: Add your control notification handler code here
	
}

void CZtconfDlg::OnClose() 
{
	// TODO: Add your message handler code here and/or call default
	SDL_Quit();
	CDialog::OnClose();
}

void CZtconfDlg::OnCancel() 
{
	// TODO: Add extra cleanup here
	
	CDialog::OnCancel();
}

void CZtconfDlg::OnOK() 
{
	// TODO: Add extra validation here
    UpdateData(true);
    CString c;
    int h,w,sel;
    sel = m_VideoListBox.GetCurSel();
    if (sel != -1) {
        m_VideoListBox.GetText( sel ,c);
        LPSTR buf = c.GetBuffer(128);
        sscanf(buf, "%d x %d", &w, &h);
    //    if (w == ztconf.screen_width && h==ztconf.screen_height)
    //        m_VideoListBox.SetSel(i);
        ztconf.screen_width = w;
        ztconf.screen_height = h;
        c.ReleaseBuffer();
    }
    if (m_bFullscreen)
        ztconf.full_screen = 1;
    else
        ztconf.full_screen = 0;
	ztconf.save();
	CDialog::OnOK();
}
