#define success 0 //成功的返回标记为0
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<map>
#include<vector>
#include<string>
#include<thread>
//#include<string.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;
char my_ip[100];
char message[100];
int port;
map<string, string>users;//记录客户端的姓名
map<SOCKET, int>sockets;//记录每个客户端对接的socket
using namespace std;
////////一些功能函数//////////////////
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
//判断是否为改名请求
bool judge_changename(char* c) {
	if (c[0] == 'c' && c[1] == 'h' && c[2] == 'a' && c[3] == 'n' && c[4] == 'g' && c[5] == 'e' && c[6] == 'n' && c[7] == 'a' && c[8] == 'm' && c[9] == 'e')
		return true;
	return false;
}
//判断是否为私聊请求
bool judge_sendone(char* buff) {
	if (buff[0] == 's' && buff[1] == 'e' && buff[2] == 'n' && buff[3] == 'd' && buff[4] == 'o' && buff[5] == 'n'&&buff[6]=='e') 
		return true;
	return false;
}

//提取新名字
string newname(char* buff) {
	string s = "";
	for (int i = 11; i <= 99; i++) {
		if (buff[i] == '\0' || buff[i] == ' ' || buff[i] == '/t')
			return s;
		else 
			s += buff[i];
	}
}
//私聊的目标用户
string target_user(char* c) {
	string s = "";
	for (int i = 8; i <= 99; i++) {
		if (c[i] == '\0' || c[i] == ' ' || c[i] == '/t') 
			return s;
		else 
			s += c[i];
	}
}

bool judgequit(char* c)
{
	if (c[0] == 'q' && c[1] == 'u' && c[2] == 'i' && c[3] == 't' && c[4] == '\0')
		return true;
	else
		return false;
}

//收发信息，打印等都在线程中执行，每一个客户端都有一个socket进行对接
DWORD WINAPI my_thread(LPVOID lparam)
{
	//获取客户端传来的socket
	SOCKET cs = (SOCKET)lparam;
	//map的find函数：
	//在容器中寻找值为cs的元素，返回该元素的迭代器。否则，返回map.end()
	if (sockets.find(cs) == sockets.end())
		sockets.insert(pair<SOCKET, int>(cs, 1));//加入到记录socket的map中
	else
		sockets[cs] = 1;

	char name[100];
	//data()生成一个const char*指针，指向数组(不以空字符终止)
	strcpy(name, to_string(cs).data());

	//send 发送用户新加入消息
	send(cs, (const char*)name, 100, 0);
	cout << endl;
	H_M_S();
	cout << " [新用户]: " << to_string(cs).data() << "加入聊天" << endl;

	char sendbuffer[100];//发送消息缓冲区
	char receivebuffer[100];//接收消息缓冲区
	int rflag = 1;//接收
	int sflag = 1;//发送
	int flag = 1;

	while (rflag != -1 && flag != 0)
	{
		rflag = recv(cs, receivebuffer, 100, 0);

		//判断quit break,销毁该socket
		if (judgequit(receivebuffer))
		{
			// mymap[cs] = 0;
			if (users.find(to_string(cs).data()) == users.end())
			{
				H_M_S();
				cout << " [用户退出]: 用户[" << to_string(cs).data() << "]退出了群聊" << endl;
				sockets.erase(cs);
				break;
			}
			else
			{
				H_M_S();
				cout << " [用户退出]: 用户[" << users[to_string(cs).data()] << "]退出了群聊" << endl;
				sockets.erase(cs);
				break;
			}
		}

		//判断改名 
		if (judge_changename(receivebuffer))
		{
			cout << endl;
			//新改的名字
			string newName = newname(receivebuffer);
			//记录新名字，insert也可以
			users[to_string(cs).data()] = newName;
			//打印新名字
			H_M_S();
			cout << " [用户改名]: " << to_string(cs).data() << " 修改昵称为 " << newName << "" << endl;
			continue;
		}

		//发送私聊消息
		if (judge_sendone(receivebuffer))
		{
			bool stflag = false;
			string s = target_user(receivebuffer);
			for (auto it : sockets)
			{
				//cout << it.first;
				//私聊功能
				//改名了
				if (users[to_string(it.first).data()] == s)
				{
					//创建发送的消息
					strcpy(sendbuffer, "用户");
					//发送消息的人
					if (users.find(to_string(cs).data()) == users.end()) {
						strcat(sendbuffer, to_string(cs).data());
					}
					else {
						strcat(sendbuffer, users[to_string(cs).data()].c_str());
					}

					strcat(sendbuffer, " 对你说：");
					// cout << message << endl;
					strcat(sendbuffer, (const char*)receivebuffer);
					H_M_S();
					if (users.find(to_string(cs).data()) == users.end()) {
						cout << " [用户私聊]: " << to_string(cs).data() << "说: " << receivebuffer << endl;
					}
					else {
						cout << " [用户私聊]: " << users[to_string(cs).data()] << "说: " << receivebuffer << endl;
					}
					send(it.first, sendbuffer, 100, 0);
					stflag = true;
					break;
				}

				//没改名
				if (to_string(it.first).data() == s) {
					strcpy(sendbuffer, "用户");
					if (users.find(to_string(cs).data()) == users.end()) {
						strcat(sendbuffer, to_string(cs).data());
					}
					else {
						strcat(sendbuffer, users[to_string(cs).data()].c_str());
					}
					strcat(sendbuffer, " (私聊): ");
					strcat(sendbuffer, (const char*)receivebuffer);
					H_M_S();
					if (users.find(to_string(cs).data()) == users.end()) {
						cout << " [用户私聊]: " << to_string(cs).data() << "说: " << receivebuffer << endl;
					}
					else {
						cout << " [用户私聊]: " << users[to_string(cs).data()] << "说: " << receivebuffer << endl;
					}
					send(it.first, sendbuffer, 100, 0);
					stflag = true;
					break;
				}
			}

			//告诉发出请求的客户端发送失败
			if (stflag == false)
			{
				strcpy(sendbuffer, "发送失败");
				send(cs, sendbuffer, 100, 0);
				H_M_S();
				cout << " [私聊信息]: 发送失败" << endl;
			}
			continue;
		}

		// 群发
		if (rflag > 0)
		{

			strcpy(sendbuffer, "用户");

			if (users.find(to_string(cs).data()) == users.end()) {
				strcat(sendbuffer, to_string(cs).data());
			}
			else {
				strcat(sendbuffer, users[to_string(cs).data()].c_str());
			}

			strcat(sendbuffer, " 说：");

			//if (mymap[cs] == 1)
			//		strcat(sendbuffer, "欢迎畅所欲言！");
				

			strcat(sendbuffer, (const char*)receivebuffer);
			cout << endl;

			//服务器端打印
			H_M_S();
			if (sockets[cs] == 1)
			{	
				strcpy(sendbuffer, "新的用户");
				strcat(sendbuffer, to_string(cs).data());
				strcat(sendbuffer, "加入!");
				sockets[cs] += 1;
				if (users.find(to_string(cs).data()) == users.end())
					cout << " [用户发言]: " << to_string(cs).data() << "说: " << "欢迎畅所欲言！" << endl;
				else
					cout << " [用户发言]: " << users[to_string(cs).data()] << "说: " <<  "欢迎畅所欲言！" << endl;
			}
			else {
				if (users.find(to_string(cs).data()) == users.end()) 
					cout << " [用户发言]: " << to_string(cs).data() << "说: " << receivebuffer << endl;
				else 
					cout << " [用户发言]: " << users[to_string(cs).data()] << "说: " << receivebuffer << endl;
			}

			for (auto it : sockets) {

				if (it.first != cs && it.second == 2) {
					///cout<<sendbuffer<<endl;
					auto ans = send(it.first, sendbuffer, 100, 0);
					if (ans == SOCKET_ERROR) {
						cout << "[群聊信息]: " << it.first << "发送失败，" << "错误代号： " << endl;
					}
				}
			}
		}
		else
		{
			flag = 0;
		}
	}
	return 0;
}

