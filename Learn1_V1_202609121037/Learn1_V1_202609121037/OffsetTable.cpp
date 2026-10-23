#include "pch.h"
#include "OffsetTable.h"


namespace CNC
{
	//OffsetList的构造函数，创建OffsetList时即执行，将六个坐标系六个轴两个参数全部清零
	OffsetList::OffsetList()
	{
		for (int i = 0; i < (int)Axis::count; i++)
		{
			m_axis[i].Clear();
		}
	}

	// m_list作为OffsetSet的成员，在构造函数执行之前就已经创建好，
	// m_list是OffsetList类类型，所以会创建出六个OffsetList对象，OffsetList的构造函数会将六个轴的参数全部清零
	// m_axisCount是int类型，存储该机床轴数，在setBase，setTrim设置偏置的时候会用这个变量检查是否超出机床轴数。
	OffsetSet::OffsetSet(int axisCount) :m_list(), m_axisCount(axisCount)
	{
		;
	}

	//设置工件坐标系偏置Base，在分中的时候提供
	bool OffsetSet::SetBase(Frame                   frame, 
		                    Axis                     axis, 
		                    double                  value, 
		                    const SetSoftLimit&    limits, 
		                    std::vector<OffsetError>& err)
	{
		std::vector<OffsetError>seterror;

		double total = m_list[(int)frame][axis].Trim + value;

		if (!Check(frame, axis, total, limits, seterror))
		{
			err.insert(err.end(), seterror.begin(), seterror.end());
		}
		
		m_list[(int)frame][axis].Base = value;
	}

	//设置工件坐标系偏置Trim，在偏置表中提供
	//每次set都会新建函数内报错数组，所以不会有报旧错的情况
	//Trim+Base加在一起比较软限
	bool OffsetSet::SetTrim(Frame                   frame, 
		                    Axis                     axis, 
		                    double                  value, 
		                    const SetSoftLimit&    limits, 
		                    std::vector<OffsetError>& err)
	{
		std::vector<OffsetError>seterror;

		double total = m_list[(int)frame][axis].Base + value;

		if (!Check(frame, axis, total, limits, seterror))
		{
			err.insert(err.end(), seterror.begin(), seterror.end());
		}

		m_list[(int)frame][axis].Trim = value;
	}

	// 尝试将value、axis、frame三个check整合成一个,
	// 枚举型赋值只认类型名，只要类型名正确，(frame)99也能存进去，所以需要验证是否超出
	bool OffsetSet::Check(Frame frame, Axis axis, double total, const SetSoftLimit& limits, std::vector<OffsetError>& err)
	{
		bool iserror = false;

		//轴号是否合法
		if ((int)axis < 0 || (int)axis >= (int)Axis::count)
		{
			OffsetError e;
			e.AxisNo = axis;
			e.FrameNo = frame;
			e.kind = OffsetErrorKind::axis_nomber_illegal;
			err.push_back(e);
			iserror = true;
		}

		//是否超出设定的轴数，默认三轴
		if ((int)axis < 0 || (int)axis >= m_axisCount)
		{
			OffsetError e;
			e.AxisNo = axis;
			e.FrameNo = frame;
			e.kind = OffsetErrorKind::axis_nomber_exceed_axis_count;
			err.push_back(e);
			iserror = true;
		}

		//坐标系号是否合法
		if ((int)frame < 0 || (int)frame >= (int)Frame::count)
		{
			OffsetError e;
			e.AxisNo = axis;
			e.FrameNo = frame;
			e.kind = OffsetErrorKind::frame_nomber_illegal;
			err.push_back(e);
			iserror = true;
		}

		//是否超出机床软限，for轮询第一第二软限，软限未上使能则跳过
		for (int i = 0; i < (int)CNC::SoftLimitSlot::count; ++i)
		{
			const SoftLimitRange& range = limits[axis][(SoftLimitSlot)i];
			if (!range.Enable)continue;

			if (total < range.Min || total > range.Max)
			{
				OffsetError e;
				e.AxisNo = axis;
				e.FrameNo = frame;
				e.kind = OffsetErrorKind::axis_value_exceed_soft_limit;
				err.push_back(e);
				iserror = true;
			}
		}
		
		//上述四项检测都做完才会返回，返回标志位取反，有报错return false
		return !iserror;
	}
	
	
}

