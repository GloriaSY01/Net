#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
using namespace std;
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<string>
#pragma comment(lib,"ws2_32.lib")

/*
	UDP协议
	rdt 3.0
	停等
*/

const u_short IP = 0x7f01;//127.0.0.1 数据头存的都是16位，中间的0省略了
const u_short SourceIp = IP;
const u_short RouterIp = IP;
const u_short DestIp = IP;
int ip_32 = 2130706433;//127.0.0.1->0111 1111 0000 0000 0000 0000 0000 0001->2130706433
const u_short SourcePort = 1024;
const u_short RouterPort = 1025;
const u_short DestPort = 1026;
WSADATA wsadata;	//初始化dll
//数据头
struct header {
	// u_short 2字节(2B) 对应16位(16 bit)无符号比特
	//u_short source_ip;		//源ip 16位 本来是32位，127.0.0.1 中间都是0，将其省略，理论上应该是16+16位的
	//u_short dest_ip;		//目的ip
	u_short ack;			//0或1
	u_short seq;			//0或1
	u_short flag;			//bigend 0位SYN，1位ACK，2位FIN
	u_short source_port;	//源端口
	u_short dest_port;		//目的端口
	u_short length;			//消息长度
	u_short checksum;		//校验和

	header()
	{
		//source_ip = SourceIp;
		//dest_ip = DestIp;
		source_port = SourcePort;
		dest_port = DestPort;
		ack = seq = flag = length = checksum = 0;
	}
};

// 功能函数
void establish_connection();		// 三次握手建立连接
void receive_MSG();					// 读入要传输的文件并发送
void break_connection();			// 四次挥手断开连接
u_short checksum(u_short*, size_t);	// 计算校验和 

SOCKADDR_IN server_addr;//服务端
SOCKADDR_IN router_addr;//路由器
SOCKADDR_IN client_addr;//客户端
SOCKET s;				//套接字
//int serverlen = sizeof(server_addr);
int routerlen = sizeof(router_addr);
int main()
{
	//初始化工作
	WSAStartup(MAKEWORD(2, 2), &wsadata);
	//目的端：server 源端：client
	//服务端
	server_addr.sin_addr.s_addr = htonl(ip_32); //目的端ip
	server_addr.sin_port = htons(DestPort);		//目的端口号
	server_addr.sin_family = AF_INET;//IPV4
	//路由器
	router_addr.sin_addr.s_addr = htonl(ip_32); //router ip
	router_addr.sin_port = htons(RouterPort);	//router的端口号
	router_addr.sin_family = AF_INET;
	//客户端
	client_addr.sin_addr.s_addr = htonl(ip_32);	//源端ip
	client_addr.sin_port = htons(SourcePort);	//源端口号
	client_addr.sin_family = AF_INET;

	//绑定套接字
	s = socket(AF_INET, SOCK_DGRAM, 0);
	bind(s, (SOCKADDR*)&server_addr, sizeof(server_addr));

	establish_connection();
	receive_MSG();
	break_connection();
}

//建立三次握手连接
//#define SYN 0x1;
//#define ACK 0x2;
//#define SYN_ACK 0x3;
const u_short SYN = 0x1;
const u_short ACK = 0x2;
const u_short SYN_ACK = 0x3;
const double max_waitingtime = 0.1 * CLOCKS_PER_SEC;
u_long non_block = 1;
u_long block = 0;
const int maxsize = 100000000;
//u_short 2字节 16bit
//函数参数size为字节数
u_short checksum(u_short* mes, int size)
{
	// 分配所需的内存空间，返回一个指向它的指针，size+1为内存块的大小，以字节为单位
	size_t size_need;
	if (size % 2 == 0)//2B 16it的倍数
		size_need = size;
	else
		size_need = size + 1;

	int count = size_need / 2; //size向上取整
	u_short* buf = (u_short*)malloc(size_need);//开辟的空间
	memset(buf, 0, size_need); //开的buff 将数据报用0补齐为16的整数倍
	memcpy(buf, mes, size);	   //发送的buf 不要发送补的0
	u_long sum = 0;
	//按二进制反码运算求和
	while (count--)
	{
		if (count == 0)
			sum += *buf;
		else
			sum += *buf++;
		if (sum & 0xffff0000)//实际就是看有没有进位
		{
			sum &= 0xffff;//取低16位
			sum++;//进位加到最后
		}
	}

	//产生的伪首部,后期可能会写一个struct用来封装伪首部
	sum += SourceIp;
	if (sum & 0xffff0000)//实际就是看有没有进位
	{
		sum &= 0xffff;//取低16位
		sum++;//进位加到最后
	}
	sum += DestIp;
	if (sum & 0xffff0000)//实际就是看有没有进位
	{
		sum &= 0xffff;//取低16位
		sum++;//进位加到最后
	}

	//得到的结果求反码得到校验和
	return ~(sum & 0xffff);
}

