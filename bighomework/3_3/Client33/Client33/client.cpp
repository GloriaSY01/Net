//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
using namespace std;
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<string>
#include<pthread.h>
#pragma comment(lib,"ws2_32.lib")

/*
	UDP协议
	rdt 3.0
	停等
*/

const u_short IP = 0x7f01; //127.0.0.1 数据头存的都是16位，中间的0省略了
const u_short SourceIp = IP;
const u_short RouterIp = IP;
const u_short DestIp = IP;
int ip_32 = 2130706433;   //127.0.0.1->0111 1111 0000 0000 0000 0000 0000 0001->2130706433
const u_short SourcePort = 1024;
const u_short RouterPort = 1025;
const u_short DestPort = 1026;
WSADATA wsadata;	      //初始化dll
//数据头
struct header {
	// u_short 2字节(2B) 对应16位(16 bit)无符号比特
	u_short ack;			//0或1
	u_short seq;			//0或1
	u_short flag;			//bigend 0位SYN，1位ACK，2位FIN
	u_short source_port;	//源端口
	u_short dest_port;		//目的端口
	u_short length;			//消息长度
	u_short checksum;		//校验和
	int start;//起始的地方
	header()
	{
		start = 0;
		source_port = SourcePort;
		dest_port = DestPort;
		ack = seq = flag = length = checksum = 0;
	}
};

// 功能函数
void establish_connection();		// 三次握手建立连接
void read_and_send_file();			// 读入要传输的文件并发送
void break_connection();			// 四次挥手断开连接
u_short checksum(u_short*, size_t);	// 计算校验和 

SOCKADDR_IN server_addr;//服务端
SOCKADDR_IN router_addr;//路由器
SOCKADDR_IN client_addr;//客户端
SOCKET s;				//套接字
int routerlen = sizeof(router_addr);

int main()
{
	//初始化工作
	WSAStartup(MAKEWORD(2, 2), &wsadata);
	//目的端：server 源端：client
	//服务端
	server_addr.sin_addr.s_addr = htonl(ip_32); //目的端ip
	server_addr.sin_port = htons(DestPort);	  //目的端口号
	server_addr.sin_family = AF_INET;			  //IPV4
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
	bind(s, (SOCKADDR*)&client_addr, sizeof(client_addr));

	establish_connection();
	read_and_send_file();
	break_connection();
}

//建立三次握手连接
const u_short SYN = 0x1;
const u_short ACK = 0x2;
const u_short SYN_ACK = 0x3;
const int max_waitingtime =  CLOCKS_PER_SEC;
u_long non_block = 1;
u_long block = 0;

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
	::memset(buf, 0, size_need); //开的buff 将数据报用0补齐为16的整数倍
	::memcpy(buf, mes, size);	   //发送的buf 不要发送补的0
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

	//产生的伪首部,后期可能会写一个struct创建伪首部
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
	header e_header;
	int header_size = sizeof(e_header);
	e_header.ack = 0;
	e_header.seq = 0;
	e_header.flag = SYN;
	e_header.length = 0;//请求建立连接length=0
	//校验和域段清零
	e_header.checksum = 0;
	//计算校验和并存入响应域段
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	::cout << "[握手请求]:   [1] 发送数据包 校验和 ：" << e_header.checksum << endl;

	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	::memcpy(sendbuff, &e_header, header_size);

	int result = 0;
	bool over_time_flag = false;
	//第一次和第二次握手
	while (true)
	{
		over_time_flag = false;//未超时
		//第一次握手 发送SYN
		result = sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
		if (result == -1)
		{
			::cout << "[握手请求]:   [1] 发送数据包 失败" << endl;
			exit(-1);//退出程序
		}
		else
			::cout << "[握手请求]:   [1] 发送数据包 成功" << endl;

		ioctlsocket(s, FIONBIO, &non_block); //设置非阻塞
		clock_t start; //开始计时器
		clock_t end;   //结束计时器	
		start = clock();
		//连接正常关闭返回0，错误返回-1
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			// 判断超时与否
			end = clock();
			if ((end - start) > max_waitingtime)
			{
				over_time_flag = true; //超时设置flag为1
				break;
			}
		}

		//超时重传
		if (over_time_flag)
		{
			::cout << "[握手请求]:   [1] 发送数据包 失败 具体原因: 反馈超时，重新发送" << endl;
			continue;
		}

		//未超时接收，判断消息是否正确
		::memcpy(&e_header, recbuff, header_size);
		::cout << "[ACK]:        [2] " << e_header.ack << endl;
		::cout << "[计算校验和]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
		if ((e_header.flag == SYN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
		{
			::cout << "[握手请求]:   [2] 接收数据包 成功" << endl;
			break;//接收到正确的信息并跳出循环
		}
		else
		{
			::cout << "[握手请求]:   [2] 接收数据包 失败 具体原因: 收到错误的数据包，重新发送" << endl;
			continue;//重传
		}
	}

	//第三次握手 发送ack
	e_header.ack = 1;
	e_header.seq = 1;
	e_header.flag = ACK;
	e_header.checksum = 0;
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	::memcpy(sendbuff, &e_header, header_size);
	//第三次握手
	clock_t start, end;
	sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	start = clock();
	end = clock();
	while (end - start <= 2 * max_waitingtime)
	{
		result = recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen);
		if (result <= 0)
		{
			end = clock();
			continue;//没收到，继续等待
		}
		//收到了，理论分析应该是对方没收到第三次的ACK，故重发了第二次挥手，才会收到
		::cout << "[握手请求]:   [3] 发送数据包 失败 重新发送" << endl;
		sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
		start = end = clock();
	}
	::cout << "[握手请求]:   [3] 发送数据包 成功" << endl;
	::cout << "\n[建立连接]: Congratulations! 三次握手成功！" << endl;
	::cout << "[建立连接]: 客户端与服务端成功建立连接！\n" << endl;
}

