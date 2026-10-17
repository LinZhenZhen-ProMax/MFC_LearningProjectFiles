/*****************************************
* WorkerThread.h
* 工作线程基类（header-only，无 MFC 依赖）
*
* 职责：统一封装线程的"启停 / 等待退出 / 异常捕获"，
*       子类只需实现 Run() 循环体。
*
* 线程模型约定：
*   1. Start()  启动 std::thread，异常在 Trampoline 中被
*      std::current_exception 捕获，交由 OnThreadException
*      处理（默认吞掉并记录），保证线程体不向外抛异常——
*      std::thread 函数向外抛异常会直接 std::terminate。
*   2. Stop()   先置停止标志，再 join 等待。子类 Run() 里
*      必须周期性检查 IsStopRequested()，或阻塞在带超时的
*      RingBuffer::pop 上（close 会唤醒），确保能在有限
*      时间内退出。
*   3. 析构函数自动调用 Stop()，杜绝"忘 join 导致
*      std::terminate"的经典崩溃。
*****************************************/
#pragma once

#include <thread>
#include <atomic>
#include <exception>
#include <string>

class WorkerThread
{
public:
    WorkerThread() = default;
    virtual ~WorkerThread() { Stop(); }

    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

    bool Start()
    {
        if (m_thread.joinable()) return false;   // 已在运行
        m_stopRequested = false;
        try
        {
            m_thread = std::thread(&WorkerThread::Trampoline, this);
        }
        catch (const std::system_error&)
        {
            m_stopRequested = true;
            return false;                        // 线程资源耗尽等
        }
        return true;
    }

    // 请求停止并 join；可安全地重复调用
    void Stop() noexcept
    {
        m_stopRequested = true;
        if (m_thread.joinable())
        {
            try { m_thread.join(); }
            catch (...) { /* join 期间不应抛出；兜底防止析构向外抛 */ }
        }
    }

    bool  IsRunning()        const { return m_thread.joinable(); }
    bool  IsStopRequested()  const { return m_stopRequested.load(std::memory_order_relaxed); }

protected:
    // 子类实现的工作循环；实现中必须能响应 IsStopRequested()
    virtual void Run() = 0;

    // 线程体异常兜底；默认吞掉（避免 terminate），子类可覆写上报 UI
    virtual void OnThreadException(const std::exception_ptr& e)
    {
        (void)e;
    }

private:
    static void Trampoline(WorkerThread* self) noexcept
    {
        try
        {
            self->Run();
        }
        catch (...)
        {
            // 异常安全：线程体任何未捕获异常都在这里收口
            self->OnThreadException(std::current_exception());
        }
    }

    std::thread          m_thread;
    std::atomic<bool>    m_stopRequested{ true };
};
