# MFC_LearningProjectFiles — MFC 学习项目（G 代码解释器 + 串口通信上位机）

一个基于 **MFC 对话框程序** 的 CNC（数控）学习项目：目标是做一个能**解析 G 代码加工程序**、并通过**串口**把运动指令下发给下位机（如 STM32 等）的 Windows 上位机。代码中保留了大量的中文学习注释，记录了每一步的实现思路，适合作为 MFC / Win32 串口 / G 代码模态解释器的入门参考。

> 项目名 `Learn1_V1_202609121037` 表示这是第一个学习工程（V1，创建于 2026-09-12 10:37）。

>  **分支说明**：`feature/async-planning` 分支在 master 基础上新增了 **多线程串口/规划模型** 与 **前瞻速度规划 + 轨迹实时仿真预览**，详见 [docs/THREADING_AND_PLANNING.md](docs/THREADING_AND_PLANNING.md)。master 分支保持原样。

## 项目简介

整个解决方案只有一个 MFC 对话框工程，按功能拆成三个相对独立的模块：

| 模块 | 文件 | 状态 | 职责 |
|------|------|------|------|
| G 代码模态解释器 | `GCodeModalMachine.h/.cpp` | **核心已完成**（词法 + 语法解析；坐标/圆弧段产出待完善） | 读入加工程序 → 逐行词法分析 → 解析 G/M 字并更新模态状态机 |
| 运动规划器 | `MotionPlanner.h/.cpp` | 骨架阶段 | 根据当前模态（G00/G01/G02/G03）生成运动段 |
| 串口通信 | `SerialPort.h/.cpp` | **已完成** | Win32 API 封装的串口打开/读/写/关闭 |
| 对话框 UI | `Learn1_V1_202609121037Dlg.h/.cpp` | 界面就绪，事件处理为 TODO | 串口选择下拉框、打开串口按钮、发送/接收编辑框 |

### 分支 `feature/async-planning` 新增模块

| 模块 | 文件 | 职责 |
|------|------|------|
| 环形缓冲区 | `RingBuffer.h` | 线程安全收发队列（满时丢最旧、超时等待、优雅关闭） |
| 工作线程基类 | `WorkerThread.h` | 统一启停/join/异常收口 |
| 串口后台线程 | `SerialWorker.h/.cpp` | 串口收发全部迁出 UI 线程，收行入环形缓冲 + PostMessage 通知 |
| 前瞻规划器 | `LookAheadPlanner.h/.cpp` | 交接限速 → 后向传递 → 梯形/S 型加减速轮廓；`Sample()` 实时采样 |
| 规划后台线程 | `PlannerWorker.h/.cpp` | 覆盖式任务队列，后台规划，`shared_ptr` 快照发布结果 |
| 轨迹仿真预览 | `Learn1_V1_202609121037Dlg.cpp`（`DrawPreview`） | 右侧面板画完整轨迹 + 回放当前点/实时速度；"加载G代码/开始仿真/停止仿真"按钮 |

## 主要功能

### 1. G 代码模态解释器（GCodeModalMachine）

- **LoadProgram(path, ...)**：打开加工程序文件，逐行（`std::getline`）分发给词法分析；打不开文件时按 `cannot_open_file` 报错并返回。
- **LexLine(lineNo, line, words, err)**：把一行代码切成 "字（Word）" 数组
  - 去空白字符、丢弃 `(...)` 行内注释、遇到 `;` 视为行注释结束本行、小写转大写；
  - 用 `std::stod` 解析字母后的数值，并记录是否带小数点（`hasDot`）；
  - 拒绝科学计数法（`e/E`），对非法字符、非数字、数值越界分别报错。