const int maxsize_total = 100000000;//文件总的字节数
const int MSS = 1024;		//一条数据的最大数
const u_short OVER = 0x8;//OVER=1,FIN=0,ACK=0,SYN=0
const u_short OVER_ACK = 0xA;//OVER=1,FIN=0,ACK=1,SYN=0
clock_t time_file;//总的时间

// 全局变量的修改
int window_len = 8;//窗口的个数
int window_size = window_len * MSS;//窗口能够容纳的最大字节数
long long int cwnd = MSS;//初始时化cwnd大小
long long int ssthresh = 6*MSS;//阈值大小
long long int LastByteAcked = -1;
long long int LastByteSent = -1;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
int dupACKcount = 0;//接收重复ACK的次数
enum {slow_start,avoid_congestion,quick_recovery};//三个状态
int state = slow_start;//当前状态

//int seq_total = 0;// 总的seq号
int current_seq = 0;//当前需要发送的seq
int current_hope_ack = 0;//当前期待收到的ack
char* sendbuff_data;//发送的包,包含了数据
char* recbuff_data;//收到的包,只需要接收数据头
const int record_num = 1000000;
clock_t recordtime[record_num];//每个包发送时的开始时间
bool rt_invalid[record_num];//每个包是否发送
long long int packnum = 0;//总共要发送的数据包个数
long long int len_of_wholeMSG = 0;//整个文件的长度
//bool istimeout = false;//是否超时
bool end1 = false;
string in_re_ack;
string in_send;
//线程：判断超时
DWORD WINAPI timeOut(LPVOID param)
{
	while (true)
	{
		//退出线程
		//if (LastByteAcked == len_of_wholeMSG - 1)
		if(end1)
		{
			::cout << "\nexit timeout" << endl;
			break;
		}

		////判断超时：当前时间 - 期待收到的ack对应的seq数据包发送的时间 且 当前包已经发送了 debug
		//if ((clock() - recordtime[current_hope_ack % record_num] > max_waitingtime))
		//{
		//	std::cout << "\n[ACK超时]:      超时未收到期待的ack [" << current_hope_ack << "] 重新传输" << endl;
		//	// 1. 状态超时都会回到慢启动状态
		//	// 慢启动状态超时
		//	if (state == slow_start)
		//	{
		//		ssthresh = cwnd / 2;
		//		cwnd = MSS;
		//		dupACKcount = 0;
		//		::cout << "\n[超时]        仍处于慢启动" << endl;
		//	}
		//	// 拥塞避免状态超时
		//	else if (state == avoid_congestion)
		//	{
		//		ssthresh = cwnd / 2;
		//		cwnd = MSS;
		//		dupACKcount = 0;
		//		::cout << "\n[超时状态切换] 拥塞避免->慢启动" << endl;
		//		state = slow_start;
		//	}
		//	// 快速恢复状态超时
		//	else if (state == quick_recovery)
		//	{
		//		ssthresh = cwnd / 2;
		//		cwnd = MSS;
		//		dupACKcount = 0;
		//		::cout << "\n[超时状态切换] 快速回复->慢启动" << endl;
		//		state = slow_start;
		//	}
		//	LastByteSent = LastByteAcked;//丢失的字节开始发
		//	current_seq = current_hope_ack;
		//}	
	}
	return 0;
}

