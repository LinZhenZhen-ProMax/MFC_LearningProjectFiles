/*********************************************
* SerialPort.cpp
* 声明SerialPort类的函数
* 包括SerialPort()构造函数
*    ~SerialPort()析构函数
*    openserial(const char* PortName, int Baud)串口打开函数
*    close()串口关闭函数
*    isserialopen()串口状态检测函数
**********************************************/
#include "pch.h"
#include "SerialPort.h"


/**********************************************
* SerialPort::SerialPort()
* 构造函数
* 1.初始化句柄
**********************************************/
SerialPort::SerialPort()
{
	m_hComm = INVALID_HANDLE_VALUE;
}
/**********************************************
* SerialPort::~SerialPort()
* 析构函数
* 1.对象死亡自动关串口
**********************************************/
SerialPort::~SerialPort()
{
	SerialPort::close();
}
/**********************************************
* bool SerialPort::openserial(const char* PortName, int Baud)
* 串口打开函数
* 1.打开前先关上
* 2.将传入的com口前面添加\\\\.\\
* 3.这个是Win32API给出来的串口打开函数CreateFileA，返回一个句柄，后面6个参数不用动
* 4.检查串口是否打开
* 5.声明一个DCB对象，把dcb里面所有变量先清零，再把长度填回去，
*   Get会根据dcb的长度填写参数，清零那一步把长度清了填不进去，
*   把句柄串口的已有的参数写在dcb上，修改dcb，把dcb写回句柄对应的串口里
* 6.声明一个COMMTIMEOUTS对象，管“超时”的，里面只有五个参数，前三个参数是管读串口的，后面两个参数是管写串口的
*   ReadIntervalTimeout管接收到字符之后，gap多久确认没有字符来了，再关闭
*   ReadTotalTimeoutConstant和ReadTotalTimeoutMultiplier分别是常数项和系数项，等待时间=字节数*系数+常数，
*   这两个是管发起“读”串口之后gap多久确认没有字符来
**********************************************/
bool SerialPort::openserial(const char* PortName, int Baud)
{
	SerialPort::close();

	char fullname[64];
	sprintf_s(fullname, 64, "\\\\.\\%s", PortName);

	m_hComm = CreateFileA(fullname, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	
	if (m_hComm == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	
	DCB dcb;
	memset(&dcb, 0, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(m_hComm, &dcb))
	{
		SerialPort::close();
		return false;
	}

	dcb.BaudRate = Baud;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;

	if (!SetCommState(m_hComm, &dcb))
	{
		SerialPort::close();
		return false;
	}

	COMMTIMEOUTS ctouts;
	memset(&ctouts, 0, sizeof(ctouts));

	if (!GetCommTimeouts(m_hComm, &ctouts))
	{
		SerialPort::close();
		return false;
	}

	ctouts.ReadIntervalTimeout = 50;
	ctouts.ReadTotalTimeoutConstant = 50;
	ctouts.ReadTotalTimeoutMultiplier = 0;
	ctouts.WriteTotalTimeoutConstant = 5000;
	ctouts.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(m_hComm, &ctouts))
	{
		SerialPort::close();
		return false;
	}


	return true;
}
/**********************************************
* void SerialPort::close()
* 串口关闭函数，如果串口不是关的，就给他关了
**********************************************/
void SerialPort::close()
{
	if (m_hComm != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hComm);
		m_hComm = INVALID_HANDLE_VALUE;
	}
}
/**********************************************
* bool SerialPort::isserialopen()
* 串口状态监测函数
**********************************************/
bool SerialPort::isserialopen()
{
		return m_hComm != INVALID_HANDLE_VALUE;
}
/**********************************************
* SerialPort::Writeserial_result SerialPort::writeserial (const void* data,int len)
* 写串口函数
* 函数返回结构体，结构体里包含报错信息
* 初始化结构体
* 先检测串口有没有正常打开，要发送的内容正不正常等问题
* if条件内执行写串口函数WriteFile，返回BOOL，报错则检测
* 变量比较宽容，int类型传给DWORD（实际就是u long）也可以，但是指针很严格，DWORD指针传给DWORD指针
**********************************************/
SerialPort::Writeserial_result SerialPort::writeserial (const void* data,int len)
{
	Writeserial_result result;
	result.Bytesent = 0;
	result.errorcode = 0;

	if (!isserialopen())
	{
		result.errorcode = ERROR_INVALID_HANDLE;
		return result;
	}
	if (data == nullptr || len <= 0)
	{
		result.errorcode = ERROR_INVALID_PARAMETER;
		return result;
	}

	DWORD written = 0;

	if (!::WriteFile(m_hComm, data, len, &written, NULL))
	{
		result.errorcode = ::GetLastError();
		result.Bytesent = written;
		return result;
	}

	result.Bytesent = written;
	return result;
}
/**********************************************
* SerialPort::Readserial_result SerialPort::readserial(void* data, int len)
* 读串口函数
* 与写串口函数是对称的，函数内变量都用了同一个名字，
**********************************************/
SerialPort::Readserial_result SerialPort::readserial(void* data, int len)
{
	Readserial_result result;
	result.Byterecv = 0;
	result.errorcode = 0;

	if (!isserialopen())
	{
		result.errorcode = ERROR_INVALID_HANDLE;
		return result;
	}
	if (data == nullptr || len <= 0)
	{
		result.errorcode = ERROR_INVALID_PARAMETER;
		return result;
	}

	DWORD ridden = 0;

	if (!ReadFile(m_hComm, data, len, &ridden, NULL))
	{
		result.errorcode = ::GetLastError();
		result.Byterecv = ridden;
		return result;
	}

	result.Byterecv = ridden;
	return result;
}
