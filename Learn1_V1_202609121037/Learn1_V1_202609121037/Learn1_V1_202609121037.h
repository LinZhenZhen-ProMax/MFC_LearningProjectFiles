
// Learn1_V1_202609121037.h: PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// 主符号


// CLearn1V1202609121037App:
// 有关此类的实现，请参阅 Learn1_V1_202609121037.cpp
//

//这里是定义一个类CLearn1V1202609121037App继承自CWinApp
class CLearn1V1202609121037App : public CWinApp
{
//这里是声明这个类里面有一个构造函数，函数体在.cpp里
public:
	CLearn1V1202609121037App();

// 重写：子类提供了一个"同名同参数"的版本，把父类的版本"顶替掉"，
// virtual是在父类中允许重写的标志，子类中可写可不写
// 会在重写的函数后面加一个关键字override，告诉编译器："我打算重写父类的虚函数，请帮我检查"，
// 如果函数名写错，没加 override → 编译器认为这是个【全新函数】，编译通过
// 编译器会给父类和子类各生成一张指针表VTable，
// 父类中带有virtual关键字的函数才会被写进表里，不带virtual关键字的函数跟类是绑定的，
// 没重写的时候，子类指针表中的指针指向父类声明的函数，
// 重写函数的时候，指针表中的指针会被替换成指向子类重写的函数
public:
	virtual BOOL InitInstance() override;

// 实现

	DECLARE_MESSAGE_MAP()
};

extern CLearn1V1202609121037App theApp;
