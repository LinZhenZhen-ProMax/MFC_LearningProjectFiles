/*****************************************
* RingBuffer.h
* 线程安全环形缓冲区（header-only，无 MFC 依赖）
*
* 用途：
*   在后台工作线程（串口 IO / 规划器）与 UI 主线程之间
*   传递数据，替代"直接跨线程访问控件/成员"的危险做法。
*
* 线程模型：
*   - 内部一把 std::mutex + std::condition_variable 保护，
*     支持 MPMC（任意线程 push / 任意线程 pop），
*     对本项目"单生产者-单消费者"的场景绰绰有余；
*   - push：缓冲区满时**丢弃最旧数据**并计数（遥测类数据
*     宁可丢旧也要保新，丢弃数可用 dropped() 观测）；
*   - pop：支持带超时的阻塞等待，避免轮询空转；
*   - close()：唤醒所有等待者并使其 pop 返回 false，
*     用于线程停止时的优雅退出（防止 join 卡死）。
*****************************************/
#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <cstddef>

template <typename T>
class RingBuffer
{
public:
    explicit RingBuffer(size_t capacity = 1024)
        : m_buf(capacity > 0 ? capacity : 1)
        , m_cap(capacity > 0 ? capacity : 1)
    {
    }

    // 禁止拷贝（内部有锁，拷贝无意义且危险）
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    /**********************************************
    * push：入队一个元素
    * 满时丢弃最旧元素（m_dropped++），永远返回 true
    * （本项目的数据流是"新数据更有价值"，故采用丢旧策略）
    **********************************************/
    void push(const T& item)
    {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_buf[m_head] = item;
            m_head = (m_head + 1) % m_cap;
            if (m_count < m_cap)
            {
                ++m_count;
            }
            else
            {
                m_tail = (m_tail + 1) % m_cap; // 满了：挤掉最旧的
                ++m_dropped;
            }
        }
        m_cv.notify_one();
    }

    /**********************************************
    * pop：带超时的阻塞出队
    * 返回 true 表示取到数据；false 表示超时或已 close
    **********************************************/
    bool pop(T& out, unsigned timeoutMs)
    {
        std::unique_lock<std::mutex> lk(m_mtx);
        bool ok = m_cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
            [this] { return m_count > 0 || m_closed; });

        if (!ok || m_count == 0)   // 超时或被 close 唤醒但已无数据
        {
            return false;
        }

        out = std::move(m_buf[m_tail]);
        m_tail = (m_tail + 1) % m_cap;
        --m_count;
        return true;
    }

    // 非阻塞尝试取一个
    bool tryPop(T& out)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_count == 0) return false;
        out = std::move(m_buf[m_tail]);
        m_tail = (m_tail + 1) % m_cap;
        --m_count;
        return true;
    }

    // 关闭缓冲区：所有阻塞中的 pop 立即返回 false（用于线程停止）
    void close()
    {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_closed = true;
        }
        m_cv.notify_all();
    }

    bool   closed() const { std::lock_guard<std::mutex> lk(m_mtx); return m_closed; }
    size_t size()   const { std::lock_guard<std::mutex> lk(m_mtx); return m_count; }
    size_t dropped() const { std::lock_guard<std::mutex> lk(m_mtx); return m_dropped; }
    void   clear()
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_head = m_tail = m_count = 0;
    }

private:
    mutable std::mutex      m_mtx;
    std::condition_variable m_cv;
    std::vector<T>          m_buf;
    size_t                  m_head = 0;   // 下一个写入位置
    size_t                  m_tail = 0;   // 下一个读取位置
    size_t                  m_count = 0;
    size_t                  m_cap = 0;
    bool                    m_closed = false;
    size_t                  m_dropped = 0;
};