// 建立连接
void establish_connection()
{
	cout << "[系统]: 服务端开启，等待连接客户端\n" << endl;
	header e_header;
	int header_size = sizeof(e_header);
	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	int result = 0;
	bool over_time_flag = false;

	while (true)
	{
		//阻塞接收消息
		result = recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen);
		if (result == -1)
		{
			cout << "[握手请求]:   [1] 第一次握手接收失败" << endl;
			exit(-1);
		}
		memcpy(&e_header, recbuff, header_size);
		cout << "[计算校验和]: [1] " << checksum((u_short*)&e_header, header_size) << endl;
		if (checksum((u_short*)&e_header, header_size) == 0 && e_header.flag == SYN)
		{
			cout << "[握手请求]:   [1] 成功接收第一次握手请求" << endl;
			break;
		}
		else
		{
			cout << "[握手请求]:   [1] 失败，请求数据包错误" << endl;
		}
	}

	//ACK :置1时该报文段为确认报文段
	//ack :而ack则为TCP报文段首部中“确认号字段”的具体数值。
	//ack=x+1说明B希望A下次发来的报文段的第一个数据字节为序号=x+1的字节；
	//ack=y+1说明A希望B下次发来的报文段的第一个数据字节为序号=y+1的字节。
	e_header.flag = SYN_ACK;
	e_header.ack = (e_header.seq + 1) % 2;//我们这里的seq是0/1
	e_header.seq = 0;
	e_header.length = 0;
	e_header.checksum = 0;
	e_header.checksum = checksum((u_short*)(&e_header), header_size);
	//所以可以不设置非阻塞通信
	//如果按照可能丢失设计呢？
	ioctlsocket(s, FIONBIO, &non_block); //设置非阻塞
	clock_t start;
	clock_t end;
	memcpy(sendbuff, &e_header, header_size);
	while (true)
	{
		result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
		if (result == -1)
		{
			cout << "[握手请求]:   [2] 第二次握手请求发送失败" << endl;
			exit(-1);
		}
		else
			cout << "[握手请求]:   [2] 第二次握手请求发送成功" << endl;
		start = end = clock();
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			end = clock();
			if (end - start > max_waitingtime)
			{
				result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
				if (result == -1)
				{
					cout << "[握手请求]:   [2] 第二次握手请求重新发送失败" << endl;
					exit(-1);
				}
				else
					cout << "[握手请求]:   [2] 第二次握手请求重新发送成功" << endl;
				start = end = clock();
			}
		}

		memcpy(&e_header, recbuff, header_size);
		cout << "[计算校验和]: [3] " << checksum((u_short*)&e_header, header_size) << endl;
		if (checksum((u_short*)&e_header, header_size) == 0 && e_header.flag == ACK)
		{
			cout << "[握手请求]:   [3] 第三次握手请求接收成功" << endl;
			break;
		}
		else
		{
			cout << "[握手请求]:   [3] 第三次握手请求接收错误的数据包，重新传输" << endl;
		}
	}

	//cout << "[握手请求]: [3] 成功 第三次握手成功" << endl;
	cout << "\n[建立连接]: Congratulations!" << endl;
	cout << "[建立连接]: 服务端与客户端成功建立连接！\n" << endl;
}

const int maxsize_total = 100000000;//文件总的字节数
const int maxsize_data = 1024;		//贷款
const unsigned char OVER = 0x8;//OVER=1,FIN=0,ACK=0,SYN=0
const unsigned char OVER_ACK = 0xA;//OVER=1,FIN=0,ACK=1,SYN=0
char* message = new char[maxsize_total];//存储所有收到的数据

void savefile(string name, int len)
{
	string filename = name;
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < len; i++)
	{
		fout << message[i];
	}
	fout.close();
	cout << "[文件保存]:   文件已成功保存到本地\n" << endl;
}

