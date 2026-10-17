/*****************************************
* PlannerWorker.cpp
* 轨迹规划后台工作线程实现
*****************************************/
#include "pch.h"
#include "PlannerWorker.h"

void PlannerWorker::Submit(Job&& job)
{
    {
        std::lock_guard<std::mutex> lk(m_jobMtx);
        m_jobs.clear();               // 覆盖式：旧任务没意义，只规划最新的
        m_jobs.push_back(std::move(job));
        m_wakeup = false;
    }
    m_jobCv.notify_one();

    if (!IsRunning())
    {
        WorkerThread::Start();        // 懒启动：第一次提交任务时才开线程
    }
}

std::shared_ptr<const PlannedPath> PlannerWorker::GetLatest() const
{
    std::lock_guard<std::mutex> lk(m_resultMtx);
    return m_latest;
}

std::string PlannerWorker::LastError() const
{
    std::lock_guard<std::mutex> lk(m_resultMtx);
    return m_lastErr;
}

void PlannerWorker::Shutdown() noexcept
{
    {
        std::lock_guard<std::mutex> lk(m_jobMtx);
        m_wakeup = true;              // 让 Run 里的 wait 立刻醒来并退出
        m_jobs.clear();
    }
    m_jobCv.notify_all();
    WorkerThread::Stop();
}

void PlannerWorker::Run()
{
    for (;;)
    {
        /******************************************
        * 1) 等任务（带谓词的 wait 防虚假唤醒）
        ******************************************/
        Job job;
        {
            std::unique_lock<std::mutex> lk(m_jobMtx);
            m_jobCv.wait(lk, [this] { return m_wakeup || !m_jobs.empty(); });
            if (m_wakeup && m_jobs.empty()) return;   // 关闭：优雅退出
            job = std::move(m_jobs.front());
            m_jobs.pop_back();
        }

        /******************************************
        * 2) 后台执行规划（重计算不碰 UI）
        *    异常安全：任何异常都被捕获并转为错误上报，
        *    保证规划线程本身永不崩
        ******************************************/
        PlannedPath path;
        std::string err;
        bool ok = false;
        try
        {
            ok = m_planner.Plan(job.segs, job.cfg, path, err);
        }
        catch (const std::exception& e)
        {
            ok = false;
            err = std::string("规划器异常: ") + e.what();
        }
        catch (...)
        {
            ok = false;
            err = "规划器发生未知异常";
        }

        if (ok) path.jobId = job.id;

        /******************************************
        * 3) 发布结果快照 + 通知 UI
        ******************************************/
        {
            std::lock_guard<std::mutex> lk(m_resultMtx);
            m_lastErr = err;
            if (ok)
            {
                m_latest = std::make_shared<const PlannedPath>(std::move(path));
            }
            // 失败时保留上一次的 m_latest，UI 可继续显示旧轨迹
        }

        if (m_hNotify && m_msgPlanDone)
        {
            ::PostMessage(m_hNotify, m_msgPlanDone,
                          (WPARAM)job.id, ok ? 1 : 0);
        }
    }
}
