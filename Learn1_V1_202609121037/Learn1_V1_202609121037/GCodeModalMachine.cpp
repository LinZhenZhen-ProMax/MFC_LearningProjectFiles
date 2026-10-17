#include "pch.h"
#include "GCodeModalMachine.h"

bool GCodeModalMachine::RecordError(const int                        lineNo,
                                    const std::string&                 line,
                                    ErrorKind                     errorkind,
                                    const std::string&                 path,
                                    std::vector<CodeError>&             err)
{
    CodeError e;
    e.lineNo = lineNo;
    e.source = line;
    e.kind = errorkind;
    e.path = path;
    err.push_back(e);
    return false;
}


/****************************************************
* LoadProgram读加工程序函数
* ifstream函数打开路径下的文件，打开失败则返回报错
* 当我按下执行程序按钮，就调用LoadProgram函数开始读程序，
* std::getline(jogprogram, line)每次只读一行，按行转交给LexLine切成一个个的word
* 应该要将读出来的Word拼在一起，要拼起来吗？
* 还要转给状态机搞状态，应该每读一行有状态都要先搞状态，一行读不出来要把错误往上传,错误要不要也拼进去
* 上位机交给下位机的，应该是运动段，就是带有上一坐标和下一坐标的，运动段
****************************************************/
bool GCodeModalMachine::LoadProgram(const std::string& path, 
                                    std::vector<CodeError>& outWords,
                                    std::vector<CodeError>& outErrors, 
                                    ModalState& modal,
                                    MotionSegment& outseg)
{
    std::ifstream jogprogram(path);
    if (!jogprogram.is_open())
    {
        /*CodeError e;
        e.lineNo = -1;
        e.source = -1;
        e.kind = ErrorKind::cannot_open_file;
        e.path = path;
        outErrors.push_back(e);
        return false;*/

        return RecordError(0, std::string(), ErrorKind::cannot_open_file, path, outErrors);
    }

    std::string line;
    int lineNo = 0;

    /************************************************
    * while内是每读一行执行一次，所以是每读一行自增一次行号，重新声明一次vector对象，
    ************************************************/
    while (std::getline(jogprogram, line))//
    {
        lineNo++;
        std::vector<Word>      lineWords;
        std::vector<CodeError> lineErrors;
        if (!GCodeModalMachine::LexLine(lineNo, line, lineWords, lineErrors))//报错
        {
            outErrors.insert(outErrors.end(), lineErrors.begin(), lineErrors.end());//把错误收集起来
        }
        else//读正确了，交给ParseLine更新状态机
        {
            GCodeModalMachine::ParseLine(lineNo, line, lineWords, modal, outseg, lineErrors);

        }
    }
    return true;
}

/********************************************************
* 读单行加工程序并切成一个一个的词
* vector类负责创造一个能自动变长的数组，<Word>尖括号内表示数组类型，这个自动变长的数组本质上是一个类，他有成员函数
********************************************************/
/****************************************************
* LoadProgram【新增重载】：读入加工程序并收集所有运动段
* 逐行 LexLine -> ParseLine(produced)，产生的运动段按
* 程序顺序追加进 outSegs，供 LookAheadPlanner 规划。
* 返回 true 表示文件读入完成（解析错误都累积在 outErrors）。
****************************************************/
bool GCodeModalMachine::LoadProgram(const std::string& path,
                                    std::vector<CodeError>& outErrors,
                                    ModalState& modal,
                                    std::vector<MotionSegment>& outSegs)
{
    outSegs.clear();
    std::ifstream jogprogram(path);
    if (!jogprogram.is_open())
    {
        return RecordError(0, std::string(), ErrorKind::cannot_open_file, path, outErrors);
    }

    std::string line;
    int lineNo = 0;

    while (std::getline(jogprogram, line))
    {
        lineNo++;
        std::vector<Word>      lineWords;
        std::vector<CodeError> lineErrors;

        if (!GCodeModalMachine::LexLine(lineNo, line, lineWords, lineErrors))
        {
            outErrors.insert(outErrors.end(), lineErrors.begin(), lineErrors.end());
        }
        else
        {
            MotionSegment seg;
            bool produced = false;
            if (!GCodeModalMachine::ParseLine(lineNo, line, lineWords,
                                              modal, seg, produced, lineErrors))
            {
                outErrors.insert(outErrors.end(), lineErrors.begin(), lineErrors.end());
            }
            if (produced)
            {
                outSegs.push_back(seg);
            }
        }
    }
    return true;
}

