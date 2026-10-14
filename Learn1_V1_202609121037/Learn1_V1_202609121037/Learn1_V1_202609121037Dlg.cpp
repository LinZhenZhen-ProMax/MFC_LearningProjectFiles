
// Learn1_V1_202609121037Dlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "Learn1_V1_202609121037.h"
#include "Learn1_V1_202609121037Dlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/*****************************************
* class CAboutDlg类，用于“关于”菜单，公开继承自CDialogEx类
* 公开继承的意思是，编译器将父类里面的成员复制过来以后，重新打上权限标签，
* 并且新旧标签要按更严的来，旧标签是private就继续private，
* 顺序是：1.声明类
*        2.重写DoDataExchange数据交换函数，函数中执行父类同名函数
*        3.DECLARE_MESSAGE_MAP()
*        4.构造函数直接调父类的构造函数，做出窗口（对话框）
*        5.BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
*          END_MESSAGE_MAP()
*****************************************/
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

// 声明重写DoDataExchange函数
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

// 构造函数，直接调父类的构造函数
CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

// 定义重写的DoDataExchange函数：执行父类同名函数
void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()



/********************************************************
* CLearn1V1202609121037Dlg类在头文件中做声明，依旧是继承CDialogEx类
* CLearn1V1202609121037Dlg构造函数，执行父类的构造函数，做一个主对话框（主窗口）
* CWnd   ← 所有窗口的祖宗（按钮、输入框、对话框……全是它子孙）
*  └ CWnd-derived...
*     └ CDialog
*        └ CDialogEx
*           └ CLearn1V1202609121037Dlg   ← 这个工程类
********************************************************/
CLearn1V1202609121037Dlg::CLearn1V1202609121037Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LEARN1_V1_202609121037_DIALOG, pParent)
{
	//测试
	TRACE(_T("[3] Dlg 构造函数\n"));
	m_strSend = _T("");
	m_strRecv = _T("");
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

/********************************************************
* 定义重写的DoDataExchange函数，还是执行父类同名函数
* 用来做数据交换
* 这里是两个编辑框的数据交换，两个后端变量m_strSend，m_strRecv，跟两个前端的编辑框连接
********************************************************/
void CLearn1V1202609121037Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SEND, m_strSend);
	DDX_Text(pDX, IDC_EDIT_RECEIVE, m_strRecv);
}

BEGIN_MESSAGE_MAP(CLearn1V1202609121037Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_OPENSERIAL, &CLearn1V1202609121037Dlg::OnBnClickedButtonOpenserial)
	ON_CBN_SELCHANGE(IDC_COMBO_PORT, &CLearn1V1202609121037Dlg::OnCbnSelchangeComboPort)
END_MESSAGE_MAP()



/**********************************************
* 重写对话框初始化函数OnInitDialog
* 依旧先执行一遍父类同名函数
* 后续还可以添加更多初始化函数
**********************************************/

BOOL CLearn1V1202609121037Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// TODO: 在此添加额外的初始化代码
	CLearn1V1202609121037Dlg::InitSystemMenu();
	
	

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

/**************************************************
* 系统菜单初始化函数，
* 包括把“关于”菜单添加到系统菜单
* 规矩①：命令号低 4 位必须是 0
	     因为 Windows 会往低 4 位塞它自己的标志位，
	     我们判断点击时要用 & 0xFFF0 （按位与，0与任何数都是0）把低 4 位抹掉再比。
	     所以你自己定义命令号时，低 4 位必须留空 —— 否则永远比不中。
* 规矩②：命令号必须小于 0xF000
	     0xF000 ~ 0xFFFF 被 Windows 占用了，
	     那是"关闭(0xF060)/最小化(0xF020)/最大化"这类系统命令的地盘。
**************************************************/
void CLearn1V1202609121037Dlg::InitSystemMenu()
{
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}
}
/********************************************
* 系统菜单响应函数
********************************************/
void CLearn1V1202609121037Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

//  如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CLearn1V1202609121037Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CLearn1V1202609121037Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CLearn1V1202609121037Dlg::OnBnClickedButtonOpenserial()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CLearn1V1202609121037Dlg::OnCbnSelchangeComboPort()
{
	// TODO: 在此添加控件通知处理程序代码
}