//接收文件
void receive_MSG()
{
	//文件存储的地址
	cout << "请输入存储地址:如:1.jpg" << endl;
	string a;
	cin >> a;

	//数据头
	header e_header;
	//数据头字节数
	int header_size = sizeof(e_header);
	//接收缓冲区 包含头部和数据
	char* recbuff = new char[header_size + maxsize_data];
	//发送缓冲区 仅包含头部
	char* sendbuff = new char[header_size];
	//期待接收的seq
	int hope_seq = 0;
	//发送的ack
	int send_ack = 0;
	//在总的存储message数组中的偏移
	int offset = 0;
	//设置为阻塞接收
	ioctlsocket(s, FIONBIO, &block);
	//开始准备接收
	while (true)
	{
		//等待接收数据包
		while (recvfrom(s, recbuff, header_size + maxsize_data, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			cout << "[接收数据]:   连接有误，继续等待" << endl;
		}
		//接收到了数据包,将数据包头部存入e_header
		memcpy(&e_header, recbuff, header_size);
		//判断数据包是否正确（1.接收的数据为期待的seq，2.校验和为0）
		if (e_header.seq == hope_seq && checksum((u_short*)recbuff, header_size + e_header.length) == 0)
		{
			//将接收到的数据区域存入总的存储数组message中
			memcpy(message + offset, recbuff + header_size, e_header.length);
			offset += e_header.length;
			cout << "[计算校验和]: " << 0 << endl;
			cout << "[数据接收]:   期待的 [" << e_header.seq << "]号数据包 " << endl;
			cout << "[数据长度]:   " << e_header.length << endl;
			//如果是最后一个包，对于最后一个发送的ack要打上OVER_ACK的标签
			if (e_header.flag == OVER)
				e_header.flag = OVER_ACK;

			//返回的ack为期待收到的seq号
			e_header.ack = hope_seq;
			//清空校验和重新计算并存入校验域
			e_header.checksum = 0;
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			//将要返回的数据头存入发送缓冲区
			memcpy(sendbuff, &e_header, header_size);
			//router原因，客户端一定会收到
			sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			//更新hope_seq和ack
			//debug!!!
			send_ack = hope_seq;//新收到了seq，更新累计确认ack
			hope_seq = hope_seq + 1;//下一次期待的seq

			if (e_header.flag == OVER_ACK)
			{
				cout << "[接收数据]:   成功接收文件!" << endl;
				break;
			}

		}
		//不正确，返回的是累计确认的ack，并重新等待
		else
		{
			cout << "[接收数据]:   接收 ["<<e_header.seq<<"] 的数据包 但期待的是 [" << hope_seq <<"]"<< endl;
			e_header.ack = send_ack;
			e_header.checksum = 0;
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			memcpy(sendbuff, &e_header, header_size);
			//router原因，客户端一定会收到
			sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			continue;
		}

	}
	//保存文件
	savefile(a, offset);

}
//由客户端发出关闭请求
const u_short FIN = 0x4;//OVER=0,FIN=1,ACK=0,SYN=0
const u_short FIN_ACK = 0x6;//OVER=0,FIN=1,ACK=1,SYN=0
const u_short FINAL_CHECK = 0x20;//FC=1,OVER=0,FIN=0,ACK=0,SYN=0
void break_connection()
{
	//现在还是阻塞接收
	header e_header;
	int header_size = sizeof(e_header);
	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	int result = 0;
	// 接收第一次挥手请求
	while (true)
	{
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			cout << "[接收数据]:   连接有误，继续等待" << endl;
		}
		//未超时接收，判断消息是否正确
		memcpy(&e_header, recbuff, header_size);
		if ((e_header.flag == FIN) && checksum((u_short*)&e_header, header_size) == 0)
		{
			cout << "[挥手请求]:   [1] 成功接收第1次挥手请求" << endl;
			break;//接收到正确的信息并跳出循环
		}
		else
		{
			cout << "[挥手请求]:   [2] 失败 收到错误的数据包,重新接收" << endl;
			continue;//重传
		}
	}

	//接收第一次挥手请求 回复两次
	e_header.ack = 0;
	e_header.seq = 0;
	e_header.flag = FIN_ACK;
	e_header.length = 0;//请求建立连接length=0
	//校验和域段清零
	e_header.checksum = 0;
	//计算校验和并存入响应域段

	cout << "[计算校验和]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	memcpy(sendbuff, &e_header, header_size);
	result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	if (result == -1)
	{
		cout << "[挥手请求]:   [2] 发送数据包 失败 具体原因: 连接错误" << endl;
		exit(-1);
	}
	else
		cout << "[挥手请求]:   [2] 发送数据包 成功" << endl;

	result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	if (result == -1)
	{
		cout << "[挥手请求]:   [3] 发送数据包 失败 具体原因: 连接错误" << endl;
		exit(-1);
	}
	else
		cout << "[挥手请求]:   [3] 发送数据包 成功" << endl;

	while (true)
	{
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			cout << "[接收数据]:   连接有误，继续等待" << endl;
		}
		//未超时接收，判断消息是否正确
		memcpy(&e_header, recbuff, header_size);
		if ((e_header.flag == ACK) && checksum((u_short*)&e_header, header_size) == 0)
		{
			cout << "[挥手请求]:   [4] 接收数据包 成功" << endl;
			break;//接收到正确的信息并跳出循环
		}
		else
		{
			cout << "[挥手请求]:   [4] 接收数据包 失败" << endl;
			continue;//
		}
	}

	e_header.flag = FINAL_CHECK;
	e_header.checksum = 0;
	cout << "[计算校验和]: [5] " << checksum((u_short*)&e_header, header_size) << endl;
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	memcpy(sendbuff, &e_header, header_size);
	result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	if (result == -1)
	{
		cout << "[挥手请求]:   [5] 第4次挥手确认的ACK返回失败" << endl;
		exit(-1);
	}
	else
		cout << "[挥手请求]:   [5] 第4次挥手确认的ACK返回成功" << endl;
}