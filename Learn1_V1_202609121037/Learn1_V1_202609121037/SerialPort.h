/*****************************************
* SerialPort.h
* 声明了SerialPort类
* 可用于打开一个串口
* 声明该类对象可以读写串口
*****************************************/
#pragma once
#include <windows.h>
#include <stdio.h>

class SerialPort
{
public:
	SerialPort();
	~SerialPort();

	bool openserial(const char* PortName,int Baud);
	void close();
	bool isserialopen();

	struct Writeserial_result
	{
		int Bytesent;
		DWORD errorcode;
		bool ok() const { return errorcode == 0; }
	};

	Writeserial_result writeserial(const void* data,int len);

	struct Readserial_result
	{
		int Byterecv;
		DWORD errorcode;
		bool ok() const { return errorcode == 0; }
	};

	Readserial_result readserial(void* data,int len);

	

private:
	HANDLE m_hComm;
};

