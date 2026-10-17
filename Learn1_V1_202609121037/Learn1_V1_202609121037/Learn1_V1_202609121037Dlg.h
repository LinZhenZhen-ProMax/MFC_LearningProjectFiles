
// Learn1_V1_202609121037Dlg.h: 头文件
//

#pragma once

#include "SerialWorker.h"
#include "PlannerWorker.h"
#include "LookAheadPlanner.h"
#include <memory>

// CLearn1V1202609121037Dlg 对话框
class CLearn1V1202609121037Dlg : public CDialogEx
{
// 构造
public:
	CLearn1V1202609121037Dlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_LEARN1_V1_202609121037_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonOpenserial();
	afx_msg void OnCbnSelchangeComboPort();
	afx_msg void OnBnClickedButtonSend();
	void CLearn1V1202609121037Dlg::InitSystemMenu();
	CString m_strSend;
	CString m_strRecv;

	/*==================== 分支新增：多线程 + 轨迹仿真 ====================*/
private:
	// 工作线程向 UI 投递的自定义消息（PostMessage 跨线程安全）
	static const UINT WM_APP_SERIAL_LINE = WM_APP + 1;	// 串口收到新行
	static const UINT WM_APP_PLAN_DONE   = WM_APP + 2;	// 规划完成(成败均通知)

	// 定时器
	enum { TIMER_RX = 1,        // 收串口数据（兜底轮询，100ms）
		   TIMER_PLAY = 2 };    // 轨迹回放（30ms，约33fps）

	// 动态创建的按钮 ID（不改 .rc，控件在 OnInitDialog 里 Create）
	enum { IDC_BTN_LOADFILE = 2001, IDC_BTN_PLAY = 2002, IDC_BTN_STOP = 2003 };

	afx_msg void OnBnClickedBtnLoadfile();
	afx_msg void OnBnClickedBtnPlay();
	afx_msg void OnBnClickedBtnStop();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg LRESULT OnSerialLine(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPlanDone(WPARAM wParam, LPARAM lParam);

	void CreateUiExtras();			// 加宽窗口 + 动态按钮 + 预览区布局
	void InitPortOptions();			// 枚举本机串口 / 波特率下拉框
	void AppendRecvText(LPCTSTR text);	// 向接收框追加一行（线程外仅 UI 调）
	void DrawPreview(CDC& dc);		// 右侧轨迹预览绘制（仅 UI 线程）

	// ---- 后台工作线程 ----
	SerialWorker   m_serial;	// 串口 IO 线程（收发均不阻塞 UI）
	PlannerWorker  m_planner;	// 轨迹规划线程（前瞻 + 加减速计算）
	PlannerConfig  m_cfg;		// 规划参数（加速度/加加速度/允差等）

	// ---- 轨迹预览状态（仅 UI 线程读写）----
	std::shared_ptr<const PlannedPath> m_path;	// 最新规划结果快照
	bool     m_playing = false;					// 是否正在回放
	ULONGLONG m_playT0 = 0;						// 回放起点时刻(ms)
	double   m_playPos[3] = { 0, 0, 0 };		// 回放当前点
	double   m_playSpeed = 0.0;					// 回放当前速度 mm/s
	CRect    m_previewRect;						// 预览绘制区域(客户区坐标)
	CButton  m_btnLoad, m_btnPlay, m_btnStop;	// 动态按钮
	int      m_jobSeq = 0;						// 规划任务流水号
};
