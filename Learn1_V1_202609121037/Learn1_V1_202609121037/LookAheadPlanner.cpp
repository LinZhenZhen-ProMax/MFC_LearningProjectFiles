/*****************************************
* LookAheadPlanner.cpp
* 前瞻规划与加减速轮廓算法实现
*****************************************/
#include "pch.h"
#include "LookAheadPlanner.h"

#include <cmath>
#include <algorithm>

static const double kEps = 1e-9;

/**********************************************
* 几何：段长（弦长）与方向单位向量
* G02/G03 目前按弦线近似 —— 圆弧几何参数（I/J/K/R）
* 尚未由解释器填入，注释明确标注该限制
**********************************************/
double LookAheadPlanner::SegLength(const GCodeModalMachine::MotionSegment& s)
{
    double dx = s.x1 - s.x0, dy = s.y1 - s.y0, dz = s.z1 - s.z0;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void LookAheadPlanner::SegDir(const GCodeModalMachine::MotionSegment& s, double u[3])
{
    double L = SegLength(s);
    if (L < kEps) { u[0] = u[1] = u[2] = 0.0; return; }
    u[0] = (s.x1 - s.x0) / L;
    u[1] = (s.y1 - s.y0) / L;
    u[2] = (s.z1 - s.z0) / L;
}

/**********************************************
* 第 1 步：交接点限速
* GRBL 式拐角允差模型：两段方向夹角越大，等效拐角半径越小，
* 允许通过的速度 v = sqrt(a * r) 越低。
* r 由 junction_deviation（轨迹偏离理想拐角的最大允差）估算。
**********************************************/
std::vector<double> LookAheadPlanner::JunctionLimits(
    const std::vector<GCodeModalMachine::MotionSegment>& segs,
    const std::vector<double>& vmax,
    const PlannerConfig& cfg)
{
    size_t n = segs.size();
    std::vector<double> vJ(n > 0 ? n - 1 : 0);

    for (size_t i = 0; i + 1 < n; ++i)
    {
        double u1[3], u2[3];
        SegDir(segs[i], u1);
        SegDir(segs[i + 1], u2);

        double dot = u1[0] * u2[0] + u1[1] * u2[1] + u1[2] * u2[2];
        dot = std::max(-1.0, std::min(1.0, dot));

        // 方向近乎反转（180 度掉头）：必须刹停
        if (dot < -0.99)
        {
            vJ[i] = 0.0;
            continue;
        }

        // 转向角 alpha：0 = 直线通过
        double alpha = std::acos(dot);
        double sinHalf = std::sin(alpha * 0.5);

        double vLimit;
        if (sinHalf < 1e-6)
        {
            vLimit = std::min(vmax[i], vmax[i + 1]);   // 共线，不限速
        }
        else
        {
            // GRBL 拐角公式：由允差反推等效半径下的可通过速度
            double v = std::sqrt(cfg.aMax * cfg.junctionDeviation *
                                 sinHalf / (1.0 - sinHalf));
            vLimit = std::min(v, std::min(vmax[i], vmax[i + 1]));
        }
        vJ[i] = vLimit;
    }
    return vJ;
}

/**********************************************
* 第 2 步：后向传递
* 从终点（必为 0 速度）往回扫：vExit[i] 取
* min(交接点限速, 在下一段里刹得住的速度上限)
* —— 保证正向前瞻时每一段的出口速度都能在后续路段刹停
**********************************************/
void LookAheadPlanner::BackwardPass(const std::vector<double>& L,
                                    const std::vector<double>& vJunction,
                                    double aMax,
                                    std::vector<double>& vExit)
{
    size_t n = L.size();
    vExit.assign(n, 0.0);
    if (n == 0) return;

    vExit[n - 1] = 0.0;                       // 终点刹停
    for (int i = (int)n - 2; i >= 0; --i)
    {
        // 在段 i+1 内从 vExit[i] 刹到 vExit[i+1] 需要
        // (v^2 - vExit[i+1]^2) / (2a) <= L[i+1]
        double vBrake = std::sqrt(vExit[i + 1] * vExit[i + 1] + 2.0 * aMax * L[i + 1]);
        vExit[i] = std::min(vJunction[i], vBrake);
    }
}

/**********************************************
* 单段"加速/减速部分"的解析距离（vFrom -> vTo, Δv >= 0）
* S 型三相位（加加速/匀加速/减加速）；jMax 取极大值时退化为梯形
* 返回：tj（jerk 相位时长）、tc（匀加速时长）、a（实际峰值加速度）、dist
**********************************************/
LookAheadPlanner::AccelPart LookAheadPlanner::AccelDist(double vFrom, double vTo,
                                                        double aMax, double jMax)
{
    AccelPart p;
    double dv = vTo - vFrom;
    if (dv <= kEps) return p;                 // 无需变速

    if (dv >= aMax * aMax / jMax)
    {
        // 完整三相位：能摸到峰值加速度 aMax
        p.tj = aMax / jMax;
        p.tc = dv / aMax - p.tj;
        p.a  = aMax;
    }
    else
    {
        // 短变速：摸不到 aMax，由 jerk 直接堆出 Δv
        p.tj = std::sqrt(dv / jMax);
        p.tc = 0.0;
        p.a  = p.tj * jMax;
    }
    p.dist = (vFrom + vTo) * 0.5 * (2.0 * p.tj + p.tc);   // 对称 jerk 相位均值公式
    return p;
}

/**********************************************
* 第 3 步：单段轮廓生成
* 在 [max(v0,v1), vmaxSeg] 上二分搜索巡航峰值 vc，
* 使 加速距离+减速距离+匀速时间*vc == L
* 返回 false 表示该段物理上塞不下（长度太短），调用方可放宽 v1 重试
**********************************************/
bool LookAheadPlanner::BuildProfile(double L, double v0, double v1, double vmaxSeg,
                                    double aMax, double jMax, bool useSCurve,
                                    PlannedSegment& out)
{
    double j = useSCurve ? jMax : 1e12;       // 梯形 = jerk 无穷大
    out.v0 = v0;
    out.v1 = v1;
    out.a  = aMax;

    if (L < kEps)
    {
        out.vc = out.tj1 = out.tcA = out.tj2 = out.tcD = out.tCruise = out.totalT = 0.0;
        return true;
    }

    auto totalDist = [&](double vp) {
        return AccelDist(v0, vp, aMax, j).dist + AccelDist(v1, vp, aMax, j).dist;
    };

    double lo = std::max(v0, v1);
    double hi = std::max(lo, vmaxSeg);

    // S 型曲线的变速距离比梯形长，可能塞不下：让出口速度 v1 向入口 v0 收敛重试
    // （v1 == v0 时本段恒可行：无需变速，全程匀速即可）
    // 该放宽会沿正向传递：下一段的 v0 就用放宽后的 v1，保持衔接一致
    for (int relax = 0; relax < 40 && totalDist(std::max(v0, v1)) > L + 1e-6; ++relax)
    {
        v1 = 0.5 * (v1 + v0);
        out.v1 = v1;
    }
    if (totalDist(std::max(v0, v1)) > L + 1e-6)
    {
        return false;                          // 段长实在不够（异常输入）
    }

    // 松弛可能改变了 v1，搜索区间下界必须重算（二分不变式要求 total(lo) <= L）
    lo = std::max(v0, v1);
    hi = std::max(lo, vmaxSeg);

    double vp;
    if (totalDist(hi) <= L + 1e-6)
    {
        vp = hi;                               // 能跑到段限速
    }
    else
    {
        // 二分搜索最大可行峰值
        for (int it = 0; it < 64; ++it)
        {
            double mid = 0.5 * (lo + hi);
            if (totalDist(mid) <= L) lo = mid; else hi = mid;
        }
        vp = lo;
    }

    AccelPart acc = AccelDist(v0, vp, aMax, j);
    AccelPart dec = AccelDist(v1, vp, aMax, j);
    double dAccDec = acc.dist + dec.dist;
    double tCruise = (vp > kEps) ? std::max(0.0, (L - dAccDec) / vp) : 0.0;

    out.vc = vp;
    out.tj1 = acc.tj;  out.tcA = acc.tc;
    out.tj2 = dec.tj;  out.tcD = dec.tc;
    out.a   = std::max(acc.a, dec.a);
    out.tCruise = tCruise;
    out.totalT = 2 * acc.tj + acc.tc + tCruise + 2 * dec.tj + dec.tc;
    return true;
}

/**********************************************
* 主入口：三步前瞻 + 逐段轮廓 + 预览采样
**********************************************/
bool LookAheadPlanner::Plan(const std::vector<GCodeModalMachine::MotionSegment>& segs,
                            const PlannerConfig& cfg,
                            PlannedPath& out,
                            std::string& err)
{
    out = PlannedPath{};

    // ---- 预处理：丢掉零长段，算每段限速（F 单位 mm/min -> mm/s）----
    std::vector<GCodeModalMachine::MotionSegment> geo;
    std::vector<double> vmax, L;
    for (const auto& s : segs)
    {
        double len = SegLength(s);
        if (len < 1e-6) continue;                       // 零长段不参与规划
        double f = (s.feed > 0.0 ? s.feed : cfg.defaultFeed) / 60.0;
        geo.push_back(s);
        L.push_back(len);
        vmax.push_back(std::max(1e-3, f));
    }

    if (geo.empty())
    {
        err = "没有可规划的运动段（可能全是零长段或空程序）";
        return false;
    }
    size_t n = geo.size();

    // ---- 三步前瞻 ----
    std::vector<double> vJ = JunctionLimits(geo, vmax, cfg);
    std::vector<double> vExit;
    BackwardPass(L, vJ, cfg.aMax, vExit);

    // ---- 正向逐段生成轮廓 ----
    out.segs.resize(n);
    double v0 = 0.0;                                     // 起点静止
    double tCursor = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        PlannedSegment& ps = out.segs[i];
        ps.geom = geo[i];
        ps.length = L[i];

        double v1 = vExit[i];
        // 出口速度物理可达性钳位：本段长 L，速度变化量不超过 2aL
        double hiReach = std::sqrt(v0 * v0 + 2.0 * cfg.aMax * L[i]);
        double loBrake = std::sqrt(std::max(0.0, v0 * v0 - 2.0 * cfg.aMax * L[i]));
        v1 = std::min(v1, hiReach);
        if (v1 < loBrake) v1 = loBrake;                  // 刹不到那么低也要在段内匀减速兜底

        if (!BuildProfile(L[i], v0, v1, vmax[i], cfg.aMax, cfg.jMax, cfg.useSCurve, ps))
        {
            err = "第 " + std::to_string(ps.geom.lineNo) +
                  " 行对应的运动段长度不足，无法完成减速衔接";
            return false;
        }

        ps.tStart = tCursor;
        tCursor += ps.totalT;
        v0 = ps.v1;                                      // 下一段入口 = 本段出口
    }
    out.totalTime = tCursor;

    /**********************************************
     * 预览采样：整条轨迹按 sampleDt 均匀采点，
     * UI 拿到后一次画完整路径（计算都在后台线程完成）
     **********************************************/
    out.preview.reserve((size_t)(out.totalTime / cfg.sampleDt) + 2);
    for (double t = 0.0; t <= out.totalTime; t += cfg.sampleDt)
    {
        double pos[3], spd;
        Sample(out, t, pos, spd);
        out.preview.push_back({ pos[0], pos[1], pos[2] });
    }
    double endPos[3], endSpd;
    Sample(out, out.totalTime, endPos, endSpd);
    out.preview.push_back({ endPos[0], endPos[1], endPos[2] });

    out.minX = out.maxX = out.preview.front()[0];
    out.minY = out.maxY = out.preview.front()[1];
    for (const auto& p : out.preview)
    {
        out.minX = std::min(out.minX, p[0]); out.maxX = std::max(out.maxX, p[0]);
        out.minY = std::min(out.minY, p[1]); out.maxY = std::max(out.maxY, p[1]);
    }
    return true;
}

