
// Learn1_V1_202609121037Dlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "Learn1_V1_202609121037.h"
#include "Learn1_V1_202609121037Dlg.h"
#include "afxdialogex.h"

#include <cmath>
#include <cwctype>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/*****************************************
* class CAboutDlg类，用于"关于"菜单，公开继承自CDialogEx类
*****************************************/
class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

/*****************************************
* 对话框实现
*
* 【线程模型总览】（详细文档见 docs/THREADING_AND_PLANNING.md）
*
*   UI 主线程                     后台线程 x2
*   ─────────                    ────────────────────────
*   串口打开/发送 ──────────────► SerialWorker::Run（读+写循环）
*   TIMER_RX 轮询 ◄────────────── 环形缓冲区收行 + PostMessage
*   加载G代码 ──► PlannerWorker::Submit
*                                 PlannerWorker::Run（前瞻规划）
*   WM_APP_PLAN_DONE ◄─────────── 规划快照 shared_ptr 发布
*   TIMER_PLAY 回放采样 ──► OnPaint 画轨迹 + 当前点
*
*   原则：UI 线程永不直接碰串口句柄、永不执行规划计算；
*   跨线程只传三种东西：环形缓冲区数据 / PostMessage / 不可变快照。
*****************************************/

// CLearn1V1202609121037Dlg 对话框

CLearn1V1202609121037Dlg::CLearn1V1202609121037Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LEARN1_V1_202609121037_DIALOG, pParent)
{
	//测试
	TRACE(_T("[3] Dlg 构造函数\n"));
	m_strSend = _T("");
	m_strRecv = _T("");
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

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
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BUTTON_OPENSERIAL, &CLearn1V1202609121037Dlg::OnBnClickedButtonOpenserial)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CLearn1V1202609121037Dlg::OnBnClickedButtonSend)
	ON_CBN_SELCHANGE(IDC_COMBO_PORT, &CLearn1V1202609121037Dlg::OnCbnSelchangeComboPort)
	ON_BN_CLICKED(IDC_BTN_LOADFILE, &CLearn1V1202609121037Dlg::OnBnClickedBtnLoadfile)
	ON_BN_CLICKED(IDC_BTN_PLAY, &CLearn1V1202609121037Dlg::OnBnClickedBtnPlay)
	ON_BN_CLICKED(IDC_BTN_STOP, &CLearn1V1202609121037Dlg::OnBnClickedBtnStop)
	ON_MESSAGE(WM_APP_SERIAL_LINE, &CLearn1V1202609121037Dlg::OnSerialLine)
	ON_MESSAGE(WM_APP_PLAN_DONE, &CLearn1V1202609121037Dlg::OnPlanDone)
END_MESSAGE_MAP()



/**********************************************
* 对话框初始化
* 新增：串口选项初始化、动态按钮、双定时器
**********************************************/
BOOL CLearn1V1202609121037Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CLearn1V1202609121037Dlg::InitSystemMenu();

	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	/*---- 分支新增 ----*/
	InitPortOptions();		// 枚举串口 + 波特率下拉框
	CreateUiExtras();		// 加宽窗口、创建"加载/仿真"按钮、确定预览区

	// 规划线程完成后的通知目标
	m_planner.SetNotify(GetSafeHwnd(), WM_APP_PLAN_DONE);

	// 两个定时器：接收兜底轮询 + 轨迹回放（回放定时器在"开始仿真"时启动）
	SetTimer(TIMER_RX, 100, nullptr);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

/**************************************************
* 系统菜单初始化函数，
* 包括把"关于"菜单添加到系统菜单
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
/**********************************************
* 系统菜单响应函数
**********************************************/
}
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
		// 分支新增：普通重绘用 CPaintDC（自行负责校验无效区域），
		// 再画右侧轨迹预览；子控件由系统自行绘制，无需基类 OnPaint
		CPaintDC dc(this);
		DrawPreview(dc);
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CLearn1V1202609121037Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

