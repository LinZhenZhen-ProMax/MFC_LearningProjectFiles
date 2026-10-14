bool GCodeModalMachine::ParseLine(const int lineNo,               //行号
    const std::string& line,        //行
    const std::vector<Word>& words, //
    ModalState& modal,              //状态输出
    MotionSegment& outSeg,          //运动段输出
    std::vector<CodeError>& err)    //报错输出
{
    size_t i = 0;
    while (i < words.size())
    {
        

        i++;



    }
    return true;
}



case'G':
{
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
    CodeError e;
    e.lineNo = lineNo;
    e.source = line;
    e.kind = ErrorKind::Unsupported_Letter;
    err.push_back(e);
    return false;
    break;
}


enum struct ValueKind
{
    Integer,
    Real,
    Hundred,
    Either
};

ValueKind ValueKindOf(char letter)
{
    switch (letter)
    {
    case 'N': case 'G': case 'M': case 'T':
    case 'H': case 'D': case 'L': case 'O':
        return ValueKind::Integer;

    case 'X': case 'Y': case 'Z':
    case 'A': case 'B': case 'C':
    case 'I': case 'J': case 'K':
    case 'R': 
        return ValueKind::Real;

    case 'F': case 'S':
        return ValueKind::Hundred;
    
    default:
        return ValueKind::Either;
    }
}
bool ValueKindCheck(ValueKind& valuekind,char letter)
{
    switch (valuekind)
    {
        case ValueKind::Integer:
        {

            break;
        }
        case ValueKind::Real:
        {
            break;
        }
        case ValueKind::Hundred:
        {
            break;
        }
        case ValueKind::Integer:
        {
            break;
        }
    }
}