//线程：接收ACK
DWORD WINAPI receiveACK(LPVOID param)
{
	SOCKET* clientSock = (SOCKET*)param;
	header e_header;//数据头
	int header_size = sizeof(e_header);//数据头字节数
	ioctlsocket(s, FIONBIO, &block); //设置为阻塞接收ack
	while (true)
	{
		//接收ack
		if (recvfrom(s, recbuff_data, header_size, 0, (sockaddr*)&router_addr, &routerlen) > 0)
		{
			::memcpy(&e_header, recbuff_data, header_size);
			//判断是否为期待接收的ack
			if (e_header.ack == current_hope_ack && checksum((u_short*)&e_header, header_size) == 0)
			{
				in_re_ack = "\n[期待ACK]" + to_string(current_hope_ack)+"\n[接收ACK]"+ to_string(e_header.ack);
				::cout << in_re_ack << endl;
				//如果接收到了最后一个数据包的ack
				if (e_header.flag == OVER_ACK)
				{
					::cout << "\n[接收ACK]:      收到结束OVER_ACK" << endl;
					end1 = true;
					current_hope_ack++;
					LastByteAcked += e_header.length;
					break;//停止接收
				}
				//期待接收下一个数据包的ack
				current_hope_ack++;
				LastByteAcked += e_header.length;

				// 1. 接收到NEW ACK
				//cout << "state" << state << endl;
				if (state == slow_start)
				{
					cwnd = cwnd + MSS;
					dupACKcount = 0;
					::cout << "\nslow start new ack" << endl;
					if (cwnd >= ssthresh)
					{
						state = avoid_congestion;
						::cout << "\n[NEW ACK状态切换]: 慢启动->拥塞避免 "<< endl;
					}
						
				}
				else if (state == avoid_congestion)
				{
					::cout << "\navoid congestion new ack" << endl;
					//::cout << "avoid congestion" << endl;
					//cwnd = cwnd + maxsize_data*(maxsize_data/(maxsize_data*cwnd));
					cwnd = cwnd + MSS * (MSS*1.0 / cwnd);
					dupACKcount = 0;
				}
				else if (state == quick_recovery)
				{
					::cout << "\nquik recovery new ack" << endl;
					cwnd = ssthresh;
					dupACKcount = 0;
					::cout << "\n[NEW ACK状态切换]: 快速恢复->拥塞避免" << endl;
					state = avoid_congestion;
				}
			}
			else
			{
				in_re_ack = "[期待ACK]" + to_string(current_hope_ack) + "\n[重复ACK]" + to_string(e_header.ack);
				//::cout << in_re_ack << endl;
				//in_re_ack = "[期待ACK]" + to_string(current_hope_ack);
				//::cout << in_re_ack << endl;
				//in_re_ack = "[重复ACK]" + to_string(e_header.ack);
				::cout << in_re_ack << endl;
				
				// 2. 接收到重复 ACK
				if (state == slow_start)
				{
					dupACKcount++;
					if (dupACKcount == 3)
					{
						ssthresh = cwnd / 2;
						cwnd = ssthresh + 3 * MSS;
						state = quick_recovery;
						::cout << "\n[重复ACK 状态切换]: 慢启动->快速恢复" << endl;
					}
				}
				else if (state == avoid_congestion)
				{
					dupACKcount++;
					if (dupACKcount == 3)
					{
						ssthresh = cwnd / 2;
						cwnd = ssthresh + 3 * MSS;
						state = quick_recovery;
						::cout << "\n[重复ACK 状态切换]: 拥塞避免->快速恢复" << endl;
					}
				}
				else if (state == quick_recovery)
				{
					cwnd = cwnd + MSS;
					::cout << "\n[重复ACK 状态切换]: 仍处于快速恢复" << endl;
				}
				continue;
			}
		}

	}
	return 0;
}

