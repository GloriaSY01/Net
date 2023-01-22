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
	UDPЭ��
	rdt 3.0
	ͣ��
*/

const u_short IP = 0x7f01; //127.0.0.1 ����ͷ��Ķ���16λ���м��0ʡ����
const u_short SourceIp = IP;
const u_short RouterIp = IP;
const u_short DestIp = IP;
int ip_32 = 2130706433;   //127.0.0.1->0111 1111 0000 0000 0000 0000 0000 0001->2130706433
const u_short SourcePort = 1024;
const u_short RouterPort = 1025;
const u_short DestPort = 1026;
WSADATA wsadata;	      //��ʼ��dll
//����ͷ
struct header {
	// u_short 2�ֽ�(2B) ��Ӧ16λ(16 bit)�޷��ű���
	u_short ack;			//0��1
	u_short seq;			//0��1
	u_short flag;			//bigend 0λSYN��1λACK��2λFIN
	u_short source_port;	//Դ�˿�
	u_short dest_port;		//Ŀ�Ķ˿�
	u_short length;			//��Ϣ����
	u_short checksum;		//У���
	int start;//��ʼ�ĵط�
	header()
	{
		start = 0;
		source_port = SourcePort;
		dest_port = DestPort;
		ack = seq = flag = length = checksum = 0;
	}
};

// ���ܺ���
void establish_connection();		// �������ֽ�������
void read_and_send_file();			// ����Ҫ������ļ�������
void break_connection();			// �Ĵλ��ֶϿ�����
u_short checksum(u_short*, size_t);	// ����У��� 

SOCKADDR_IN server_addr;//�����
SOCKADDR_IN router_addr;//·����
SOCKADDR_IN client_addr;//�ͻ���
SOCKET s;				//�׽���
int routerlen = sizeof(router_addr);

int main()
{
	//��ʼ������
	WSAStartup(MAKEWORD(2, 2), &wsadata);
	//Ŀ�Ķˣ�server Դ�ˣ�client
	//�����
	server_addr.sin_addr.s_addr = htonl(ip_32); //Ŀ�Ķ�ip
	server_addr.sin_port = htons(DestPort);	  //Ŀ�Ķ˿ں�
	server_addr.sin_family = AF_INET;			  //IPV4
	//·����
	router_addr.sin_addr.s_addr = htonl(ip_32); //router ip
	router_addr.sin_port = htons(RouterPort);	//router�Ķ˿ں�
	router_addr.sin_family = AF_INET;
	//�ͻ���
	client_addr.sin_addr.s_addr = htonl(ip_32);	//Դ��ip
	client_addr.sin_port = htons(SourcePort);	//Դ�˿ں�
	client_addr.sin_family = AF_INET;

	//���׽���
	s = socket(AF_INET, SOCK_DGRAM, 0);
	bind(s, (SOCKADDR*)&client_addr, sizeof(client_addr));

	establish_connection();
	read_and_send_file();
	break_connection();
}

//����������������
const u_short SYN = 0x1;
const u_short ACK = 0x2;
const u_short SYN_ACK = 0x3;
const int max_waitingtime =  CLOCKS_PER_SEC;
u_long non_block = 1;
u_long block = 0;

//u_short 2�ֽ� 16bit
//��������sizeΪ�ֽ���
u_short checksum(u_short* mes, int size)
{
	// ����������ڴ�ռ䣬����һ��ָ������ָ�룬size+1Ϊ�ڴ��Ĵ�С�����ֽ�Ϊ��λ
	size_t size_need;
	if (size % 2 == 0)//2B 16it�ı���
		size_need = size;
	else
		size_need = size + 1;

	int count = size_need / 2; //size����ȡ��
	u_short* buf = (u_short*)malloc(size_need);//���ٵĿռ�
	::memset(buf, 0, size_need); //����buff �����ݱ���0����Ϊ16��������
	::memcpy(buf, mes, size);	   //���͵�buf ��Ҫ���Ͳ���0
	u_long sum = 0;
	//�������Ʒ����������
	while (count--)
	{
		if (count == 0)
			sum += *buf;
		else
			sum += *buf++;
		if (sum & 0xffff0000)//ʵ�ʾ��ǿ���û�н�λ
		{
			sum &= 0xffff;//ȡ��16λ
			sum++;//��λ�ӵ����
		}
	}

	//������α�ײ�,���ڿ��ܻ�дһ��struct����α�ײ�
	sum += SourceIp;
	if (sum & 0xffff0000)//ʵ�ʾ��ǿ���û�н�λ
	{
		sum &= 0xffff;//ȡ��16λ
		sum++;//��λ�ӵ����
	}
	sum += DestIp;
	if (sum & 0xffff0000)//ʵ�ʾ��ǿ���û�н�λ
	{
		sum &= 0xffff;//ȡ��16λ
		sum++;//��λ�ӵ����
	}

	//�õ��Ľ������õ�У���
	return ~(sum & 0xffff);
}