/**********************************************
* 打开/关闭串口（按钮反复按切换状态）
* 串口打开后数据收发全部由 SerialWorker 后台线程负责
**********************************************/
void CLearn1V1202609121037Dlg::OnBnClickedButtonOpenserial()
{
	CComboBox* pPort = (CComboBox*)GetDlgItem(IDC_COMBO_PORT);
	CComboBox* pBaud = (CComboBox*)GetDlgItem(IDC_COMBO_BAUD);
	if (!pPort || !pBaud) return;

	if (m_serial.IsPortOpen())
	{
		m_serial.ClosePort();						// 停线程 -> 关句柄
		SetDlgItemText(IDC_BUTTON_OPENSERIAL, _T("打开串口"));
		AppendRecvText(_T("[串口已关闭]"));
		return;
	}

	CString port;
	pPort->GetWindowText(port);
	if (port.IsEmpty())
	{
		AfxMessageBox(_T("请先选择串口号"));
		return;
	}
	CString baud;
	pBaud->GetWindowText(baud);

	CT2A portA(port);								// 串口类内部用 ANSI API
	if (m_serial.OpenPort(portA, _ttoi(baud),
						  GetSafeHwnd(), WM_APP_SERIAL_LINE))
	{
		SetDlgItemText(IDC_BUTTON_OPENSERIAL, _T("关闭串口"));
		AppendRecvText((CString)_T("[串口已打开] ") + port + _T(" @") + baud);
	}
	else
	{
		AppendRecvText((CString)_T("[打开失败] ") + port + _T(" 可能不存在或被占用"));
	}
}

void CLearn1V1202609121037Dlg::OnCbnSelchangeComboPort()
{
	CString port;
	((CComboBox*)GetDlgItem(IDC_COMBO_PORT))->GetWindowText(port);
	AppendRecvText((CString)_T("[已选串口] ") + port);
}

/**********************************************
* 发送按钮：把编辑框内容交给 SerialWorker 的发送队列
* 立即返回，绝不阻塞 UI（真正的写出在后台线程）
**********************************************/
void CLearn1V1202609121037Dlg::OnBnClickedButtonSend()
{
	if (!UpdateData(TRUE)) return;
	if (m_strSend.IsEmpty()) return;

	if (!m_serial.IsPortOpen())
	{
		AppendRecvText(_T("[发送失败] 串口未打开"));
		return;
	}
	CT2A lineA((CString)m_strSend + _T("\n"));
	if (m_serial.SendLine(std::string(lineA)))
	{
		AppendRecvText((CString)_T("[已入发送队列] ") + m_strSend);
	}
}

/**********************************************
* 加载 G 代码：文件对话框 -> LoadProgram 解析出运动段
* -> 提交规划线程（解析+提交都很轻，UI 不卡）
**********************************************/
void CLearn1V1202609121037Dlg::OnBnClickedBtnLoadfile()
{
	CFileDialog dlg(TRUE, _T("nc"), nullptr,
		OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		_T("G代码程序 (*.nc;*.txt;*.gcode)|*.nc;*.txt;*.gcode|所有文件 (*.*)|*.*||"),
		this);
	if (dlg.DoModal() != IDOK) return;

	CT2A pathA(dlg.GetPathName());

	GCodeModalMachine gc;
	GCodeModalMachine::ModalState modal;			// 模态状态机
	std::vector<GCodeModalMachine::CodeError> errors;
	std::vector<GCodeModalMachine::MotionSegment> segs;

	if (!gc.LoadProgram(std::string(pathA), errors, modal, segs))
	{
		AppendRecvText(_T("[加载失败] 文件无法打开"));
		return;
	}

	CString msg;
	msg.Format(_T("[加载] 运动段 %d 个, 解析错误 %d 个, 正在后台规划..."),
			   (int)segs.size(), (int)errors.size());
	AppendRecvText(msg);

	// 提交规划任务（覆盖式：只规划最新一次加载）
	PlannerWorker::Job job;
	job.id = ++m_jobSeq;
	job.segs = std::move(segs);
	job.cfg = m_cfg;
	m_planner.Submit(std::move(job));
}

/**********************************************
* 开始仿真：以启动时刻为 t=0，由回放定时器驱动采样
**********************************************/
void CLearn1V1202609121037Dlg::OnBnClickedBtnPlay()
{
	m_path = m_planner.GetLatest();
	if (!m_path || m_path->segs.empty())
	{
		AppendRecvText(_T("[仿真] 还没有可回放的轨迹，请先加载 G 代码"));
		return;
	}
	m_playing = true;
	m_playT0 = GetTickCount64();
	SetTimer(TIMER_PLAY, 30, nullptr);
	AppendRecvText(_T("[仿真] 开始回放"));
}

void CLearn1V1202609121037Dlg::OnBnClickedBtnStop()
{
	if (m_playing)
	{
		m_playing = false;
		KillTimer(TIMER_PLAY);
		AppendRecvText(_T("[仿真] 已停止"));
		InvalidateRect(&m_previewRect);
	}
}

