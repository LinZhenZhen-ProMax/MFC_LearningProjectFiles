/*****************************************
* PlannerWorker.h
* 轨迹规划后台工作线程
*
* 线程模型与数据流向：
*
*   UI 主线程                        规划线程 (Run)
*   ─────────                        ─────────────
*   Submit(job) ──► m_jobQueue ────► LookAheadPlanner::Plan
*                                     （重计算全部在后台）
*   GetLatest() ◄── m_latest ◄────── 规划完成发布 PlannedPath 快照
*   WM plan-done ◄── PostMessage ◄── 通知 UI 刷新预览
*
*   - PlannedPath 以 std::shared_ptr<const ...> 发布：
*     后台只换指针，UI 读旧快照不受影响（读写不冲突）；
*   - 任务队列 mutex + condition_variable，Close 时唤醒退出。
*****************************************/
#pragma once

#include "pch.h"
#include "WorkerThread.h"
#include "LookAheadPlanner.h"

#include <windows.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <memory>

class PlannerWorker : public WorkerThread
{
public:
    struct Job
    {
        int id = 0;                                        // 任务序号（UI 区分新旧结果）
        std::vector<GCodeModalMachine::MotionSegment> segs;// 待规划运动段
        PlannerConfig cfg;                                 // 规划参数
    };

    PlannerWorker() = default;
    ~PlannerWorker() override { Shutdown(); }

    /**********************************************
    * 绑定通知目标：
    * msgPlanDone : 规划完成 (WPARAM=jobId, LPARAM=1成功/0失败)
    * msgPlanError: 失败详情指针随消息带出（见 m_lastErr）
    **********************************************/
    void SetNotify(HWND hNotify, UINT msgPlanDone)
    {
        m_hNotify = hNotify;
        m_msgPlanDone = msgPlanDone;
    }

    // UI 线程调用：提交规划任务（覆盖式：只保留最新一个待规划任务）
    void Submit(Job&& job);

    // UI 线程调用：取最新规划结果快照（无锁竞争窗口极短，内部 mutex）
    std::shared_ptr<const PlannedPath> GetLatest() const;

    // 最近的错误信息（UI 在收到失败通知后读取）
    std::string LastError() const;

    // 停止线程并清空队列
    void Shutdown() noexcept;

protected:
    void Run() override;                       // 后台规划循环

private:
    mutable std::mutex                 m_jobMtx;
    std::condition_variable            m_jobCv;
    std::deque<Job>                    m_jobs;         // 只保留最新，长度<=1
    bool                               m_wakeup = false; // Shutdown 唤醒标志

    mutable std::mutex                 m_resultMtx;
    std::shared_ptr<const PlannedPath> m_latest;       // 最新规划结果快照
    std::string                        m_lastErr;

    LookAheadPlanner                   m_planner;
    HWND                               m_hNotify = nullptr;
    UINT                               m_msgPlanDone = 0;
};