//主函数
//创建socket→绑定端口号→开始监听→接收请求→为每一个客户端创建通信的socket
int main()
{
	//打印年月日信息
	cout << "[尊敬的管理员],你好!" << endl;
	Y_M_D();
	cout << endl;
	cout << "您正在初始化聊天室服务器端，loading……" << endl;


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
		cout << " [WSADATA初始化]: 成功,调用的socket最高版本为2.2！" << endl;
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
		cout << " [SOCKET创建]:    成功！" << endl;

	//绑定bind
	//sockaddr_in用于socket定义和赋值；
	sockaddr_in sa_in;
	//bzero(&sa_in, sizeof(sa_in)); bzero需要在linux下使用！！！
	//初始化sa_in
	memset(&sa_in, 0, sizeof(sa_in));
	H_M_S();
	cout << " [输入ip地址]:    使用默认ip请输入localhost: ";
	cin >> my_ip;
	if ((string)my_ip == "localhost")
	{
		memset(&my_ip, 0, sizeof(my_ip));
		strcat(my_ip, "127.0.0.1");
	}
	//cout << my_ip << endl;
	H_M_S();
	cout << " [请输入端口号]:  请在1024-5000之间选择: ";
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
	cout << "[ip地址]:     " << my_ip << endl;
	cout << "[port端口号]: " << port << endl;

	//执行绑定操作
	result = bind(s, (SOCKADDR*)&sa_in, sizeof(SOCKADDR));
	if (result == success)
	{
		cout << "[ip地址]:     绑定成功" << endl;
		cout << "[port端口号]: 绑定成功" << endl;
	}
	else
	{
		closesocket(s);//释放socket
		WSACleanup();//释放dll
		cout << "ip与port绑定失败;错误代号：" << WSAGetLastError() << endl;
		return 0;
	}

	//监听
	result = listen(s, 20);
	if (result == success)
	{
		cout << "\n现在是: ";
		H_M_S();
		cout << endl;
		cout << "[聊天室初始化]: 成功!" << endl;
		cout << "[容量]:         20人" << endl;
		cout << "[监听]:         开始!" << endl;
	}
	else
	{
		closesocket(s);//释放socket
		WSACleanup();//释放dll
		cout << "监听失败; 错误代号：" << WSAGetLastError() << endl;
		return 0;
	}

	cout << endl;

	cout << "------等待一大批用户加入！------" << endl;

	while (true)
	{

		sockaddr_in client_sa_in;
		int addr_temp = sizeof(sockaddr_in);
		//接受一个特定socket请求等待队列中的连接请求，未收到请求会阻塞

		//s：socket描述符。
		//addr：返回远程端地址。
		//namelen：返回地址长度。

		//对接客户端的socket
		SOCKET c_s;
		c_s = accept(s, (SOCKADDR*)&client_sa_in, &addr_temp);
		if (c_s == -1)
		{
			H_M_S();
			closesocket(s);//释放socket
			WSACleanup();//释放dll
			cout << " 连接客户端失败;错误代号：" << WSAGetLastError() << endl;
		}
		else
		{
			HANDLE cthread = CreateThread(NULL, 0, my_thread, LPVOID(c_s), 0, NULL);
			CloseHandle(cthread);
		}

	}
	closesocket(s);//释放socket资源
	WSACleanup();//释放dll资源
	cout << endl;
	cout << "[服务端关闭]: 再见！" << endl;
}

