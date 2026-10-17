/*****************************************
* SerialWorker.cpp
* 串口后台工作线程实现
*****************************************/
#include "pch.h"
#include "SerialWorker.h"

bool SerialWorker::OpenPort(const char* portName, int baud,
                            HWND hNotify, UINT msgDataReady)
{
    if (IsRunning())          // 已在运行，先停
    {
        ClosePort();
    }

    if (!m_port.openserial(portName, baud))
    {
        return false;         // 打开失败：句柄仍为 INVALID_HANDLE_VALUE
    }

    m_hNotify      = hNotify;
    m_msgDataReady = msgDataReady;
    m_open         = true;

    if (!WorkerThread::Start())
    {
        m_open = false;
        m_port.close();
        return false;
    }
    return true;
}

void SerialWorker::ClosePort() noexcept
{
    // 顺序很重要：
    // 1) 置停止标志 —— Run 循环最多等一个 50ms 读超时即退出；
    // 2) join 线程，保证之后没人再碰 m_port；
    // 3) 关闭句柄。
    WorkerThread::Stop();
    m_open = false;
    m_port.close();
}

bool SerialWorker::SendLine(const std::string& line)
{
    if (!IsPortOpen()) return false;
    {
        std::lock_guard<std::mutex> lk(m_txMtx);
        m_txQueue.push_back(line);
        if (m_txQueue.size() > 256)      // 发送队列限长，防止 UI 灌爆内存
        {
            m_txQueue.pop_front();
        }
    }
    return true;
}

void SerialWorker::FlushTx()
{
    std::deque<std::string> local;
    {
        std::lock_guard<std::mutex> lk(m_txMtx);
        local.swap(m_txQueue);           // 快速换出，缩短持锁时间
    }
    for (const auto& line : local)
    {
        auto r = m_port.writeserial(line.data(), (int)line.size());
        (void)r;   // 发送失败暂不重传；错误码在 r.errorcode 里，可扩展上报
    }
}

void SerialWorker::Run()
{
    char raw[256];

    while (!IsStopRequested())
    {
        /**************************************************
        * 1) 读串口（SerialPort 内部读超时 50ms，
        *    因此本循环天然具备停止响应性）
        **************************************************/
        auto r = m_port.readserial(raw, sizeof(raw));
        if (r.ok() && r.Byterecv > 0)
        {
            for (int k = 0; k < r.Byterecv; ++k)
            {
                char c = raw[k];
                if (c == '\n' || c == '\r')
                {
                    if (!m_lineBuf.empty())
                    {
                        m_rxBuf.push(m_lineBuf);          // 完整一行入队
                        m_lineBuf.clear();
                        if (m_hNotify && m_msgDataReady)
                        {
                            // 通知 UI：有新数据（PostMessage 跨线程安全，
                            // 即使窗口忙，消息也会排队，不阻塞本线程）
                            ::PostMessage(m_hNotify, m_msgDataReady,
                                          (WPARAM)m_rxBuf.dropped(), 0);
                        }
                    }
                }
                else if (m_lineBuf.size() < 512)          // 单行限长，防异常数据
                {
                    m_lineBuf.push_back(c);
                }
            }
        }
        // 打开失败 / 读错误：r.ok() 为 false 时本循环靠停止标志退出

        /**************************************************
        * 2) 顺带处理发送队列（单线程收发复用一个循环，
        *    避免读写并发争用同一句柄）
        **************************************************/
        FlushTx();
    }

    // 退出前把拼了一半的行丢掉（半行数据没有意义）
    m_lineBuf.clear();
}