//线程三：发送数据包
void read_and_send_file()
{
	//加载文件
	string file_name;
	::cout << "[文件传输]: ACTION" << endl;
	::cout << "[文件读入]: 输入要传输文件的绝对路径" << endl;
	cin >> file_name;
	ifstream fin(file_name.c_str(), ios::in | ios::binary);
	if (!fin.is_open())
	{
		::cout << "[文件打开]: 失败！" << endl;
		exit(-1);
	}
	char* message = new char[maxsize_total];
	char c;

	unsigned char temp = fin.get();
	while (fin)
	{
		message[len_of_wholeMSG++] = temp;
		temp = fin.get();
	}
	fin.close();
	::cout << "[文件加载]: 加载成功" << endl;

	header e_header;//数据头
	int header_size = sizeof(e_header);//数据头字节数
	sendbuff_data = new char[header_size + MSS];//发送缓冲区
	recbuff_data = new char[header_size];//接收缓冲区
	current_seq = 0;//代表需要发送的seq
	packnum = len_of_wholeMSG / MSS;//一共要发送的包的个数，序列号总大小
	if (len_of_wholeMSG % MSS != 0)
		packnum += 1;
	packnum = packnum - 1;//0,1,……,n-1
	int countpacknum = 0;//用来记录发了几个包
	int result = 0;//用来记录数据包发送与接收正确与否
	int thislen = 0;//数据部分的字节大小
	ioctlsocket(s, FIONBIO, &non_block); //设置非阻塞

	::cout << "数据包总个数 " << packnum << endl;

	//开启超时和接收ack线程
	HANDLE receiveack = CreateThread(nullptr, 0, receiveACK, LPVOID(&s), 0, nullptr);
	HANDLE timeout = CreateThread(nullptr, 0, timeOut, nullptr, 0, nullptr);
	
	::memset(recordtime, clock(), record_num);//初始化计时数组
	::memset(rt_invalid, false, record_num);//包都没发送
	long long int current_window;//窗口大小，取cwnd和max_window的最小值
	time_file = clock();//总的时间大小
	while (true)
	{
		//cout << "statehh" << state << endl;
		//接收完毕
		//if (LastByteAcked == len_of_wholeMSG - 1)
		if (end1)
		{
			CloseHandle(receiveack);
			CloseHandle(timeout);
			double time_record = (clock() - time_file) / 1000.0;
			::cout << "\n[传输数据]:     文件传输完毕！\n" << endl;
			::cout << "[传输时间]:       " << time_record << " s" << endl;
			::cout << "[文件大小]:       " << len_of_wholeMSG * 8 << " bit" << endl;
			::cout << "[数据头大小总和]: " << header_size * packnum * 8 << " bit" << endl;
			double throu = (double)(len_of_wholeMSG + packnum * header_size) * 8 / time_record;
			::cout << "[平均吞吐量]:     " << throu << "bit/s" << endl;
			::cout << endl;
			break;
		}
		//判断超时：当前时间 - 期待收到的ack对应的seq数据包发送的时间 且 当前包已经发送了 debug
		if ((clock() - recordtime[current_hope_ack % record_num] > max_waitingtime))
		{
			in_send = "\n[ACK超时]:      超时未收到期待的ack [" + to_string(current_hope_ack) + "] 重新传输";
			//std::cout << "\n[ACK超时]:      超时未收到期待的ack [" << current_hope_ack << "] 重新传输" << endl;
			std::cout << in_send << endl;
			// 3. 状态超时都会回到慢启动状态
			// 慢启动状态超时
			ssthresh = cwnd / 2;
			cwnd = MSS;
			dupACKcount = 0;
			if (state == slow_start)
			{
				::cout << "\n[超时]        仍处于慢启动" << endl;
			}
			// 拥塞避免状态超时
			else if (state == avoid_congestion)
			{
				state = slow_start;
				::cout << "\n[超时状态切换] 拥塞避免->慢启动" << endl;
			}
			// 快速恢复状态超时
			else if (state == quick_recovery)
			{
				state = slow_start;
				::cout << "\n[超时状态切换] 快速恢复->慢启动" << endl;
			}
			LastByteSent = LastByteAcked;//丢失的字节开始发
			current_seq = current_hope_ack;
		}
		current_window = min(cwnd,window_size);
		//if (((LastByteSent - LastByteAcked) < current_window) && (LastByteAcked < (len_of_wholeMSG - 1)))
		if (((LastByteSent - LastByteAcked) < current_window) && (LastByteSent < (len_of_wholeMSG - 1)))
		{		
			//剩余窗口的字节大小

			in_send = "\n[窗口大小]:    " + to_string(current_window) + "\n[剩余窗口大小]: " + to_string(current_window - (LastByteSent - LastByteAcked));
			int left = current_window - (LastByteSent - LastByteAcked);
			::cout << in_send << endl;
			//int temp = current_window;
			//::cout << "\n[窗口大小]:    " << temp << endl;
			//::cout << "\n[剩余窗口大小]: " << left << endl;
			//制作数据包
			::memset(sendbuff_data, 0, header_size + MSS);
			//处理数据头
			e_header.flag = 0;
			e_header.ack = 0;
			//debug!!!
			if (current_seq < current_hope_ack)
			{
				current_seq = current_hope_ack;
			}
			e_header.seq = current_seq;
			
			//如果超过文件了
			if ((LastByteAcked + current_window) > (len_of_wholeMSG - 1))
				left = (len_of_wholeMSG - 1) - LastByteSent;

			//如果是发送最后一个数据包
			//if (left <= MSS && )
			//{
			//	e_header.length = thislen = (len_of_wholeMSG - 1) - LastByteSent;//最后一个数据包大小
			//	e_header.flag = OVER;//结束标志
			//}
			if (left / MSS > 0)
			{
				e_header.length = thislen = MSS;//最大数据包
			}
			else if (left / MSS == 0)
			{
				if (LastByteSent + left == len_of_wholeMSG - 1)
				{
					e_header.flag = OVER;//结束标志

				}
				//e_header.length = thislen = (LastByteAcked + cwnd) - LastByteSent;//剩余不满MSS的窗口
				e_header.length = thislen = left % MSS;
			}

			//将数据头放入发送缓冲区
			::memcpy(sendbuff_data, &e_header, header_size);
			//将数据填入发送缓冲区
			//int len = LastByteSent;
			//pthread_mutex_lock(&counter_mutex);
			e_header.start = LastByteSent + 1;
			::memcpy(sendbuff_data + header_size, message + LastByteSent+1, thislen);
			e_header.checksum = 0;
			::memcpy(sendbuff_data, &e_header, header_size);
			//计算校验和
			e_header.checksum = checksum((u_short*)sendbuff_data, header_size + thislen);
			//重新填充头部
			::memcpy(sendbuff_data, &e_header, header_size);
			result = sendto(s, sendbuff_data, header_size + thislen, 0, (sockaddr*)&router_addr, routerlen);
			//每发送一个包就要记录发送的时间，这里需要说明 a和a+window_len两个数据是不会同时在发送中的
			//故record_time数组可以做到取模重复利用
			//recordtime[current_seq % window_len] = clock();
				
			recordtime[current_seq % record_num] = clock();
			rt_invalid[current_seq % record_num] = true;
			if (result == -1)
			{
				::cout << "\n[传输数据包]:   [" << current_seq << "]号 发送失败" << endl;
				exit(-1);
			}
			LastByteSent += thislen;
			//pthread_mutex_unlock(&counter_mutex);
			in_send = "\n[传输数据包]:   [" + to_string(current_seq) + "]号 发送成功";
			//::cout << "\n[传输数据包]:   [" << current_seq << "]号 发送成功" << endl;
			::cout << in_send << endl;
			current_seq++;
		}
	}

}

