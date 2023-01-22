#define success 0
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
#include<string>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<thread>
#include<map>
using namespace std;
#pragma comment(lib,"ws2_32.lib")
char my_ip[100];//服务器ip
int port;//服务器端口号
char name[100];//client名称，由服务器返回，是可以修改的
int flag = 1;//当send函数发送退出聊天室的请求时，接收线程也应该相应停止
char receivebuffer[100];//接收缓冲区
char sendbuffer[100];//发送缓冲区

//时间函数，打印当前时间 年月日
void Y_M_D()
{
	SYSTEMTIME y_m_d;
	GetLocalTime(&y_m_d);
	cout << "今天是——" << y_m_d.wYear << "年" << y_m_d.wMonth << "月" << y_m_d.wDay << "日" << endl;
}

//时间函数，打印当前时间 时分秒
void H_M_S()
{
	SYSTEMTIME h_m_s;
	GetLocalTime(&h_m_s);
	cout << h_m_s.wHour << ":" << h_m_s.wMinute << ":" << h_m_s.wSecond;
}

DWORD WINAPI re_ma(LPVOID lparam);
DWORD WINAPI se_ma(LPVOID lparam);

int main() {
	//打印年月日信息
	cout << "[尊敬的客户],你好!" << endl;
	Y_M_D();
	cout << endl;
	cout << "您正在初始化聊天室服务器端，   loading……" << endl;


	//初始化WSDATA
	WSADATA ws;
	//初始化Socket DLL，协商使用的Socket版本
	//MAKEWORD(2, 2) 调用2.2版本，调用希望使用的最高版本
	//ws 可用Socket的详细信息
	int result;
	result = WSAStartup(MAKEWORD(2, 2), &ws);
	//cout << result;
	H_M_S();
	if (result == success)
	{
		cout << " [WSADATA初始化]:       成功,调用的socket最高版本为2.2！" << endl;
	}
	else
	{
		cout << " WSADATA初始化：失败;错误代号: " << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}

	//初始化一个socket
	SOCKET s;
	//初始化socket，ipv4，数据流，TCP协议
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//s的返回值
	//0,1,2分别表示标准输入、标准输出、标准错误。
	//所以其他打开的文件描述符都会大于2, 错误时就返回 -1. 这里INVALID_SOCKET 也被定义为 -1 
	H_M_S();
	if (s == -1)
	{
		cout << " SOCKET创建失败;错误代号: " << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else
		cout << " [SOCKET创建]:          成功！" << endl;

	//连接
	//sockaddr_in用于socket定义和赋值；
	sockaddr_in sa_in;
	//bzero(&sa_in, sizeof(sa_in)); bzero需要在linux下使用！！！
	//初始化sa_in
	memset(&sa_in, 0, sizeof(sa_in));
	H_M_S();
	cout << " [输入服务器ip地址]:    使用默认ip请输入localhost: ";
	cin >> my_ip;
	if ((string)my_ip == "localhost")
	{

		memset(&my_ip, 0, sizeof(my_ip));
		//cout << "hello";
		strcat(my_ip, "127.0.0.1");
		//cout << "hello";
	}
	//cout << my_ip << endl;
	H_M_S();
	cout << " [请输入服务器端口号]:  请在1024-5000之间选择: ";
	while (1)
	{

		int cin_port;
		cin >> cin_port;
		if (cin_port > 5000 || cin_port < 1024)
		{
			H_M_S();
			cout << " 端口不在支持范围内，请重新输入: ";
		}
		else
		{
			port = cin_port;
			break;
		}
	}
	//Address family一般来说AF_INET（地址族）PF_INET（协议族）,windows下基本无差别
	sa_in.sin_family = AF_INET;
	//ip_address
	sa_in.sin_addr.s_addr = inet_addr(my_ip);
	//port
	sa_in.sin_port = htons(port);
	cout << endl;
	cout << "[服务器ip地址]:     " << my_ip << endl;
	cout << "[服务器port端口号]: " << port << endl;
	//执行连接操作
	result = connect(s, (SOCKADDR*)&sa_in, sizeof(SOCKADDR));
	if (result == success)
	{
		cout << "[目的端ip地址]:     连接成功" << endl;
		cout << "[目的端port端口号]: 连接成功" << endl;
	}
	else
	{
		closesocket(s);//释放socket
		WSACleanup();//释放dll
		cout << "ip与port连接失败;错误代号：" << WSAGetLastError() << endl;
		return 0;
	}

	recv(s, name, 100, 0);//获取用户名，对应server端的第一个send
	cout << "[加入聊天室]:       成功！ 您的用户名是" << name << endl;
	cout << endl;

	cout << "[tips]: " << endl;
	cout << "[改名]:             changename newname 例:changename user1" << endl;
	cout << "[私聊消息]:         sendone username message 例:sendone 230 message"<<endl;
	cout << "[退出聊天室]:       quit"<< endl;
	cout << endl;
	HANDLE t[2];
	t[0] = CreateThread(NULL, 0, re_ma, (LPVOID)&s, 0, NULL);
	t[1] = CreateThread(NULL, 0, se_ma, (LPVOID)&s, 0, NULL);
	WaitForMultipleObjects(2, t, TRUE, INFINITE);
	CloseHandle(t[1]);
	CloseHandle(t[0]);
	closesocket(s);
	WSACleanup();
}

DWORD WINAPI re_ma(LPVOID lparam) {
	SOCKET* s = (SOCKET*)lparam;
	int rflag;
	while (true) {
		rflag = recv(*s, receivebuffer, 100, 0);
		if (rflag > 0 && flag) {
			cout << endl;
			H_M_S();
			cout << " [新消息]:   ";
			cout << receivebuffer;
			cout << "    ---请积极发言" << endl;;
		}
		else {
			closesocket(*s);
			return 0;
		}
	}
}

DWORD WINAPI se_ma(LPVOID lparam) {
	SOCKET* s = (SOCKET*)lparam;
	int sflag;
	while (true) {
		cin.getline(sendbuffer, 100);
		if (string(sendbuffer) == "quit") {
			sflag = send(*s, sendbuffer, 100, 0);
			flag = 0;
			H_M_S();
			cout << " [退出聊天]: 再见! " << endl;
			closesocket(*s);
			return 0;
		}
		sflag = send(*s, sendbuffer, 100, 0);
		if (sflag == SOCKET_ERROR) {
			H_M_S();
			cout << " [消息发送]: 失败; 错误代号" << WSAGetLastError() << endl;
			closesocket(*s);
			WSACleanup();
			return 0;
		}
		else {
			H_M_S();
			cout << " [消息发送]: 成功"<<endl;
		}
	}
}
