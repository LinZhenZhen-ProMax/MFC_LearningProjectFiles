#include "pch.h"
#include "SoftPositionLimit.h"


namespace CNC
{

	bool SetSoftLimit::SetRange(Axis axis, SoftLimitSlot slot, double min, double max, std::vector<LimitError>& err)
	{
		m_list[(int)axis][slot].Min = min;
		m_list[(int)axis][slot].Max = max;
	}
	
	bool SetSoftLimit::SetEnable(Axis axis, SoftLimitSlot slot, bool on, std::vector<LimitError>& err)
	{
		m_list[(int)axis][slot].Enable = on;
	}

	bool SetSoftLimit::Check(Axis axis, SoftLimitSlot slot, double min, double max, std::vector<LimitError>& err)
	{

	}

}