- **ParseLine(...)**：按字母分派，更新 9 个互斥模态"槽位"：

  | 槽位 | 模态组 | 默认值 |
  |------|--------|--------|
  | 运动模式 | G00/G01/G02/G03 | G00 快速定位 |
  | 加工平面 | G17/G18/G19 | XY 平面 |
  | 尺寸单位 | G20/G21 | 毫米 |
  | 刀具半径补偿 | G40/G41/G42 | 取消 |
  | 刀具长度补偿 | G43/G44/G49 | 正补偿 |
  | 工件坐标系 | G53~G59 | G54 |
  | 坐标计算方式 | G90/G91 | 绝对值编程 |
  | 主轴转向 | M03/M04/M05 | 停止 |
  | 冷却液 | M08/M09 | 关 |

  另维护非模态量：进给速度 F、主轴转速 S、刀具号 T；G/M 后带小数点视为非法值报错；无法识别的字母报 `Unsupported_Letter`。X/Y/Z/I/J/K/R 等坐标字已留好校验位（待实现）。
- **错误系统**：`CodeError` 结构统一记录 *文件路径 / 行号 / 原始行文本 / 错误类型*，错误类型含 `not_a_number`、`out_of_range`、`illegal_char`、`cannot_open_file`、`Unsupported_Letter`、`illegal_value` 等，方便 UI 层报"第 N 行出错"。

### 2. 运动规划器（MotionPlanner）

- `ProcessLine(words, modal, errors, outSegments)` 接收一行解析后的字和当前模态状态，按运动模式（Rapid / Line / ArcCW / ArcCCW）分派，产出 `MotionSegment`（含起点、终点、进给速度、原始行号）。
- 目前四个分支为空实现（骨架），是下一步的开发重点。

### 3. 串口通信（SerialPort）

- `openserial(PortName, Baud)`：以 `\\.\COMx` 方式用 `CreateFileA` 打开串口；`GetCommState`/`SetCommState` 配置为 **8 数据位、无校验、1 停止位**；`COMMTIMEOUTS` 设置读间隔 50ms、读总超时 50ms、写总超时 5000ms。
- `writeserial` / `readserial`：返回 `Writeserial_result` / `Readserial_result` 结构（字节数 + 错误码 + `ok()`），不抛异常，方便上层判断。
- `close()` 在析构函数中自动调用，防止句柄泄漏。

### 4. 对话框界面

- 主对话框基于 `CDialogEx`，包含：
  - `IDC_COMBO_PORT` 串口选择下拉框（`OnCbnSelchangeComboPort`，处理逻辑待填）；
  - `IDC_BUTTON_OPENSERIAL` 打开串口按钮（`OnBnClickedButtonOpenserial`，处理逻辑待填）；
  - 发送 / 接收两个编辑框，通过 **DDX** 绑定到 `m_strSend` / `m_strRecv`；
  - 系统"关于"菜单（含命令号低 4 位必须为 0、需小于 0xF000 的说明注释）。
- App 层用 `TRACE` 在构造/初始化各阶段打调试日志，方便理解 MFC 程序的启动时序。

### 前瞻规划 + 仿真预览（分支新增，UI 内操作流程）

1. 启动程序（窗口自动加宽，右侧为轨迹预览面板）；
2. 点击 **"加载G代码"**：选择 `.nc/.txt` 文件 → 解析出运动段 → 自动提交后台规划线程；
3. 接收框显示"规划完成 N 段, 总时长 X s"，预览面板出现完整轨迹折线（蓝线、绿点起点、红点终点）；
4. 点击 **"开始仿真"**：黑色圆点沿轨迹按 S 型/梯形速度曲线回放，底部实时显示 `X/Y/当前速度`；"停止仿真"随时中断。

程序内调用：

```cpp
LookAheadPlanner planner;
PlannerConfig cfg;                       // aMax/jMax/拐角允差/梯形或S型
PlannedPath path;
std::string err;
if (planner.Plan(segs, cfg, path, err))  // segs 来自 LoadProgram 新重载
{
    double pos[3], speed;
    LookAheadPlanner::Sample(path, 1.25, pos, speed);  // 取 t=1.25s 时的位置/速度
}
```