bool GCodeModalMachine::LexLine(const int lineNo, 
                                const std::string& line, 
                                std::vector<Word>& words, 
                                std::vector<CodeError>& err)
{
    words.reserve(32);//在内存上申请容量
    words.clear();    //清空，但容量不变
    err.clear();

    size_t i = 0;
    
  
    /**************************************************
    * while逐字读加工代码
    **************************************************/
    while (i < line.size())
    {
        //修改类型为无符号整数
        //static_cast<unsigned char>(line[i]);

        //读到空格，跳过空格
        if (std::isspace(static_cast<unsigned char>(line[i])))
        {
            i++;
            continue;
        }
        //读到左括号注释，寻找右括号，跳过括号
        if (static_cast<unsigned char>(line[i]) == '(')
        {
            while (i < line.size() && static_cast<unsigned char>(line[i]) != ')')i++;//停在右括号前
            if (i < line.size() && static_cast<unsigned char>(line[i]) == ')')i++;   //跨过右括号
            continue;
        }
        //读到注释符；退出循环，跳过整行代码
        if (static_cast<unsigned char>(line[i]) == ';')
        {
            break;
        }
        /************************************************
        * 读到一个字母，小写转化成大写，读完字母开始读数值
        * stod()，string to double，字符串转双精度浮点数，返回position表示将多少字符解析为value
        * 但是stod只能从头读一整段字符串，不能从某个位置开始
        * substr()，截取一段子字符串
        * 子串交给stod()读出数值存入结构体word，
        * substr利用stod返回的position截取子串，检查子串内是否有“.”、“e”、“E”，“.”记录hasDot，“e”、“E”报非法值
        * 用try，catch组合接住stod()抛出的两个类型的错误
        * 若不抛出错误则执行try内所有代码，将结构体word同步到vertor<Word>words，光标向后移动
        * 抛出错误则将错误信息存入err
        ************************************************/
        if (std::isalpha(static_cast<unsigned char>(line[i])))
        {
            Word word;
            size_t position = 0;

            word.letter = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(line[i])));
            i++;
            try
            {
                word.value = std::stod(line.substr(i), &position);

                word.hasDot = (std::string(line.substr(i, position)).find('.') != std::string::npos);
                
                if (std::string(line.substr(i, position)).find('e') != std::string::npos ||
                    std::string(line.substr(i, position)).find('E') != std::string::npos)
                {
                    return RecordError(lineNo, line, ErrorKind::illegal_value, std::string(), err);
                }
                
                words.push_back(word);
                i = i + position;
            }
            catch (const std::invalid_argument&)
            {
                return RecordError(lineNo, line, ErrorKind::not_a_number, std::string(), err);
            }
            catch (const std::out_of_range&)
            {
                return RecordError(lineNo, line, ErrorKind::out_of_range, std::string(), err);
            }

            continue;
        }

        //所有的if都没踩到，返回false
        return RecordError(lineNo, line, ErrorKind::illegal_char, std::string(), err);
    }

    //所有字符都顺利读完，或者是注释，返回true
    return true;
}

