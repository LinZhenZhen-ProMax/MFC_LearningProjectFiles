#pragma once

#include <vector>
#include <string>

#include "CNCTypes.h"



// 仿照OffsetTable类做的软限类
namespace CNC
{

	// 第一层声明软限结构体，某一轴的正负软限范围和使能
	struct SoftLimitRange
	{
		double   Min = 0.0;
		double   Max = 0.0;
		bool  Enable = false;
	};

	// 第二层枚举软限槽位，第一软限第二软限
	enum struct SoftLimitSlot
	{
		FirstLimit=0, // 第一软限
		SecondLimit,// 第二软限

		count,
	};

	// 第三层结构是Axis枚举，已在CNCTypes.h中完成

	// 报警类型
	enum struct LimitErrorKind
	{
		no_error,
		exceed_limiting_position,
		setting_limit_beyond_machine_range,
	};

	// 软限报警结构体，
	struct LimitError
	{
		Axis             AxisNo = Axis::X;
		SoftLimitSlot      Slot = SoftLimitSlot::FirstLimit;
		double  CurrentPosition = 0.0;
		double            Limit = 0.0;
		LimitErrorKind     Kind = LimitErrorKind::no_error;
	};

	// 第二层结构的类
	class SoftLimitList
	{
	public:

		      SoftLimitRange& operator[](SoftLimitSlot slot)       { return m_limit[(int)slot]; }
		const SoftLimitRange& operator[](SoftLimitSlot slot) const { return m_limit[(int)slot]; }

	private:
		SoftLimitRange m_limit[(int)SoftLimitSlot::count];
	};

	// 第三层结构的类，包含一个setLimit函数用来设置软限，一个check检查软限参数合不合规
	class SetSoftLimit
	{
	public:
		

		      SoftLimitList& operator[](Axis axis)       { return m_list[(int)axis]; }
		const SoftLimitList& operator[](Axis axis) const { return m_list[(int)axis]; }

		bool SetRange(Axis axis, SoftLimitSlot slot, double min, double max, std::vector<LimitError>& err);

		bool SetEnable(Axis axis, SoftLimitSlot slot, bool on, std::vector<LimitError>& err);

		bool Check(Axis axis, SoftLimitSlot slot, double min, double max, std::vector<LimitError>& err);



	private:
		SoftLimitList m_list[(int)Axis::count];
	};
}