## 目录结构

```
MFC_LearningProjectFiles/
├── README.md                      # 本文件
├── docs/
│   └── THREADING_AND_PLANNING.md  # 分支新增：线程模型与数据流向文档
├── .gitattributes                 # 行尾自动规范化配置
├── .gitignore                     # Visual Studio 标准忽略规则
└── Learn1_V1_202609121037/        # 解决方案根目录
    ├── Learn1_V1_202609121037.sln                 # VS 解决方案
    └── Learn1_V1_202609121037/                    # 工程目录
        ├── Learn1_V1_202609121037.vcxproj         # 工程文件
        ├── Learn1_V1_202609121037.vcxproj.filters # 工程文件分组
        ├── Learn1_V1_202609121037.h/.cpp          # 应用类 CWinApp（程序入口）
        ├── Learn1_V1_202609121037Dlg.h/.cpp       # 主对话框（串口 UI + 轨迹预览）
        ├── GCodeModalMachine.h/.cpp               # G 代码模态解释器 ★核心
        ├── MotionPlanner.h/.cpp                   # 运动规划器（骨架）
        ├── SerialPort.h/.cpp                      # 串口封装 ★核心
        ├── RingBuffer.h                           # [分支] 线程安全环形缓冲区
        ├── WorkerThread.h                         # [分支] 工作线程基类
        ├── SerialWorker.h/.cpp                    # [分支] 串口后台线程
        ├── LookAheadPlanner.h/.cpp                # [分支] 前瞻规划 + 加减速轮廓
        ├── PlannerWorker.h/.cpp                   # [分支] 规划后台线程
        ├── test.cpp                               # 草稿代码（已排除出编译）
        ├── pch.h/.cpp                             # 预编译头
        ├── framework.h / targetver.h              # MFC 框架头
        ├── Resource.h                             # 资源 ID
        ├── Learn1_V1_202609121037.rc              # 资源文件（对话框布局、菜单、字符串）
        └── res/                                   # 图标等资源
```

## 环境要求（依赖）

- **操作系统**：Windows 10/11（串口和 MFC 均为 Windows 专有技术，不能跨平台编译）
- **IDE**：Visual Studio 2022（解决方案版本 17，`PlatformToolset v143`）
- **VS 安装组件**（在 Visual Studio Installer 中勾选）：
  - "使用 C++ 的桌面开发" 工作负载；
  - **适用于最新 v143 生成工具的 C++ MFC（x86 和 x64）** —— 动态链接 MFC（`UseOfMfc=Dynamic`），必须安装；
  - Windows 10/11 SDK（工程设置为 `WindowsTargetPlatformVersion=10.0`，任意最新 SDK 即可）。
- **运行时依赖**：MFC 以动态方式链接，发布给他人使用时目标机器需要对应的 MFC 运行库（通常随系统 VC++ Redistributable 提供）。
- **无第三方库**：所有代码只用标准库（STL）与 Win32/MFC API，无需额外配置 NuGet 或包管理。
- **可选硬件**：一块真实的串口设备（或虚拟串口对，如 com0com）用于联调串口功能。

## 安装与运行

1. 克隆仓库：

   ```bash
   git clone https://github.com/LinZhenZhen-ProMax/MFC_LearningProjectFiles.git
   ```

2. 双击打开解决方案：

   ```
   Learn1_V1_202609121037/Learn1_V1_202609121037.sln
   ```

3. 在 Visual Studio 顶部选择配置（Debug / Release，x64 或 x86），按 **F5**（调试运行）或 **Ctrl+F5** 运行。
   - 首次编译会生成 `pch.h` 预编译头，耗时略长属正常现象。
   - 若提示缺少 MFC 库，回到 VS Installer 补装上文的 MFC 组件即可。

4. 程序启动后是一个对话框窗口；当前版本界面已就绪，串口下拉框与打开按钮的事件逻辑仍在开发中，完整联调流程（选串口 → 打开 → 发送 G 代码 → 收下位机回包）待后续版本实现。

