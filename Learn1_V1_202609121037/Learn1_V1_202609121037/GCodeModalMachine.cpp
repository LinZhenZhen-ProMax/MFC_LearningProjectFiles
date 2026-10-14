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

}

/********************************************************
* 读单行加工程序并切成一个一个的词
* vector类负责创造一个能自动变长的数组，<Word>尖括号内表示数组类型，这个自动变长的数组本质上是一个类，他有成员函数
********************************************************/
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
bool GCodeModalMachine::ParseLine(const int lineNo,               //行号
                                  const std::string& line,        //行
                                  const std::vector<Word>& words, //
                                  ModalState& modal,              //状态输出
                                  MotionSegment& outSeg,          //运动段输出
                                  std::vector<CodeError>& err)    //报错输出
{
    size_t i = 0;
    while (i <= words.size())
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
                    case 0:modal.motion = MotionType::Rapid; break;  //快速进给
                    case 1:modal.motion = MotionType::Line; break;   //切削进给
                    case 2:modal.motion = MotionType::ArcCW; break;  //顺时针圆弧插补
                    case 3:modal.motion = MotionType::ArcCCW; break; //逆时针圆弧插补
                    case 4:break; //暂停
                    case 10:break; //取消镜像
                    case 11:break; //X轴镜像
                    case 12:break; //Y轴镜像
                    case 13:break; //Z轴镜像
                    case 17:modal.plane = JogPlane::XY; break; //选择XY平面
                    case 18:modal.plane = JogPlane::ZX; break; //选择ZX平面
                    case 19:modal.plane = JogPlane::YZ; break; //选择YZ平面
                    case 40:modal.ccomp = CutterComp::Off; break;     //取消刀具半径补偿
                    case 41:modal.ccomp = CutterComp::Left; break;    //左侧刀具半径补偿
                    case 42:modal.ccomp = CutterComp::Right; break;   //右侧刀具半径补偿
                    case 43:modal.lcomp = LengthComp::Positive; break;//刀具长度正向补偿
                    case 49:modal.lcomp = LengthComp::Cancel; break;  //取消刀具长度补偿
                    case 51:break; //使用缩放
                    case 53:modal.offset = WorkOffset::G53; break; //选择机床坐标系
                    case 54:modal.offset = WorkOffset::G54; break;
                    case 55:modal.offset = WorkOffset::G55; break;
                    case 56:modal.offset = WorkOffset::G56; break;
                    case 57:modal.offset = WorkOffset::G57; break;
                    case 58:modal.offset = WorkOffset::G58; break;
                    case 59:modal.offset = WorkOffset::G59; break;
                    case 68:break; //坐标旋转
                    case 69:break; //坐标旋转取消
                    case 90:modal.coord = CoordinateMode::Absolute; break; //绝对值编程
                    case 91:modal.coord = CoordinateMode::Incremental; break; //增量值编程
                    case 151:break; //选择加工参数
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
                    case 0:break; //暂停
                    case 1:break; //暂停
                    case 2:break; //结束加工
                    case 3:modal.spindleDir = SpindleDirection::CW; break; //主轴正转
                    case 4:modal.spindleDir = SpindleDirection::CCW; break; //主轴反转
                    case 5:modal.spindleDir = SpindleDirection::Stop; break; //主轴停止
                    case 6:break; //根据上一个 T 指令指定的刀号进行取刀
                    case 7:break; //根据当前主轴上的刀号放至对应的刀座上
                    case 8:modal.coolant = Coolant::Flood; break; //冷却开启
                    case 9:modal.coolant = Coolant::Off; break; //冷却关闭
                    case 13:break; //无线测头开启
                    case 14:break; //无线测头关闭
                    case 19:break; //主轴定位
                    case 30:break; //结束加工
                    case 37:break; //工件计数 + 1
                    case 38:break; //当前刀具使用次数 + 1
                    case 54:break; //当前位置设为用户坐标 X 轴原点
                    case 55:break; //当前位置设为用户坐标 Y 轴原点
                    case 56:break; //当前位置设为用户坐标 Z 轴原点
                    case 57:break; //当前位置设为用户坐标三轴原点
                    case 91:break; //对刀，对刀过程出错不暂停其他刀具对刀
                    case 98:break; //调用子程序
                    case 99:break; //结束子程序，返回主程序
                    case 100:break; //停止译码
                    case 101:break; //停止译码
                }
                break;//case'M'
            }
            case'T':modal.tool = words[i].value; break;//刀具号，根据letter检查value是否合法，刀具号不能超出刀具数量
            case'D':break; //刀具半径补偿号，根据letter检查value是否合法，刀具号不能超出刀具数量
            case'H':break; //刀具长度补偿号，根据letter检查value是否合法，刀具号不能超出刀具数量

            case'F':modal.feed = words[i].value; break;      //进给速度，根据letter检查value是否合法，不能超出最大速度
            case'S':modal.spindleRpm = words[i].value; break;//主轴转速，根据letter检查value是否合法，不能超出最大转速

            case'X':break; //X轴，根据letter检查value是否合法，会有“0.”“.5”这样的值，不能超出机床行程
            case'Y':break; //Y轴，根据letter检查value是否合法，会有“0.”“.5”这样的值，不能超出机床行程
            case'Z':break; //Z轴，根据letter检查value是否合法，会有“0.”“.5”这样的值，不能超出机床行程
            case'A':break; //4轴，根据letter检查value是否合法，会有“0.”“.5”这样的值，不能超出机床行程（若有）
            case'B':break; //5轴，根据letter检查value是否合法，会有“0.”“.5”这样的值，不能超出机床行程（若有）
            case'C':break; //6轴，根据letter检查value是否合法，会有“0.”“.5”这样的值，不能超出机床行程（若有）

            case'I':break; //X轴圆心偏移，根据letter检查value是否合法，会有“0.”“.5”这样的值
            case'J':break; //Y轴圆心偏移，根据letter检查value是否合法，会有“0.”“.5”这样的值
            case'K':break; //Z轴圆心偏移，根据letter检查value是否合法，会有“0.”“.5”这样的值
            case'R':break; //圆弧半径

            case'L':break; //重复次数，根据letter检查value是否合法，必须是整数
            case'N':break; //行号，根据letter检查value是否合法，行号是否超出

            case'P':break; //参数
            case'Q':break; //参数
            case'V':break; //加工参数组号
                
            default:
            {
                //所有case都没命中，报错
                return RecordError(lineNo, line, ErrorKind::Unsupported_Letter, std::string(), err);
                break;
            }

        }
            
        i++;

        

    }
    return true;
}