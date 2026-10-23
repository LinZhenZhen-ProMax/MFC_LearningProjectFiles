#pragma once

#include <vector>
#include <string>

#include "CNCTypes.h"
#include "SoftPositionLimit.h"

// 枚举轴和工件坐标系依旧在CNC命名空间内，转移到了CNCTypes.h
namespace CNC
{
	
	// 轴偏置参数结构体，包括基础偏置和修调偏置，提供三个函数用于计算，查询，清零。同时这个也是第一层结构
	struct AxisOffset
	{
		double Base = 0.0;   // 
		double Trim = 0.0;   // 

		double Total() const { return Base + Trim; }
		bool   IsZero() const { return Base == 0.0 && Trim == 0.0; }
		void   Clear() { Base = 0.0; Trim = 0.0; }
	};

	//坐标系偏置错误类型
	enum struct OffsetErrorKind
	{
		no_error,
		axis_nomber_illegal,
		axis_nomber_exceed_axis_count,
		frame_nomber_illegal,
		frame_nomber_exceed_frame_count,
		axis_value_exceed_soft_limit
	};

	//报错结构体，包含三个字段FrameNo，axis，kind
	struct OffsetError
	{
		Frame         FrameNo = Frame::G54;
		Axis           AxisNo = Axis::X;
		OffsetErrorKind  kind = OffsetErrorKind::no_error;
	};

	// 第二层结构，用于存放一个工件坐标系里的六个轴的偏置参数
	// operator[]()是重载函数，使用中括号就能调用，调用时[]中括号内是参数(Axis axis)
	// 参数是枚举出来的轴号，XYZABC，将枚举强行转化成整数作为偏置参数数组的下标，
	// 只要往中括号内输入枚举出来的轴号（例如 X）就能定位到相应参数数组元素，元素是一个结构体
	// 轴偏置参数结构体，包括基础偏置和修调偏置，提供三个函数用于计算，查询，清零。
	class OffsetList
	{
	public:

		OffsetList();//构造函数

		      AxisOffset& operator[](Axis axis)       { return m_axis[(int)axis]; }
		const AxisOffset& operator[](Axis axis) const { return m_axis[(int)axis]; }

	private:

		//声明一个AxisOffset数组长度为6
		AxisOffset m_axis[(int)Axis::count];

	};

	// 第三层结构，原理同第二层，用于存放一个机床里的六个工件坐标系
	class OffsetSet
	{
	public:

		OffsetSet(int axisCount = 3);//构造函数，默认实参只决定实参省略时传进来什么，若在构造函数中不赋值，则默认是三轴

		      OffsetList& operator[](Frame frame)       { return m_list[(int)frame]; }
		const OffsetList& operator[](Frame frame) const { return m_list[(int)frame]; }

		// 设定偏置函数
		bool SetBase(Frame frame, Axis axis, double value, const SetSoftLimit& limits, std::vector<OffsetError>& err);
		bool SetTrim(Frame frame, Axis axis, double value, const SetSoftLimit& limits, std::vector<OffsetError>& err);

		// 检查偏置函数输入参数是否正确
		bool Check(Frame frame, Axis axis, double total, const SetSoftLimit& limits, std::vector<OffsetError>& err);
		
		
	private:

		//声明一个OffsetList数组长度为6
		OffsetList m_list[(int)Frame::count];

		int m_axisCount;
	};

}