// ��������
void establish_connection()
{
	header e_header;
	int header_size = sizeof(e_header);
	e_header.ack = 0;
	e_header.seq = 0;
	e_header.flag = SYN;
	e_header.length = 0;//����������length=0
	//У����������
	e_header.checksum = 0;
	//����У��Ͳ�������Ӧ���
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	::cout << "[��������]:   [1] �������ݰ� У��� ��" << e_header.checksum << endl;

	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	::memcpy(sendbuff, &e_header, header_size);

	int result = 0;
	bool over_time_flag = false;
	//��һ�κ͵ڶ�������
	while (true)
	{
		over_time_flag = false;//δ��ʱ
		//��һ������ ����SYN
		result = sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
		if (result == -1)
		{
			::cout << "[��������]:   [1] �������ݰ� ʧ��" << endl;
			exit(-1);//�˳�����
		}
		else
			::cout << "[��������]:   [1] �������ݰ� �ɹ�" << endl;

		ioctlsocket(s, FIONBIO, &non_block); //���÷�����
		clock_t start; //��ʼ��ʱ��
		clock_t end;   //������ʱ��	
		start = clock();
		//���������رշ���0�����󷵻�-1
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			// �жϳ�ʱ���
			end = clock();
			if ((end - start) > max_waitingtime)
			{
				over_time_flag = true; //��ʱ����flagΪ1
				break;
			}
		}

		//��ʱ�ش�
		if (over_time_flag)
		{
			::cout << "[��������]:   [1] �������ݰ� ʧ�� ����ԭ��: ������ʱ�����·���" << endl;
			continue;
		}

		//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
		::memcpy(&e_header, recbuff, header_size);
		::cout << "[ACK]:        [2] " << e_header.ack << endl;
		::cout << "[����У���]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
		if ((e_header.flag == SYN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
		{
			::cout << "[��������]:   [2] �������ݰ� �ɹ�" << endl;
			break;//���յ���ȷ����Ϣ������ѭ��
		}
		else
		{
			::cout << "[��������]:   [2] �������ݰ� ʧ�� ����ԭ��: �յ���������ݰ������·���" << endl;
			continue;//�ش�
		}
	}

	//���������� ����ack
	e_header.ack = 1;
	e_header.seq = 1;
	e_header.flag = ACK;
	e_header.checksum = 0;
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	::memcpy(sendbuff, &e_header, header_size);
	//����������
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
			continue;//û�յ��������ȴ�
		}
		//�յ��ˣ����۷���Ӧ���ǶԷ�û�յ������ε�ACK�����ط��˵ڶ��λ��֣��Ż��յ�
		::cout << "[��������]:   [3] �������ݰ� ʧ�� ���·���" << endl;
		sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
		start = end = clock();
	}
	::cout << "[��������]:   [3] �������ݰ� �ɹ�" << endl;
	::cout << "\n[��������]: Congratulations! �������ֳɹ���" << endl;
	::cout << "[��������]: �ͻ��������˳ɹ��������ӣ�\n" << endl;
}

const int maxsize_total = 100000000;//�ļ��ܵ��ֽ���
const int MSS = 1024;		//һ�����ݵ������
const u_short OVER = 0x8;//OVER=1,FIN=0,ACK=0,SYN=0
const u_short OVER_ACK = 0xA;//OVER=1,FIN=0,ACK=1,SYN=0
clock_t time_file;//�ܵ�ʱ��

// ȫ�ֱ������޸�
int window_len = 8;//���ڵĸ���
int window_size = window_len * MSS;//�����ܹ����ɵ�����ֽ���
long long int cwnd = MSS;//��ʼʱ��cwnd��С
long long int ssthresh = 6*MSS;//��ֵ��С
long long int LastByteAcked = -1;
long long int LastByteSent = -1;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
int dupACKcount = 0;//�����ظ�ACK�Ĵ���
enum {slow_start,avoid_congestion,quick_recovery};//����״̬
int state = slow_start;//��ǰ״̬

//int seq_total = 0;// �ܵ�seq��
int current_seq = 0;//��ǰ��Ҫ���͵�seq
int current_hope_ack = 0;//��ǰ�ڴ��յ���ack
char* sendbuff_data;//���͵İ�,����������
char* recbuff_data;//�յ��İ�,ֻ��Ҫ��������ͷ
const int record_num = 1000000;
clock_t recordtime[record_num];//ÿ��������ʱ�Ŀ�ʼʱ��
bool rt_invalid[record_num];//ÿ�����Ƿ���
long long int packnum = 0;//�ܹ�Ҫ���͵����ݰ�����
long long int len_of_wholeMSG = 0;//�����ļ��ĳ���
//bool istimeout = false;//�Ƿ�ʱ
bool end1 = false;
string in_re_ack;
string in_send;
//�̣߳��жϳ�ʱ
DWORD WINAPI timeOut(LPVOID param)
{
	while (true)
	{
		//�˳��߳�
		//if (LastByteAcked == len_of_wholeMSG - 1)
		if(end1)
		{
			::cout << "\nexit timeout" << endl;
			break;
		}

		////�жϳ�ʱ����ǰʱ�� - �ڴ��յ���ack��Ӧ��seq���ݰ����͵�ʱ�� �� ��ǰ���Ѿ������� debug
		//if ((clock() - recordtime[current_hope_ack % record_num] > max_waitingtime))
		//{
		//	std::cout << "\n[ACK��ʱ]:      ��ʱδ�յ��ڴ���ack [" << current_hope_ack << "] ���´���" << endl;
		//	// 1. ״̬��ʱ����ص�������״̬
		//	// ������״̬��ʱ
		//	if (state == slow_start)
		//	{
		//		ssthresh = cwnd / 2;
		//		cwnd = MSS;
		//		dupACKcount = 0;
		//		::cout << "\n[��ʱ]        �Դ���������" << endl;
		//	}
		//	// ӵ������״̬��ʱ
		//	else if (state == avoid_congestion)
		//	{
		//		ssthresh = cwnd / 2;
		//		cwnd = MSS;
		//		dupACKcount = 0;
		//		::cout << "\n[��ʱ״̬�л�] ӵ������->������" << endl;
		//		state = slow_start;
		//	}
		//	// ���ٻָ�״̬��ʱ
		//	else if (state == quick_recovery)
		//	{
		//		ssthresh = cwnd / 2;
		//		cwnd = MSS;
		//		dupACKcount = 0;
		//		::cout << "\n[��ʱ״̬�л�] ���ٻظ�->������" << endl;
		//		state = slow_start;
		//	}
		//	LastByteSent = LastByteAcked;//��ʧ���ֽڿ�ʼ��
		//	current_seq = current_hope_ack;
		//}	
	}
	return 0;
}

