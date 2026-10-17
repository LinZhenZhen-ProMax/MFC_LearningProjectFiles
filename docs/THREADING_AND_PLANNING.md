# 线程模型与数据流向说明（feature/async-planning 分支）

本文档说明本分支新增的两块内容：**多线程通信模型** 与 **连续轨迹前瞻规划 / 实时仿真预览**。
所有新增代码都在 `Learn1_V1_202609121037/Learn1_V1_202609121037/` 下，未改动 master 分支。

---

## 一、总体线程模型

程序共有 **3 个线程**：

```
┌─────────────────────────── UI 主线程（MFC）───────────────────────────┐
│  对话框消息循环 / OnPaint 轨迹预览 / TIMER_RX 收数据 / TIMER_PLAY 回放  │
└──────┬────────────────▲────────────────▲─────────────────┬───────────┘
       │ Submit(job)    │ PostMessage    │ PostMessage     │ SendLine()
       ▼                │ (WM_APP_PLAN_  │ (WM_APP_SERIAL_ │
┌──────────────┐       │  DONE)         │  LINE)          ▼
│PlannerWorker │───────┘                │          ┌─────────────┐
│  规划线程     │ 发布 PlannedPath 快照   │          │SerialWorker │
│(前瞻+加减速)  │ (shared_ptr 原子换手)   │          │  串口IO线程  │
└──────▲───────┘                        └──────────┴──────▲──────┘
       │                                                 │
  RingBuffer<m_jobs>                                m_txQueue (mutex)
  (覆盖式任务队列)                                  RingBuffer<std::string> 收行
```

### 线程间只传三种东西

| 通道 | 载体 | 方向 | 用途 |
|------|------|------|------|
| 环形缓冲区 | `RingBuffer<T>`（mutex + condition_variable） | 后台 → UI | 串口收到的完整行 |
| 消息投递 | `::PostMessage`（WM_APP+1 / WM_APP+2） | 后台 → UI | "有新数据" / "规划完成" 通知 |
| 不可变快照 | `std::shared_ptr<const PlannedPath>` | 规划线程 → UI | 规划结果（预览折线+相位参数） |

**原则：UI 线程永不直接碰串口句柄、永不执行规划计算；后台线程永不触碰窗口控件。**

---

## 二、新增文件与职责

| 文件 | 类型 | 职责 |
|------|------|------|
| `RingBuffer.h` | header-only | 线程安全环形缓冲区：满时丢最旧并计数；`pop` 支持超时等待；`close()` 唤醒所有等待者用于优雅停机 |
| `WorkerThread.h` | header-only | 工作线程基类：`Start/Stop/join`、`std::atomic` 停止标志、线程体异常在 `Trampoline` 收口（防止 `std::terminate`） |
| `SerialWorker.h/.cpp` | 线程 | 包住原 `SerialPort`：单 IO 循环"读 50ms 超时 → 拼行入环形缓冲 → PostMessage → 顺带清发送队列"；UI 用 `SendLine/TryGetLine` |
| `LookAheadPlanner.h/.cpp` | 纯算法 | 前瞻规划：交接限速 → 后向传递 → 梯形/S 型轮廓；`Sample()` 纯函数采样任意时刻位置/速度 |
| `PlannerWorker.h/.cpp` | 线程 | 规划任务队列（覆盖式，只保留最新任务）→ 后台 `Plan()` → 发布快照 + PostMessage；异常全部捕获转错误上报 |

### 线程启停与异常安全要点

1. **启停**：`WorkerThread::Stop()` 先置停止标志再 `join()`。串口 IO 循环靠 `SerialPort` 自带 50ms 读超时保证最多 50ms 退出；规划线程阻塞在条件变量上，`Shutdown()` 置唤醒标志并 `notify_all()`。
2. **异常安全**：
   - `std::thread` 函数向外抛异常会直接 `terminate`，`WorkerThread::Trampoline` 用 `try/catch(...)` 收口；
   - `PlannerWorker::Run` 内部把 `Plan()` 的任何异常转成 `LastError` 字符串 + 失败通知，线程永不崩；
   - `WorkerThread` 析构函数自动 `Stop()`，杜绝忘 join。
3. **数据同步**：
   - 收发行缓冲：满时丢最旧（遥测语义，宁可丢旧保新），丢弃计数可通过消息 WPARAM 观测；
   - 规划快照：`shared_ptr<const PlannedPath>` 在互斥锁保护下整体换手，UI 拿到的是不可变对象，读它全程无锁；
   - 发送队列：`std::mutex` 保护，UI push / IO 线程 swap 后批量写。

---

## 三、数据流向（端到端）

