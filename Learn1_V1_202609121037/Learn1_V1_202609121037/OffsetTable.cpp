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
	// m_axisCount是int类型，是存储该机床轴数的。
	OffsetSet::OffsetSet(int axisCount) :m_list(), m_axisCount(axisCount)
	{
		;
	}

	//设置工件坐标系偏置Base，在分中的时候提供
	bool OffsetSet::setBase(Frame frame, Axis axis, double value, std::vector<OffsetError>& err)
	{
		if (!CheckAxis) { ; }
		if (!CheckFrame) { ; }
		if (!CheckValue) { ; }
		m_list[(int)frame][axis].Base = value;
	}

	//设置工件坐标系偏置Trim，在偏置表中提供
	bool OffsetSet::setTrim(Frame frame, Axis axis, double value, std::vector<OffsetError>& err)
	{
		if (!CheckAxis(frame, axis,)) { ; }
		if (!CheckFrame) { ; }
		if (!CheckValue) { ; }
		m_list[(int)frame][axis].Trim = value;
	}

	
	bool OffsetSet::CheckValue(Frame frame, Axis axis, double value, std::vector<OffsetError>& err)
	{
		if ((int)frame < 0 || (int)frame >= (int)Frame::count)
		{

		}
	}

	bool OffsetSet::CheckAxis(Frame frame, Axis axis, std::vector<OffsetError>& err)
	{
		if ((int)axis < 0 || (int)axis >= (int)Axis::count)
		{
			OffsetError e;
			e.axis = axis;
			e.FrameNo = frame;
			e.kind = OffsetErrorKind::axis_nomber_illegal;
			err.push_back(e);
		}
		if ((int)axis >= m_axisCount)
		{
			OffsetError e;
			e.axis = axis;
			e.FrameNo = frame;
			e.kind = OffsetErrorKind::axis_nomber_exceed_axis_count;
			err.push_back(e);
		}
	}
	bool OffsetSet::CheckFrame(Frame frame, std::vector<OffsetError>& err)
	{

	}
	
}