/**********************************************
* 定时器：
*   TIMER_RX     兜底轮询接收缓冲区（平时由消息驱动）
*   TIMER_PLAY   按墙钟时间采样轨迹 -> 重画预览区
**********************************************/
void CLearn1V1202609121037Dlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == TIMER_RX)
	{
		// 把环形缓冲区里的行全部搬空（非阻塞）
		std::string line;
		while (m_serial.TryGetLine(line))
		{
			AppendRecvText(CA2T(line.c_str()));
		}
	}
	else if (nIDEvent == TIMER_PLAY && m_playing)
	{
		auto path = m_planner.GetLatest();			// 总是拿最新快照
		if (path)
		{
			m_path = path;
			double t = (double)(GetTickCount64() - m_playT0) / 1000.0;
			LookAheadPlanner::Sample(*m_path, t, m_playPos, m_playSpeed);

			if (t >= m_path->totalTime)				// 播完了
			{
				m_playing = false;
				KillTimer(TIMER_PLAY);
			}
			InvalidateRect(&m_previewRect);
		}
	}
}

/**********************************************
* 后台线程消息：串口有新行 -> 立即搬空缓冲区
**********************************************/
LRESULT CLearn1V1202609121037Dlg::OnSerialLine(WPARAM wParam, LPARAM lParam)
{
	std::string line;
	while (m_serial.TryGetLine(line))
	{
		AppendRecvText(CA2T(line.c_str()));
	}
	return 0;
}

/**********************************************
* 后台线程消息：规划完成（成功/失败都到这）
* lParam: 1=成功 0=失败；结果通过 GetLatest() 取快照
**********************************************/
LRESULT CLearn1V1202609121037Dlg::OnPlanDone(WPARAM wParam, LPARAM lParam)
{
	m_path = m_planner.GetLatest();
	if (lParam)
	{
		CString msg;
		msg.Format(_T("[规划完成] %d 段, 总时长 %.2f s, 预览点 %d 个"),
				   (int)m_path->segs.size(), m_path->totalTime,
				   (int)m_path->preview.size());
		AppendRecvText(msg);
	}
	else
	{
		CA2T errA(m_planner.LastError().c_str());
		AppendRecvText((CString)_T("[规划失败] ") + CString(errA));
	}
	InvalidateRect(&m_previewRect);
	return 0;
}

/**********************************************
* 加宽窗口 + 动态创建三个按钮 + 计算预览区
* （不动 .rc：控件全部 Create，窗口尺寸代码里调）
**********************************************/
void CLearn1V1202609121037Dlg::CreateUiExtras()
{
	// 原对话框按 320x200 对话框单位设计，先整体加宽到 950x540 像素
	SetWindowPos(nullptr, 0, 0, 950, 540, SWP_NOMOVE | SWP_NOZORDER);

	CRect rc;
	GetClientRect(&rc);
	int W = rc.Width(), H = rc.Height();

	// 预览区：客户区右侧大块
	m_previewRect = CRect(W - 400, 7, W - 100, H - 30);

	// 按钮列：最右侧
	auto mkBtn = [&](CButton& btn, UINT id, LPCTSTR text, int y) {
		btn.Create(text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
				   CRect(W - 92, y, W - 12, y + 28), this, id);
		btn.SetFont(GetFont());
	};
	mkBtn(m_btnLoad, IDC_BTN_LOADFILE, _T("加载G代码"), 7);
	mkBtn(m_btnPlay, IDC_BTN_PLAY,     _T("开始仿真"), 45);
	mkBtn(m_btnStop, IDC_BTN_STOP,     _T("停止仿真"), 83);
}

/**********************************************
* 枚举本机串口（QueryDosDevice 扫 COMn 设备名）+ 波特率表
**********************************************/
void CLearn1V1202609121037Dlg::InitPortOptions()
{
	CComboBox* pPort = (CComboBox*)GetDlgItem(IDC_COMBO_PORT);
	CComboBox* pBaud = (CComboBox*)GetDlgItem(IDC_COMBO_BAUD);
	if (pPort)
	{
		wchar_t names[65536];
		DWORD n = ::QueryDosDeviceW(nullptr, names, 65536);
		if (n > 0)
		{
			for (wchar_t* p = names; *p; p += wcslen(p) + 1)
			{
				if (wcsncmp(p, L"COM", 3) == 0 && iswdigit(p[3]))
				{
					pPort->AddString(CString(p));
				}
			}
		}
		if (pPort->GetCount() > 0) pPort->SetCurSel(0);
	}
	if (pBaud)
	{
		static const LPCTSTR bauds[] = { _T("9600"), _T("19200"), _T("38400"),
										 _T("57600"), _T("115200") };
		for (auto b : bauds) pBaud->AddString(b);
		pBaud->SetWindowText(_T("115200"));
	}
}