```
G代码文件 (.nc)
   │  CFileDialog（UI 线程）
   ▼
GCodeModalMachine::LoadProgram(path, errors, modal, outSegs)   ← 新重载，收集全部运动段
   │  LexLine（词法）→ ParseLine（模态更新 + X/Y/Z 坐标换算 G90/G91 → 产出 MotionSegment）
   ▼
std::vector<MotionSegment>
   │  PlannerWorker::Submit(job)     ← UI 线程，仅入队，立即返回
   ▼
PlannerWorker::Run（后台线程）
   │  LookAheadPlanner::Plan()：
   │    ① 交接点限速（GRBL 拐角允差模型，v=√(a·r)）
   │    ② 后向传递（保证每段出口速度刹得住后续路段）
   │    ③ 逐段梯形/S 型轮廓（峰值速度二分搜索贴合段长）
   ▼
PlannedPath 快照 { 相位参数 + 预览采样折线 + 包围盒 }
   │  shared_ptr 换手 + PostMessage(WM_APP_PLAN_DONE)
   ▼
UI：OnPlanDone → 保存快照 → "开始仿真"
   │  TIMER_PLAY(30ms)：LookAheadPlanner::Sample(快照, 墙钟t) → 当前点
   ▼
OnPaint/DrawPreview：画完整路径折线 + 起终点标记 + 回放当前点/实时速度
```

串口链路（并行存在，供后续下发指令）：

```
UI SendLine() ─► m_txQueue ─► SerialWorker::Run 顺带写出 ─► 串口
串口 ─► readserial(50ms) ─► 拼行 ─► RingBuffer ─► TIMER_RX / WM_APP_SERIAL_LINE ─► 接收框
```

---

## 四、前瞻算法细节

### 1. 交接点限速（Junction Limits）

相邻两段方向单位向量夹角 α（0 = 直线）：

- 近乎掉头（cosα < -0.99）→ 交接速度 = 0（必须刹停）；
- 否则按 GRBL 拐角允差模型：

  `v_junction = min( √(a_max · δ · sin(α/2) / (1 - sin(α/2))), v_max[i], v_max[i+1] )`

  δ = `junctionDeviation`（默认 0.05mm）：允许轨迹偏离理想拐角的最大距离。α 越大允许速度越低。

### 2. 后向传递（Backward Pass）

从终点（速度 0）往回扫：

`v_exit[i] = min( v_junction[i], √(v_exit[i+1]² + 2·a_max·L[i+1]) )`

物理含义：以 v_exit[i] 进入第 i+1 段后，必须能在剩余长度里用 a_max 刹到 v_exit[i+1]。

### 3. 前向轮廓生成（逐段）

- 出口速度物理钳位：`v1 ∈ [√(v0²-2aL), √(v0²+2aL)]`；
- 在 `[max(v0,v1), v_limit]` 上二分搜索巡航峰值 vc，使
  `加/减速距离之和 + vc·t_cruise == L`；
- 变速部分解析公式（S 型三相位，梯形 = jerk→∞ 退化）：
  - Δv ≥ a²/j（能摸到峰值加速度）：`tj = a/j`，匀加速 `tc = Δv/a - tj`；
  - 否则短变速：`tj = √(Δv/j)`，匀加速为 0；
  - 对称 jerk 相位下变速距离 = `(v_from + v_to)/2 · (2tj + tc)`；
- S 型曲线变速距离更长可能塞不下：出口速度 v1 向 v0 收敛重试（v1=v0 时该段恒可行），放宽结果沿正向传递保持衔接一致。

### 4. 实时仿真采样

`LookAheadPlanner::Sample(path, t, pos, speed)` 是**纯函数**：按相位积分（jerk 相位 `s = v0·t + j·t³/6`，匀加速 `s += v_a·t + a·t²/2`）得到沿弦弧长，再线性映射到段几何上。UI 的 30ms 定时器直接调用它取"当前位置 + 当前速度"来驱动预览——重计算已在后台完成，回放本身零成本。

**数值验证**（Python 等价实现，脚本见开发过程）：单段相位距离闭合 s(T)=L、v(T)=v1（误差 < 1e-4）；四段正方形轨迹端到端：90° 拐角限速 4.91mm/s 与公式手算一致、峰值速度不超 25mm/s 限速、末段刹停。

---

## 五、配置参数（PlannerConfig）

| 参数 | 默认值 | 含义 |
|------|--------|------|
| `aMax` | 200 mm/s² | 最大加速度 |
| `jMax` | 5000 mm/s³ | 最大加加速度（S 型用；`useSCurve=false` 时退化为梯形） |
| `junctionDeviation` | 0.05 mm | 拐角允差，越小拐角越保守 |
| `defaultFeed` | 1500 mm/min | 程序未给 F 时的默认进给 |
| `sampleDt` | 0.005 s | 预览折线采样步长 |
| `useSCurve` | true | true=S 型加减速，false=梯形 |

## 六、已知限制

1. G02/G03 圆弧按**弦线**近似（解释器尚未填入 I/J/K/R 圆弧几何）；
2. 未做刀具半径补偿 / 工件坐标系偏移（`ApplyOffset` 待实现）；
3. `test.cpp` 为草稿代码，本分支起已从构建中排除（`ExcludedFromBuild`）；
4. 顺带修复 master 上两个问题：`ParseLine` 中 `i <= words.size()` 的越界读取；`LoadProgram` 末尾缺 `return` 的未定义行为。