## 使用示例

### G 代码解释器（程序内调用）

```cpp
#include "GCodeModalMachine.h"

GCodeModalMachine gc;
std::vector<GCodeModalMachine::CodeError> words, errors;
GCodeModalMachine::ModalState   modal;    // 模态状态机（跨行保持）
GCodeModalMachine::MotionSegment seg;     // 最近一次解析出的运动段

// 读入并解析整个加工程序；出错信息按行号累积在 errors 中
bool ok = gc.LoadProgram("D:/jog_program.nc", words, errors, modal, seg);

for (const auto& e : errors)
{
    printf("[%s] 第 %d 行出错(%d): %s\n",
           e.path.c_str(), e.lineNo, (int)e.kind, e.source.c_str());
}
```

一段可被正确解析的加工程序示例（支持大写/小写、空格、括号注释、分号行注释）：

```gcode
; 程序开始
N10 G90 G54 G17          (绝对值编程, G54 坐标系, XY 平面)
N20 M03 S1200            (主轴正转 1200rpm)
N30 G01 F300 X50.0 Y20   (以 300mm/min 直线插补到 (50,20))
N40 M05 M09              (主轴停, 冷却关)
```

### 串口类（程序内调用）

```cpp
#include "SerialPort.h"

SerialPort com;

if (com.openserial("\\\\.\\COM3", 115200))   // 也可只写 "COM3"，类内部会补前缀
{
    const char* cmd = "G01 X50\n";
    auto w = com.writeserial(cmd, (int)strlen(cmd));
    if (w.ok()) { /* 已发送 w.Bytesent 字节 */ }

    char buf[128];
    auto r = com.readserial(buf, sizeof(buf));
    if (r.ok() && r.Byterecv > 0) { /* 收到 r.Byterecv 字节 */ }

    com.close();
}
else
{
    // 打开失败：串口不存在或被占用
}
```

## 配置说明

| 配置项 | 位置 | 当前值 | 说明 |
|--------|------|--------|------|
| 平台工具集 | `.vcxproj` | `v143` | 对应 VS2022；改用其他版本需同步修改 |
| MFC 使用方式 | `.vcxproj` | `在共享 DLL 中使用 MFC`（Dynamic） | 需安装 VS 的 MFC 组件；改静态链接可脱离 MFC 运行库 |
| 字符集 | `.vcxproj` | Unicode | 串口类内部用 `CreateFileA`（ANSI），UI 层为宽字符，注意转换 |
| Windows SDK | `.vcxproj` | `10.0`（最新已装版本） | 无需指定具体版本号 |
| 串口参数 | `SerialPort.cpp` | 8-N-1，读超时 50ms，写超时 5000ms | 波特率由 `openserial` 参数传入；校验位/停止位改 `dcb` 字段 |
| 串口名格式 | `SerialPort.cpp` | `\\.\COMx` | `COM1~COM9` 可省前缀，`COM10` 及以上必须带前缀 |
| G 代码注释 | `GCodeModalMachine.cpp` | `(...)` 行内注释；`;` 行注释 | 与常见 CNC 控制器习惯一致 |

## 开发进度与说明

-  已完成：G 代码词法分析、G/M 模态解析、统一错误记录；串口完整读写封装；对话框 UI 与 DDX 绑定。
-  进行中：坐标字（X/Y/Z/I/J/K/R）数值校验与运动段计算、G91 增量坐标换算（`ApplyOffset` 已声明待实现）、`MotionPlanner` 各运动模式的段生成、串口 UI 事件处理。
-  `test.cpp` 为学习过程的草稿代码，未纳入正常构建流程，仅作思路参考。
- 源码注释为中文（部分文件为 GBK/ANSI 编码，在 UTF-8 环境下查看可能出现乱码，用 VS 或切换 GBK 编码打开即可）。

## 许可证

仅供学习交流使用。