/**********************************************
* 实时仿真采样：t 时刻的位置与速度（纯函数）
* 相位积分：jerk 相位 s = v0*t + j*t^3/6，
*           匀加速相位 s = s0 + v_a*t + a*t^2/2
**********************************************/
void LookAheadPlanner::Sample(const PlannedPath& path, double t,
                              double pos[3], double& speed)
{
    if (path.segs.empty())
    {
        pos[0] = pos[1] = pos[2] = 0.0; speed = 0.0;
        return;
    }
    if (t >= path.totalTime) t = path.totalTime;

    // 找到 t 所在段（段数不多，线性扫足够）
    size_t idx = 0;
    while (idx + 1 < path.segs.size() && path.segs[idx + 1].tStart <= t) ++idx;
    const PlannedSegment& s = path.segs[idx];

    double tau = t - s.tStart;
    double tAcc = 2 * s.tj1 + s.tcA;                 // 加速侧总时长
    double j = (s.tj1 > kEps) ? s.a / s.tj1 : 0.0;   // 由 a 与 tj 反推 jerk

    double sArc = 0.0, v = 0.0;                      // 沿弦弧长 & 速度

    if (tau <= tAcc)                                 // ---- 加速侧 ----
    {
        double v0 = s.v0;
        double t1 = std::min(tau, s.tj1);            // 相位1：加加速
        double s1 = v0 * t1 + j * t1 * t1 * t1 / 6.0;
        double v1e = v0 + j * t1 * t1 / 2.0;
        sArc = s1; v = v1e;
        if (tau > s.tj1)                             // 相位2：匀加速
        {
            double t2 = std::min(tau, s.tj1 + s.tcA) - s.tj1;
            sArc += v1e * t2 + 0.5 * s.a * t2 * t2;
            v = v1e + s.a * t2;
            if (tau > s.tj1 + s.tcA)                 // 相位3：减加速
            {
                double vA2 = v1e + s.a * s.tcA;
                double s2 = v1e * s.tcA + 0.5 * s.a * s.tcA * s.tcA;
                double t3 = tau - s.tj1 - s.tcA;
                sArc += s2 + vA2 * t3 + 0.5 * s.a * t3 * t3 - j * t3 * t3 * t3 / 6.0;
                v = vA2 + s.a * t3 - 0.5 * j * t3 * t3;
            }
        }
    }
    else if (tau <= tAcc + s.tCruise)                // ---- 匀速段 ----
    {
        // 加速侧总位移 = (v0+vc)/2 * tAcc
        sArc = (s.v0 + s.vc) * 0.5 * tAcc + s.vc * (tau - tAcc);
        v = s.vc;
    }
    else                                             // ---- 减速侧（加速侧的时间反演）----
    {
        // 相位顺序与加速侧镜像：jerk -j [tj2] -> 匀减速 a [tcD] -> jerk +j [tj2]
        double u = tau - tAcc - s.tCruise;           // 进入减速侧的时间
        double sAcc = (s.v0 + s.vc) * 0.5 * tAcc + s.vc * s.tCruise;
        double j2 = (s.tj2 > kEps) ? s.a / s.tj2 : 0.0;

        double t1 = std::min(u, s.tj2);              // 相位1：jerk -j（下落变缓）
        double s1 = s.vc * t1 - j2 * t1 * t1 * t1 / 6.0;
        double v1e = s.vc - j2 * t1 * t1 / 2.0;
        sArc = sAcc + s1; v = v1e;
        if (u > s.tj2)
        {
            double t2 = std::min(u, s.tj2 + s.tcD) - s.tj2;   // 相位2：匀减速
            sArc += v1e * t2 - 0.5 * s.a * t2 * t2;
            v = v1e - s.a * t2;
            if (u > s.tj2 + s.tcD)                   // 相位3：jerk +j（刹停变缓）
            {
                double vD2 = v1e - s.a * s.tcD;
                double s2 = v1e * s.tcD - 0.5 * s.a * s.tcD * s.tcD;
                double t3 = u - s.tj2 - s.tcD;
                sArc += s2 + vD2 * t3 - 0.5 * s.a * t3 * t3 + j2 * t3 * t3 * t3 / 6.0;
                v = vD2 - s.a * t3 + 0.5 * j2 * t3 * t3;
            }
        }
    }

    // 弦长参数化：位置 = 起点 + 方向 * 沿弦弧长
    double L = (s.length > kEps) ? s.length : 1.0;
    double ratio = std::min(1.0, std::max(0.0, sArc / L));
    pos[0] = s.geom.x0 + (s.geom.x1 - s.geom.x0) * ratio;
    pos[1] = s.geom.y0 + (s.geom.y1 - s.geom.y0) * ratio;
    pos[2] = s.geom.z0 + (s.geom.z1 - s.geom.z0) * ratio;
    speed = std::max(0.0, v);
}
