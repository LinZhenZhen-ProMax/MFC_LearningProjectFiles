#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>

class GCodeModalMachine
{
public:

    //G代码结构体，字母后面跟着数值
	struct Word
	{
		char   letter = 0;
		double value = 0.0;
	};

    enum struct ErrorKind
    {
        no_error,
        not_a_number,
        out_of_range,
        illegal_code,
        cannot_open_file,
        Unsupported_Letter
    };
    
    struct CodeError
    {
        std::string path = 0;
        int lineNo = 0;
        ErrorKind kind = ErrorKind::no_error;
        std::string source = 0;
    };


    /******************************************************
    * 枚举模态机所有模态
    ******************************************************/
	//枚举运动类型，都是常量
	enum struct MotionType
	{
		Rapid = 0,   // G00 快速进给
		Line = 1,     // G01 切削进给
		ArcCW = 2,    // G02 顺时针圆弧插补
		ArcCCW = 3    // G03 逆时针圆弧插补
	};

    //枚举工作平面
    enum struct JogPlane
    {
        XY = 17,
        ZX = 18,
        YZ = 19,
    };

    //枚举尺寸单位
    enum class Units
    {
        Inch = 20,
        Millimeter = 21,
    };

    //枚举刀具半径补偿
    enum class CutterComp
    {
        Off = 40,          // G40 取消
        Left = 41,          // G41 左补偿
        Right = 42,          // G42 右补偿
    };

    //枚举刀具长度补偿
    enum class LengthComp
    {
        Positive = 43,       // G43 正补偿
        Negative = 44,       // G44 负补偿
        Cancel = 49,       // G49 取消
    };

    //枚举工件坐标系
    enum class WorkOffset
    {
        G53 = 53,
        G54 = 54, G55 = 55, G56 = 56,
        G57 = 57, G58 = 58, G59 = 59,
    };

    //枚举坐标计算方式
    enum class CoordinateMode
    {
        Absolute = 90,    // G90绝对值编程
        Incremental = 91,    // G91增量式编程
    };

    //枚举主轴转动
    enum class SpindleDirection
    {
        CW = 3,            // M03 正转
        CCW = 4,            // M04 反转
        Stop = 5,            // M05 停
    };

    enum class Coolant
    {
        Flood = 8,           // M08 工件冷却
        Off = 9,           // M09 工件冷却关
    };
    /******************************************************
    * 模态枚举结束
    ******************************************************/

    //模态结构体，系统当前的模态，将所有互斥的模态归入一个槽位
    struct ModalState
    {
        MotionType    motion = MotionType::Rapid;       // ---- 槽位 1: 运动模式

        JogPlane       plane = JogPlane::XY;            // ---- 槽位 2：加工平面

        Units          units = Units::Millimeter;       // ---- 槽位 3: 尺寸单位
        
        CutterComp     ccomp = CutterComp::Off;         // ---- 槽位 4: 刀具半径补偿

        LengthComp     lcomp = LengthComp::Positive;      // ---- 槽位 5: 刀具长度补偿

        WorkOffset    offset = WorkOffset::G54;         // ---- 槽位 6: 坐标系

        CoordinateMode coord = CoordinateMode::Absolute;// ---- 槽位 7: 坐标计算方式

        Coolant      coolant = Coolant::Off;           // ---- 槽位 9: 工件冷却

        SpindleDirection spindleDir = SpindleDirection::Stop;     // ---- 槽位 8: 主轴转向
        
        
        double feed = 0.0;

        double spindleRpm = 0.0;

        int  tool = 0;

        int  lineNo = 0;

        // ---- ⭐ 当前位置: 上一条运动段的终点 ----
        //      解释器必须维护它, 因为 G91 增量要靠它算
        double currentX = 0.0;
        double currentY = 0.0;
        double currentZ = 0.0;
    };

    struct MotionSegment
    {
        MotionType type = MotionType::Rapid;

        double x0 = 0.0, y0 = 0.0, z0 = 0.0;   // 起点
        double x1 = 0.0, y1 = 0.0, z1 = 0.0;   // 终点
        double feed = 0.0;                      // mm/min

        int lineNo = 0;                         // 原始行号 (出错要能报"第 N 行")
    };
    /***************************************
    * LoadProgram函数读程序并把单行程序输出，行号输出
    ***************************************/
    bool GCodeModalMachine::LoadProgram(const std::string& path, std::vector<CodeError>& outWords, std::vector<CodeError>& outErrors, ModalState& modal);

    /****************************************
    * LexLine函数逐行读加工程序，切成 word 数组
    * 输入: "N40 G01 X50.0 (粗加工)"
    * 输出: [ {N,40}, {G,1}, {X,50.0} ]
    * 去空格、丢注释、转大写
    * 输入参数类型，std::string，std::vector<Word>，std::string
    ****************************************/
    bool GCodeModalMachine::LexLine(const int lineNo, const std::string& line, std::vector<Word>& words, std::vector<CodeError>& err);

    /****************************************
    * ParseLine函数解析一行, 更新模态和当前位置, 产出运动段
    ****************************************/
    bool GCodeModalMachine::ParseLine(const int lineNo, 
                                      const std::string& line, 
                                      const std::vector<Word>& words, 
                                      ModalState& modal, 
                                      MotionSegment& outSeg, 
                                      std::vector<CodeError>& err);

    /****************************************
    * ApplyOffset函数把"程序坐标"换算成"机床坐标"
    ****************************************/ 
    void ApplyOffset(double& x, double& y, double& z, const ModalState& modal);
};