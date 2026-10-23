#pragma once

namespace CNC
{
	//枚举轴CNC::Axis
	enum struct Axis
	{
		X = 0,
		Y,
		Z,
		A,
		B,
		C,
		count
	};

	//枚举坐标系CNC::Frame
	enum struct Frame
	{
		G54 = 0,
		G55,
		G56,
		G57,
		G58,
		G59,
		count
	};
}