//�̣߳�����ACK
DWORD WINAPI receiveACK(LPVOID param)
{
	SOCKET* clientSock = (SOCKET*)param;
	header e_header;//����ͷ
	int header_size = sizeof(e_header);//����ͷ�ֽ���
	ioctlsocket(s, FIONBIO, &block); //����Ϊ��������ack
	while (true)
	{
		//����ack
		if (recvfrom(s, recbuff_data, header_size, 0, (sockaddr*)&router_addr, &routerlen) > 0)
		{
			::memcpy(&e_header, recbuff_data, header_size);
			//�ж��Ƿ�Ϊ�ڴ����յ�ack
			if (e_header.ack == current_hope_ack && checksum((u_short*)&e_header, header_size) == 0)
			{
				in_re_ack = "\n[�ڴ�ACK]" + to_string(current_hope_ack)+"\n[����ACK]"+ to_string(e_header.ack);
				::cout << in_re_ack << endl;
				//������յ������һ�����ݰ���ack
				if (e_header.flag == OVER_ACK)
				{
					::cout << "\n[����ACK]:      �յ�����OVER_ACK" << endl;
					end1 = true;
					current_hope_ack++;
					LastByteAcked += e_header.length;
					break;//ֹͣ����
				}
				//�ڴ�������һ�����ݰ���ack
				current_hope_ack++;
				LastByteAcked += e_header.length;

				// 1. ���յ�NEW ACK
				//cout << "state" << state << endl;
				if (state == slow_start)
				{
					cwnd = cwnd + MSS;
					dupACKcount = 0;
					::cout << "\nslow start new ack" << endl;
					if (cwnd >= ssthresh)
					{
						state = avoid_congestion;
						::cout << "\n[NEW ACK״̬�л�]: ������->ӵ������ "<< endl;
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
					::cout << "\n[NEW ACK״̬�л�]: ���ٻָ�->ӵ������" << endl;
					state = avoid_congestion;
				}
			}
			else
			{
				in_re_ack = "[�ڴ�ACK]" + to_string(current_hope_ack) + "\n[�ظ�ACK]" + to_string(e_header.ack);
				//::cout << in_re_ack << endl;
				//in_re_ack = "[�ڴ�ACK]" + to_string(current_hope_ack);
				//::cout << in_re_ack << endl;
				//in_re_ack = "[�ظ�ACK]" + to_string(e_header.ack);
				::cout << in_re_ack << endl;
				
				// 2. ���յ��ظ� ACK
				if (state == slow_start)
				{
					dupACKcount++;
					if (dupACKcount == 3)
					{
						ssthresh = cwnd / 2;
						cwnd = ssthresh + 3 * MSS;
						state = quick_recovery;
						::cout << "\n[�ظ�ACK ״̬�л�]: ������->���ٻָ�" << endl;
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
						::cout << "\n[�ظ�ACK ״̬�л�]: ӵ������->���ٻָ�" << endl;
					}
				}
				else if (state == quick_recovery)
				{
					cwnd = cwnd + MSS;
					::cout << "\n[�ظ�ACK ״̬�л�]: �Դ��ڿ��ٻָ�" << endl;
				}
				continue;
			}
		}

	}
	return 0;
}

//�߳������������ݰ�
void read_and_send_file()
{
	//�����ļ�
	string file_name;
	::cout << "[�ļ�����]: ACTION" << endl;
	::cout << "[�ļ�����]: ����Ҫ�����ļ��ľ���·��" << endl;
	cin >> file_name;
	ifstream fin(file_name.c_str(), ios::in | ios::binary);
	if (!fin.is_open())
	{
		::cout << "[�ļ���]: ʧ�ܣ�" << endl;
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
	::cout << "[�ļ�����]: ���سɹ�" << endl;

	header e_header;//����ͷ
	int header_size = sizeof(e_header);//����ͷ�ֽ���
	sendbuff_data = new char[header_size + MSS];//���ͻ�����
	recbuff_data = new char[header_size];//���ջ�����
	current_seq = 0;//������Ҫ���͵�seq
	packnum = len_of_wholeMSG / MSS;//һ��Ҫ���͵İ��ĸ��������к��ܴ�С
	if (len_of_wholeMSG % MSS != 0)
		packnum += 1;
	packnum = packnum - 1;//0,1,����,n-1
	int countpacknum = 0;//������¼���˼�����
	int result = 0;//������¼���ݰ������������ȷ���
	int thislen = 0;//���ݲ��ֵ��ֽڴ�С
	ioctlsocket(s, FIONBIO, &non_block); //���÷�����

	::cout << "���ݰ��ܸ��� " << packnum << endl;

	//������ʱ�ͽ���ack�߳�
	HANDLE receiveack = CreateThread(nullptr, 0, receiveACK, LPVOID(&s), 0, nullptr);
	HANDLE timeout = CreateThread(nullptr, 0, timeOut, nullptr, 0, nullptr);
	
	::memset(recordtime, clock(), record_num);//��ʼ����ʱ����
	::memset(rt_invalid, false, record_num);//����û����
	long long int current_window;//���ڴ�С��ȡcwnd��max_window����Сֵ
	time_file = clock();//�ܵ�ʱ���С
	while (true)
	{
		//cout << "statehh" << state << endl;
		//�������
		//if (LastByteAcked == len_of_wholeMSG - 1)
		if (end1)
		{
			CloseHandle(receiveack);
			CloseHandle(timeout);
			double time_record = (clock() - time_file) / 1000.0;
			::cout << "\n[��������]:     �ļ�������ϣ�\n" << endl;
			::cout << "[����ʱ��]:       " << time_record << " s" << endl;
			::cout << "[�ļ���С]:       " << len_of_wholeMSG * 8 << " bit" << endl;
			::cout << "[����ͷ��С�ܺ�]: " << header_size * packnum * 8 << " bit" << endl;
			double throu = (double)(len_of_wholeMSG + packnum * header_size) * 8 / time_record;
			::cout << "[ƽ��������]:     " << throu << "bit/s" << endl;
			::cout << endl;
			break;
		}
		//�жϳ�ʱ����ǰʱ�� - �ڴ��յ���ack��Ӧ��seq���ݰ����͵�ʱ�� �� ��ǰ���Ѿ������� debug
		if ((clock() - recordtime[current_hope_ack % record_num] > max_waitingtime))
		{
			in_send = "\n[ACK��ʱ]:      ��ʱδ�յ��ڴ���ack [" + to_string(current_hope_ack) + "] ���´���";
			//std::cout << "\n[ACK��ʱ]:      ��ʱδ�յ��ڴ���ack [" << current_hope_ack << "] ���´���" << endl;
			std::cout << in_send << endl;
			// 3. ״̬��ʱ����ص�������״̬
			// ������״̬��ʱ
			ssthresh = cwnd / 2;
			cwnd = MSS;
			dupACKcount = 0;
			if (state == slow_start)
			{
				::cout << "\n[��ʱ]        �Դ���������" << endl;
			}
			// ӵ������״̬��ʱ
			else if (state == avoid_congestion)
			{
				state = slow_start;
				::cout << "\n[��ʱ״̬�л�] ӵ������->������" << endl;
			}
			// ���ٻָ�״̬��ʱ
			else if (state == quick_recovery)
			{
				state = slow_start;
				::cout << "\n[��ʱ״̬�л�] ���ٻָ�->������" << endl;
			}
			LastByteSent = LastByteAcked;//��ʧ���ֽڿ�ʼ��
			current_seq = current_hope_ack;
		}
		current_window = min(cwnd,window_size);
		//if (((LastByteSent - LastByteAcked) < current_window) && (LastByteAcked < (len_of_wholeMSG - 1)))
		if (((LastByteSent - LastByteAcked) < current_window) && (LastByteSent < (len_of_wholeMSG - 1)))
		{		
			//ʣ�ര�ڵ��ֽڴ�С

			in_send = "\n[���ڴ�С]:    " + to_string(current_window) + "\n[ʣ�ര�ڴ�С]: " + to_string(current_window - (LastByteSent - LastByteAcked));
			int left = current_window - (LastByteSent - LastByteAcked);
			::cout << in_send << endl;
			//int temp = current_window;
			//::cout << "\n[���ڴ�С]:    " << temp << endl;
			//::cout << "\n[ʣ�ര�ڴ�С]: " << left << endl;
			//�������ݰ�
			::memset(sendbuff_data, 0, header_size + MSS);
			//��������ͷ
			e_header.flag = 0;
			e_header.ack = 0;
			//debug!!!
			if (current_seq < current_hope_ack)
			{
				current_seq = current_hope_ack;
			}
			e_header.seq = current_seq;
			
			//��������ļ���
			if ((LastByteAcked + current_window) > (len_of_wholeMSG - 1))
				left = (len_of_wholeMSG - 1) - LastByteSent;

			//����Ƿ������һ�����ݰ�
			//if (left <= MSS && )
			//{
			//	e_header.length = thislen = (len_of_wholeMSG - 1) - LastByteSent;//���һ�����ݰ���С
			//	e_header.flag = OVER;//������־
			//}
			if (left / MSS > 0)
			{
				e_header.length = thislen = MSS;//������ݰ�
			}
			else if (left / MSS == 0)
			{
				if (LastByteSent + left == len_of_wholeMSG - 1)
				{
					e_header.flag = OVER;//������־

				}
				//e_header.length = thislen = (LastByteAcked + cwnd) - LastByteSent;//ʣ�಻��MSS�Ĵ���
				e_header.length = thislen = left % MSS;
			}

			//������ͷ���뷢�ͻ�����
			::memcpy(sendbuff_data, &e_header, header_size);
			//���������뷢�ͻ�����
			//int len = LastByteSent;
			//pthread_mutex_lock(&counter_mutex);
			e_header.start = LastByteSent + 1;
			::memcpy(sendbuff_data + header_size, message + LastByteSent+1, thislen);
			e_header.checksum = 0;
			::memcpy(sendbuff_data, &e_header, header_size);
			//����У���
			e_header.checksum = checksum((u_short*)sendbuff_data, header_size + thislen);
			//�������ͷ��
			::memcpy(sendbuff_data, &e_header, header_size);
			result = sendto(s, sendbuff_data, header_size + thislen, 0, (sockaddr*)&router_addr, routerlen);
			//ÿ����һ������Ҫ��¼���͵�ʱ�䣬������Ҫ˵�� a��a+window_len���������ǲ���ͬʱ�ڷ����е�
			//��record_time�����������ȡģ�ظ�����
			//recordtime[current_seq % window_len] = clock();
				
			recordtime[current_seq % record_num] = clock();
			rt_invalid[current_seq % record_num] = true;
			if (result == -1)
			{
				::cout << "\n[�������ݰ�]:   [" << current_seq << "]�� ����ʧ��" << endl;
				exit(-1);
			}
			LastByteSent += thislen;
			//pthread_mutex_unlock(&counter_mutex);
			in_send = "\n[�������ݰ�]:   [" + to_string(current_seq) + "]�� ���ͳɹ�";
			//::cout << "\n[�������ݰ�]:   [" << current_seq << "]�� ���ͳɹ�" << endl;
			::cout << in_send << endl;
			current_seq++;
		}
	}

}

//�ɿͻ��˷����ر�����
const u_short FIN = 0x4;//OVER=0,FIN=1,ACK=0,SYN=0
const u_short FIN_ACK = 0x6;//OVER=0,FIN=1,ACK=1,SYN=0
const u_short FINAL_CHECK = 0x20;//FC=1,OVER=0,FIN=0,ACK=0,SYN=0
void break_connection()
{
	header e_header;
	int header_size = sizeof(e_header);
	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	int send_turn = 1;//����2��
	//int receive_turn = 2;//����2��
	bool over_time_flag = false;
	int result = 0;
	while (true)
	{
		//׼�����ݰ�

		//��һ�η���
		if (send_turn == 1)
		{
			e_header.ack = 0;
			e_header.seq = 0;
			e_header.flag = FIN;
			e_header.length = 0;//����������length=0
			//У����������
			e_header.checksum = 0;
			//����У��Ͳ�������Ӧ���
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			::cout << "[��������]:   [1] ���͵����ݰ�У���Ϊ��" << e_header.checksum << endl;
		}
		//���Ĵλ���
		else if (send_turn == 4)
		{
			e_header.ack = 0;
			e_header.seq = 1;
			e_header.flag = ACK;
			e_header.length = 0;//����������length=0
			//У����������
			e_header.checksum = 0;
			//����У��Ͳ�������Ӧ���
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			::cout << "[��������]:   [4] ���͵����ݰ�У���Ϊ��" << e_header.checksum << endl;
		}
		::memcpy(sendbuff, &e_header, header_size);

		//���Ͳ��ȴ�����

		while (true)
		{
			over_time_flag = false;//δ��ʱ
			//��һ������ ����SYN
			result = sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			if (result == -1)
			{
				::cout << "[��������]:   [" << send_turn << "] ��" << send_turn << "�λ�����Ϣ����ʧ��" << endl;
				exit(-1);//�˳�����
			}
			else
				::cout << "[��������]:   [" << send_turn << "] ��" << send_turn << "�λ�����Ϣ���ͳɹ�" << endl;

			ioctlsocket(s, FIONBIO, &non_block); //���÷�����
			clock_t start; //��ʼ��ʱ��
			clock_t end;   //������ʱ��	

			//���������رշ���0�����󷵻�-1

			if (send_turn == 1)
			{
				//��һ�ν���
				start = clock();
				over_time_flag = false;
				while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
				{
					// �жϳ�ʱ���
					end = clock();
					/// <summary>
					/// this ������ 555
					/// </summary>
					if ((end - start) > 10 * max_waitingtime)
					{
						over_time_flag = true; //��ʱ����flagΪ1
						break;
					}
				}

				//��ʱ�ش�
				if (over_time_flag)
				{
					::cout << "[��������]:   [" << send_turn << "] ʧ�� ������ʱ�����·���" << endl;
					continue;
				}

				//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
				::memcpy(&e_header, recbuff, header_size);
				::cout << "[����У���]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
				if ((e_header.flag == FIN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
				{
					::cout << "[��������]:   [2] �������ݰ� �ɹ�" << endl;
					//break;//���յ���ȷ����Ϣ������ѭ��
				}
				else
				{
					::cout << "[��������]:   [2] �������ݰ� ʧ�� ����ԭ��: �յ���������ݰ�" << endl;
					continue;//�ش�
				}

				//�ڶ��ν���
				start = clock();
				over_time_flag = false;
				while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
				{
					// �жϳ�ʱ���
					end = clock();
					if ((end - start) > 10 * max_waitingtime)
					{
						over_time_flag = true; //��ʱ����flagΪ1
						break;
					}
				}

				//��ʱ�ش�
				if (over_time_flag)
				{
					::cout << "[��������]:   [" << send_turn << "] ʧ�� ������ʱ�����·���" << endl;
					continue;
				}

				//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
				::memcpy(&e_header, recbuff, header_size);
				::cout << "[����У���]: [3] " << checksum((u_short*)&e_header, header_size) << endl;
				if ((e_header.flag == FIN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
				{
					::cout << "[��������]:   [3] �������ݰ� �ɹ�" << endl;
					break;//���յ���ȷ����Ϣ������ѭ��
				}
				else
				{
					::cout << "[��������]:   [3] �������ݰ� ʧ�� ����ԭ��: �յ���������ݰ�" << endl;
					continue;//�ش�
				}

			}
			else if (send_turn == 4)
			{
				start = clock();
				over_time_flag = false;
				while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
				{
					// �жϳ�ʱ���
					end = clock();
					if ((end - start) > 10 * max_waitingtime)
					{
						over_time_flag = true; //��ʱ����flagΪ1
						break;
					}
				}

				//��ʱ�ش�
				if (over_time_flag)
				{
					::cout << "[��������]:   [" << send_turn << "] ʧ�� ������ʱ�����·���" << endl;
					continue;
				}

				//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
				::memcpy(&e_header, recbuff, header_size);
				::cout << "[����У���]: [4] " << checksum((u_short*)&e_header, header_size) << endl;
				if (e_header.flag == FINAL_CHECK && checksum((u_short*)&e_header, header_size) == 0)
				{
					::cout << "[��������]:   [4] �������ݰ� �ɹ�" << endl;
					break;//���յ���ȷ����Ϣ������ѭ��
				}
				else
				{
					::cout << "[��������]:   [4] �������ݰ� ʧ�� ����ԭ��:�յ���������ݰ�" << endl;
					continue;//�ش�
				}
			}

		}
		if (send_turn == 1)
			send_turn += 3;
		else
			break;
	}
	::cout << "[�Ĵλ���]:   �ɹ��� �ͻ��˷��͹ر�����" << endl;
}