/**********************************************
* 向接收框尾部追加文本（只允许 UI 线程调用）
**********************************************/
void CLearn1V1202609121037Dlg::AppendRecvText(LPCTSTR text)
{
	CEdit* pEdit = (CEdit*)GetDlgItem(IDC_EDIT_RECEIVE);
	if (!pEdit) return;
	int len = pEdit->GetWindowTextLength();
	pEdit->SetSel(len, len);
	pEdit->ReplaceSel(text);
	pEdit->ReplaceSel(_T("\r\n"));
}

/**********************************************
* 轨迹预览绘制（UI 线程 OnPaint 内调用）
* 数据全部来自不可变快照 m_path，绘制本身无锁
**********************************************/
void CLearn1V1202609121037Dlg::DrawPreview(CDC& dc)
{
	CRect rc = m_previewRect;
	if (rc.IsRectEmpty()) return;

	// 背景 + 边框
	dc.FillSolidRect(rc, RGB(245, 246, 248));
	dc.Draw3dRect(rc, RGB(160, 160, 160), RGB(160, 160, 160));

	CString title(_T("轨迹预览 (XY 平面)"));
	dc.TextOutW(rc.left + 6, rc.top + 4, title);

	CRect plot(rc.left + 10, rc.top + 24, rc.right - 10, rc.bottom - 10);
	if (plot.Width() < 20 || plot.Height() < 20) return;

	auto path = m_path;
	if (!path || path->preview.size() < 2)
	{
		dc.TextOutW(plot.left + 10, plot.top + plot.Height() / 2,
					_T("点击\"加载G代码\"后自动规划并显示轨迹"));
		return;
	}

	// 世界坐标(mm) -> 屏幕：等比缩放 + 居中
	double w = path->maxX - path->minX;
	double h = path->maxY - path->minY;
	if (w < 1e-6) w = 1.0;
	if (h < 1e-6) h = 1.0;
	double scale = std::min((plot.Width() - 8) / w, (plot.Height() - 8) / h);
	double cx = (path->minX + path->maxX) * 0.5;
	double cy = (path->minY + path->maxY) * 0.5;
	auto toScr = [&](double x, double y, CPoint& pt) {
		pt.x = plot.CenterPoint().x + (int)((x - cx) * scale);
		pt.y = plot.CenterPoint().y - (int)((y - cy) * scale);   // Y 向上
	};

	// 完整路径折线（规划线程已采好样）
	CPen penPath(PS_SOLID, 1, RGB(40, 100, 220));
	CPen* old = dc.SelectObject(&penPath);
	CPoint pt;
	for (size_t i = 0; i < path->preview.size(); ++i)
	{
		toScr(path->preview[i][0], path->preview[i][1], pt);
		if (i == 0) dc.MoveTo(pt); else dc.LineTo(pt);
	}

	// 起点(绿) / 终点(红) 标记
	CPoint ps, pe;
	toScr(path->preview.front()[0], path->preview.front()[1], ps);
	toScr(path->preview.back()[0],  path->preview.back()[1],  pe);
	dc.FillSolidRect(ps.x - 4, ps.y - 4, 8, 8, RGB(0, 160, 60));
	dc.FillSolidRect(pe.x - 4, pe.y - 4, 8, 8, RGB(210, 40, 40));

	// 回放当前点（黑圆）+ 速度文字
	if (m_playing || m_playSpeed > 0.0)
	{
		CPoint pc;
		toScr(m_playPos[0], m_playPos[1], pc);
		CBrush br(RGB(20, 20, 20));
		CBrush* oldBr = dc.SelectObject(&br);
		dc.Ellipse(pc.x - 5, pc.y - 5, pc.x + 5, pc.y + 5);
		dc.SelectObject(oldBr);

		CString info;
		info.Format(_T("X=%.2f  Y=%.2f  V=%.1f mm/s"), m_playPos[0], m_playPos[1], m_playSpeed);
		dc.TextOutW(plot.left + 4, plot.bottom - 18, info);
	}
	dc.SelectObject(old);
}

/**********************************************
* 析构清理：停定时器、停两个后台线程（幂等，可重复调用）
**********************************************/
void CLearn1V1202609121037Dlg::OnDestroy()
{
	KillTimer(TIMER_RX);
	KillTimer(TIMER_PLAY);
	m_playing = false;
	m_serial.ClosePort();		// Stop -> join -> close，最多等一个读超时(50ms)
	m_planner.Shutdown();		// 唤醒任务队列 -> join
	CDialogEx::OnDestroy();
}