//由客户端发出关闭请求
const u_short FIN = 0x4;//OVER=0,FIN=1,ACK=0,SYN=0
const u_short FIN_ACK = 0x6;//OVER=0,FIN=1,ACK=1,SYN=0
const u_short FINAL_CHECK = 0x20;//FC=1,OVER=0,FIN=0,ACK=0,SYN=0
void break_connection()
{
	header e_header;
	int header_size = sizeof(e_header);
	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	int send_turn = 1;//发送2次
	//int receive_turn = 2;//接收2次
	bool over_time_flag = false;
	int result = 0;
	while (true)
	{
		//准备数据包

		//第一次发送
		if (send_turn == 1)
		{
			e_header.ack = 0;
			e_header.seq = 0;
			e_header.flag = FIN;
			e_header.length = 0;//请求建立连接length=0
			//校验和域段清零
			e_header.checksum = 0;
			//计算校验和并存入响应域段
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			::cout << "[挥手请求]:   [1] 发送的数据包校验和为：" << e_header.checksum << endl;
		}
		//第四次挥手
		else if (send_turn == 4)
		{
			e_header.ack = 0;
			e_header.seq = 1;
			e_header.flag = ACK;
			e_header.length = 0;//请求建立连接length=0
			//校验和域段清零
			e_header.checksum = 0;
			//计算校验和并存入响应域段
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			::cout << "[挥手请求]:   [4] 发送的数据包校验和为：" << e_header.checksum << endl;
		}
		::memcpy(sendbuff, &e_header, header_size);

		//发送并等待接收

		while (true)
		{
			over_time_flag = false;//未超时
			//第一次握手 发送SYN
			result = sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			if (result == -1)
			{
				::cout << "[挥手请求]:   [" << send_turn << "] 第" << send_turn << "次挥手消息发送失败" << endl;
				exit(-1);//退出程序
			}
			else
				::cout << "[挥手请求]:   [" << send_turn << "] 第" << send_turn << "次挥手消息发送成功" << endl;

			ioctlsocket(s, FIONBIO, &non_block); //设置非阻塞
			clock_t start; //开始计时器
			clock_t end;   //结束计时器	

			//连接正常关闭返回0，错误返回-1

			if (send_turn == 1)
			{
				//第一次接收
				start = clock();
				over_time_flag = false;
				while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
				{
					// 判断超时与否
					end = clock();
					/// <summary>
					/// this ！！！ 555
					/// </summary>
					if ((end - start) > 10 * max_waitingtime)
					{
						over_time_flag = true; //超时设置flag为1
						break;
					}
				}

				//超时重传
				if (over_time_flag)
				{
					::cout << "[挥手请求]:   [" << send_turn << "] 失败 反馈超时，重新发送" << endl;
					continue;
				}

				//未超时接收，判断消息是否正确
				::memcpy(&e_header, recbuff, header_size);
				::cout << "[计算校验和]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
				if ((e_header.flag == FIN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
				{
					::cout << "[挥手请求]:   [2] 接收数据包 成功" << endl;
					//break;//接收到正确的信息并跳出循环
				}
				else
				{
					::cout << "[挥手请求]:   [2] 接收数据包 失败 具体原因: 收到错误的数据包" << endl;
					continue;//重传
				}

				//第二次接收
				start = clock();
				over_time_flag = false;
				while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
				{
					// 判断超时与否
					end = clock();
					if ((end - start) > 10 * max_waitingtime)
					{
						over_time_flag = true; //超时设置flag为1
						break;
					}
				}

				//超时重传
				if (over_time_flag)
				{
					::cout << "[挥手请求]:   [" << send_turn << "] 失败 反馈超时，重新发送" << endl;
					continue;
				}

				//未超时接收，判断消息是否正确
				::memcpy(&e_header, recbuff, header_size);
				::cout << "[计算校验和]: [3] " << checksum((u_short*)&e_header, header_size) << endl;
				if ((e_header.flag == FIN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
				{
					::cout << "[挥手请求]:   [3] 接收数据包 成功" << endl;
					break;//接收到正确的信息并跳出循环
				}
				else
				{
					::cout << "[挥手请求]:   [3] 接收数据包 失败 具体原因: 收到错误的数据包" << endl;
					continue;//重传
				}

			}
			else if (send_turn == 4)
			{
				start = clock();
				over_time_flag = false;
				while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
				{
					// 判断超时与否
					end = clock();
					if ((end - start) > 10 * max_waitingtime)
					{
						over_time_flag = true; //超时设置flag为1
						break;
					}
				}

				//超时重传
				if (over_time_flag)
				{
					::cout << "[握手请求]:   [" << send_turn << "] 失败 反馈超时，重新发送" << endl;
					continue;
				}

				//未超时接收，判断消息是否正确
				::memcpy(&e_header, recbuff, header_size);
				::cout << "[计算校验和]: [4] " << checksum((u_short*)&e_header, header_size) << endl;
				if (e_header.flag == FINAL_CHECK && checksum((u_short*)&e_header, header_size) == 0)
				{
					::cout << "[挥手请求]:   [4] 发送数据包 成功" << endl;
					break;//接收到正确的信息并跳出循环
				}
				else
				{
					::cout << "[挥手请求]:   [4] 发送数据包 失败 具体原因:收到错误的数据包" << endl;
					continue;//重传
				}
			}

		}
		if (send_turn == 1)
			send_turn += 3;
		else
			break;
	}
	::cout << "[四次挥手]:   成功！ 客户端发送关闭请求" << endl;
}