/*************************************************
* ParseLine函数解析一行, 更新模态和当前位置, 产出运动段
* 应该还要根据letter检查value是否合法
* 像G04那样的代码还得配合其他letter，还得捕捉相应的letter
* 产出运动段，用什么方式把word交给motionplanner，交给motionplanner的时候还要考虑更新状态的时机，
* 比如说圆弧代码一定要在状态机更新为圆弧插补运动模式之后再给motionplanner
* case 2:modal.motion = MotionType::ArcCW; break;像这样子就能更新状态了吗？
* 
* 每一行的行号，原文输入进来用于报错，
* words输入进来ParseLine函数解析，
* vector数组words每个元素都包含三个参数，.letter .value .hasDot
* while循环读一行的words
* switch，case配合定制每一个letter的解析方案，包括合法检测
* G、M两个代码是离散值，需要再嵌套switch
* 
*************************************************/
bool GCodeModalMachine::ParseLine(const int               lineNo,
                                  const std::string&        line,
                                  const std::vector<Word>& words,
                                  ModalState&              modal,
                                  MotionSegment&          outSeg,
                                  bool&                   produced,
                                  std::vector<CodeError>&    err)
{
    produced = false;

    // 本行的坐标目标：未指定的轴保持当前位置（G90 绝对 / G91 增量）
    double tx = modal.currentX, ty = modal.currentY, tz = modal.currentZ;
    bool hasAxis = false;

    size_t i = 0;
    while (i < words.size())
    {
        switch (words[i].letter)
        {
            case'G':
            {
                if (words[i].hasDot)
                {
                    return RecordError(lineNo, line, ErrorKind::illegal_value, std::string(), err);
                }
                int i_value = (int)std::round(words[i].value);
                switch (i_value)
                {
                    case 0:modal.motion = MotionType::Rapid; break;   // G00 快速定位
                    case 1:modal.motion = MotionType::Line; break;    // G01 直线插补
                    case 2:modal.motion = MotionType::ArcCW; break;   // G02 顺圆插补
                    case 3:modal.motion = MotionType::ArcCCW; break;  // G03 逆圆插补
                    case 4:break;   // G04 暂停
                    case 10:break;  // 取消镜像
                    case 11:break;  // X 轴镜像
                    case 12:break;  // Y 轴镜像
                    case 13:break;  // Z 轴镜像
                    case 17:modal.plane = JogPlane::XY; break;        // 选 XY 平面
                    case 18:modal.plane = JogPlane::ZX; break;        // 选 ZX 平面
                    case 19:modal.plane = JogPlane::YZ; break;        // 选 YZ 平面
                    case 40:modal.ccomp = CutterComp::Off; break;     // 取消半径补偿
                    case 41:modal.ccomp = CutterComp::Left; break;    // 左半径补偿
                    case 42:modal.ccomp = CutterComp::Right; break;   // 右半径补偿
                    case 43:modal.lcomp = LengthComp::Positive; break;// 正长度补偿
                    case 49:modal.lcomp = LengthComp::Cancel; break;  // 取消长度补偿
                    case 51:break;  // 使用伸缩
                    case 53:modal.offset = WorkOffset::G53; break;    // 机床坐标系
                    case 54:modal.offset = WorkOffset::G54; break;
                    case 55:modal.offset = WorkOffset::G55; break;
                    case 56:modal.offset = WorkOffset::G56; break;
                    case 57:modal.offset = WorkOffset::G57; break;
                    case 58:modal.offset = WorkOffset::G58; break;
                    case 59:modal.offset = WorkOffset::G59; break;
                    case 68:break;  // 坐标旋转
                    case 69:break;  // 坐标旋转取消
                    case 90:modal.coord = CoordinateMode::Absolute; break;   // 绝对编程
                    case 91:modal.coord = CoordinateMode::Incremental; break;// 增量编程
                    case 151:break;// 选择加工件数
                }
                break;//case'G'
            }

            case'M':
            {
                if (words[i].hasDot)
                {
                    return RecordError(lineNo, line, ErrorKind::illegal_value, std::string(), err);
                }
                int i_value = (int)std::round(words[i].value);
                switch (i_value)
                {
                    case 0:break;   // 程序暂停
                    case 1:break;   // 选择暂停
                    case 2:break;   // 程序结束
                    case 3:modal.spindleDir = SpindleDirection::CW; break;   // 主轴正转
                    case 4:modal.spindleDir = SpindleDirection::CCW; break;  // 主轴反转
                    case 5:modal.spindleDir = SpindleDirection::Stop; break; // 主轴停止
                    case 6:break;   // 换刀
                    case 7:break;   // 换刀箱
                    case 8:modal.coolant = Coolant::Flood; break;  // 冷却开
                    case 9:modal.coolant = Coolant::Off; break;    // 冷却关
                    case 13:break; // 刀具夹头开
                    case 14:break; // 刀具夹头关
                    case 19:break; // 主轴定位
                    case 30:break; // 程序结束
                    case 37:break; // 减速电机 +1
                    case 38:break; // 当前刀具使用次数 +1
                    case 54:break; // 当前位置设为用户数据 X 轴原点
                    case 55:break; // 当前位置设为用户数据 Y 轴原点
                    case 56:break; // 当前位置设为用户数据 Z 轴原点
                    case 57:break; // 当前位置设为用户数据全部原点
                    case 91:break; // 对刀仪
                    case 98:break; // 子程序调用
                    case 99:break; // 子程序返回
                    case 100:break;// 停止主轴
                    case 101:break;// 停止冷却
                }
                break;//case'M'
            }
            case'T':modal.tool = words[i].value; break;  // 刀具号（范围校验待补）
            case'D':break; // 刀具半径补偿号
            case'H':break; // 刀具长度补偿号

            case'F':modal.feed = words[i].value; break;       // 进给速度 mm/min
            case'S':modal.spindleRpm = words[i].value; break; // 主轴转速

            // 坐标字：按 G90 绝对 / G91 增量换算为目标点
            case'X':
                tx = (modal.coord == CoordinateMode::Incremental)
                     ? modal.currentX + words[i].value : words[i].value;
                hasAxis = true; break;
            case'Y':
                ty = (modal.coord == CoordinateMode::Incremental)
                     ? modal.currentY + words[i].value : words[i].value;
                hasAxis = true; break;
            case'Z':
                tz = (modal.coord == CoordinateMode::Incremental)
                     ? modal.currentZ + words[i].value : words[i].value;
                hasAxis = true; break;
            case'A':break; // 4 轴（校验待补）
            case'B':break; // 5 轴（校验待补）
            case'C':break; // 6 轴（校验待补）

            case'I':break; // X 向圆心偏移（圆弧几何待补）
            case'J':break; // Y 向圆心偏移（圆弧几何待补）
            case'K':break; // Z 向圆心偏移（圆弧几何待补）
            case'R':break; // 圆弧半径（圆弧几何待补）

            case'L':break; // 重复次数
            case'N':break; // 行号
            case'P':break; // 暂停参数
            case'Q':break; // 暂停参数
            case'V':break; // 加工件总数

            default:
            {
                // 没有 case 接住的字母：报"不支持的字符"
                return RecordError(lineNo, line, ErrorKind::Unsupported_Letter, std::string(), err);
            }
        }

        i++;
    }

    /****************************************************
    * 出现了坐标字 -> 产出运动段：
    *   - 起点为模态机维护的当前位置，终点为换算后的目标点；
    *   - G00/G01 直线段为精确几何；
    *   - G02/G03 圆弧暂按"起点->终点弦线"近似（圆弧参数
    *     I/J/K/R 尚未由解释器填入，为已知限制）；
    *   - 产出后推进 modal.currentX/Y/Z。
    ****************************************************/
    if (hasAxis)
    {
        outSeg.type   = modal.motion;
        outSeg.x0 = modal.currentX; outSeg.y0 = modal.currentY; outSeg.z0 = modal.currentZ;
        outSeg.x1 = tx;             outSeg.y1 = ty;             outSeg.z1 = tz;
        outSeg.feed   = modal.feed;
        outSeg.lineNo = lineNo;

        modal.currentX = tx;
        modal.currentY = ty;
        modal.currentZ = tz;
        produced = true;
    }
    return true;
}

/****************************************
* 旧签名（兼容保留）：转调带 produced 标志的新重载
****************************************/
bool GCodeModalMachine::ParseLine(const int               lineNo,
                                  const std::string&        line,
                                  const std::vector<Word>& words,
                                  ModalState&              modal,
                                  MotionSegment&          outSeg,
                                  std::vector<CodeError>&    err)
{
    bool produced = false;
    return ParseLine(lineNo, line, words, modal, outSeg, produced, err);
}
