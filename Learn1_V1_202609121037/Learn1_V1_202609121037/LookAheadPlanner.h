/*****************************************
* LookAheadPlanner.h
* 连续轨迹前瞻（Look-Ahead）速度规划器
*
* 位置：MotionSegment（几何段） ──► LookAheadPlanner ──► PlannedPath（时间参数化轨迹）
*       （由 GCodeModalMachine 解析产出）              （含每段速度轮廓 + 预览采样点）
*
* 算法流程（经典 GRBL 式前瞻，三步）：
*   1. 交接点限速：相邻两段方向夹角越大，允许通过的速度越低
*      （用 junction_deviation 偏差模型估算拐角等效半径）；
*   2. 后向传递（backward pass）：从轨迹终点往回扫，
*      保证每段"入口速度"都刹得住后面的路（v² ≤ v_next² + 2aL）；
*   3. 正向生成轮廓：对每段按 梯形 / S 型（7 段简化为
*      加加速-匀加速-减加速-匀速-…）曲线计算相位时长，
*      峰值速度用二分搜索贴合段长。
*
* 已知限制（学习项目，明确标注）：
*   - G02/G03 圆弧段暂按"弦线"直线处理（方向取起点→终点）；
*   - 未做刀具半径补偿 / 工件坐标系偏移（ApplyOffset 待实现）。
*****************************************/
#pragma once

#include "pch.h"
#include "GCodeModalMachine.h"

#include <string>
#include <vector>
#include <array>

/**********************************************
* 规划参数（可在 UI 层调整后随任务提交）
* 单位约定：长度 mm，速度 mm/s，加速度 mm/s^2，加加速度 mm/s^3
* G 代码里 F 的单位是 mm/min，规划器入口统一换算 /60
**********************************************/
struct PlannerConfig
{
    double aMax            = 200.0;    // 最大加速度
    double jMax            = 5000.0;   // 最大加加速度（S 型用）
    double junctionDeviation = 0.05;   // 拐角允差(mm)：越小拐角越"尖"、越保守
    double defaultFeed     = 1500.0;   // 程序未给 F 时的默认进给 (mm/min)
    double sampleDt        = 0.005;    // 预览轨迹采样步长 (s)
    bool   useSCurve       = true;     // true=S型，false=梯形
};

/**********************************************
* 单段规划结果
* 相位结构（S 型为完整三段 jerk 相位；梯形时 tj=0）：
*   [加加速 tj1 | 匀加速 tcA | 减加速 tj1] [匀速 tCruise]
*   [加减速 tj2 | 匀减速 tcD | 减减速 tj2]   （对称）
**********************************************/
struct PlannedSegment
{
    GCodeModalMachine::MotionSegment geom;  // 原始几何段（起点/终点/类型/行号）

    double length = 0.0;   // 弦长 mm
    double v0 = 0.0;       // 入口速度 mm/s
    double v1 = 0.0;       // 出口速度 mm/s
    double vc = 0.0;       // 巡航峰值速度 mm/s
    double a  = 0.0;       // 本段实际使用的加速度
    double tj1 = 0.0, tcA = 0.0;    // 加速侧 jerk 相位时长 / 匀加速时长
    double tj2 = 0.0, tcD = 0.0;    // 减速侧
    double tCruise = 0.0;
    double totalT = 0.0;   // 本段总时长
    double tStart = 0.0;   // 在整条轨迹中的起始时刻
};

/**********************************************
* 整条轨迹的规划结果（不可变快照，UI 与后台共享）
**********************************************/
struct PlannedPath
{
    std::vector<PlannedSegment> segs;
    double totalTime = 0.0;                       // 总时长 s
    std::vector<std::array<double, 3>> preview;   // 预览折线采样点 (mm)
    double minX = 0, maxX = 0, minY = 0, maxY = 0; // XY 包围盒（画图用）
    int    jobId = 0;
};

class LookAheadPlanner
{
public:
    /**********************************************
    * 主入口：把运动段序列规划成时间参数化轨迹
    * 失败时返回 false 并填写 err（供 UI 显示）
    **********************************************/
    bool Plan(const std::vector<GCodeModalMachine::MotionSegment>& segs,
              const PlannerConfig& cfg,
              PlannedPath& out,
              std::string& err);

    /**********************************************
    * 实时仿真采样：取轨迹在 t 时刻的位置与速度
    * 纯函数、无状态，UI 定时器线程可直接调用
    * （t 超出总时长时返回终点，速度为 0）
    **********************************************/
    static void Sample(const PlannedPath& path, double t,
                       double pos[3], double& speed);

private:
    // ---- 几何预处理 ----
    static double SegLength(const GCodeModalMachine::MotionSegment& s);
    static void   SegDir(const GCodeModalMachine::MotionSegment& s, double u[3]);

    // ---- 前瞻三步 ----
    static std::vector<double> JunctionLimits(
        const std::vector<GCodeModalMachine::MotionSegment>& segs,
        const std::vector<double>& vmax,
        const PlannerConfig& cfg);

    static void BackwardPass(const std::vector<double>& L,
                             const std::vector<double>& vJunction,
                             double aMax,
                             std::vector<double>& vExit);   // vExit[i] = 段 i 的出口限速

    // ---- 单段速度轮廓 ----
    // 给定 v0/v1/峰值上限，搜索满足段长 L 的峰值 vc，并填相位时长
    static bool BuildProfile(double L, double v0, double v1, double vmaxSeg,
                             double aMax, double jMax, bool useSCurve,
                             PlannedSegment& out);

    struct AccelPart { double tj = 0, tc = 0, a = 0, dist = 0; };
    static AccelPart AccelDist(double vFrom, double vTo, double aMax, double jMax);
};
