/*****************************************
* SerialWorker.h
* 串口后台工作线程
*
* 线程模型与数据流向：
*
*   UI 主线程                          后台 IO 线程 (Run)
*   ─────────                          ──────────────────
*   SendLine(text) ──► m_txQueue ────► 取出发送 writeserial
*                                      循环 readserial(50ms 超时)
*   GetDropped/…  ◄── m_rxBuf  ◄────── 拼行后 push 完整行
*   OnSerialLine ◄── PostMessage ◄──── 有新行时通知
*
*   - UI 永不直接读写串口句柄；串口打开/关闭也由本类内部管理；
*   - 收到的完整行进入 RingBuffer<string>，UI 用定时器或
*     消息驱动去取（取不到也不阻塞界面）；
*   - 串口读操作自带 50ms 超时，因此停止线程时最多 50ms 即可 join。
*****************************************/
#pragma once

#include "pch.h"        // MFC 预编译头（cpp 需要它，头文件保持轻量）
#include "WorkerThread.h"
#include "RingBuffer.h"
#include "SerialPort.h"

#include <windows.h>
#include <string>
#include <deque>
#include <mutex>

class SerialWorker : public WorkerThread
{
public:
    SerialWorker() : m_rxBuf(1024) {}
    ~SerialWorker() override { ClosePort(); }

    /**********************************************
    * 打开串口并启动后台 IO 线程
    * hNotify : 接收通知的窗口（UI 线程窗口句柄）
    * msgDataReady : 收到新行时 PostMessage(hNotify, msgDataReady, dropped, 0)
    * 返回 false 表示串口打开失败（例如被占用 / 不存在）
    **********************************************/
    bool OpenPort(const char* portName, int baud,
                  HWND hNotify, UINT msgDataReady);

    // 停止线程并关闭串口；可安全重复调用
    void ClosePort() noexcept;

    bool IsPortOpen() const { return m_open.load(); }

    /**********************************************
    * UI 线程调用：把一行文本投入发送队列（自动补 \n）
    * 线程安全；串口未打开时返回 false
    **********************************************/
    bool SendLine(const std::string& line);

    // UI 线程调用：非阻塞取一条接收到的完整行（取不到返回 false）
    bool TryGetLine(std::string& out) { return m_rxBuf.tryPop(out); }

    // 观测：满溢丢弃的行数（诊断用）
    size_t GetDroppedLines() const { return m_rxBuf.dropped(); }

protected:
    void Run() override;   // IO 主循环（后台线程）

private:
    void FlushTx();        // 把发送队列写到串口（仅 IO 线程调用）

    SerialPort             m_port;       // 现有串口封装，仅 IO 线程触碰
    std::atomic<bool>      m_open{ false };

    // 发送队列：UI push，IO 线程 pop（mutex 保护）
    std::mutex             m_txMtx;
    std::deque<std::string> m_txQueue;

    // 接收行缓冲：IO 线程 push，UI pop
    RingBuffer<std::string> m_rxBuf;

    // 行拼装缓冲（仅 IO 线程使用，无需加锁）
    std::string            m_lineBuf;

    // 通知目标（窗口句柄仅用于 PostMessage，跨线程安全）
    HWND                   m_hNotify = nullptr;
    UINT                   m_msgDataReady = 0;
};
