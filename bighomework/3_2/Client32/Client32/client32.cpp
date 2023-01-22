//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
using namespace std;
#include<WinSock2.h>
#include<time.h>
#include<fstream>
#include<iostream>
#include<string>
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

	header()
	{
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
const int max_waitingtime = CLOCKS_PER_SEC;
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
const int maxsize_data = 1024;		//һ�����ݵ������
const u_short OVER = 0x8;//OVER=1,FIN=0,ACK=0,SYN=0
const u_short OVER_ACK = 0xA;//OVER=1,FIN=0,ACK=1,SYN=0
clock_t time_file;//�ܵ�ʱ��

const int window_len = 4;//�������ڵĴ�С Ϊ4
int seq_total = 0;// �ܵ�seq��
int current_seq = 0;//��ǰ��Ҫ���͵�seq
int current_hope_ack = 0;//��ǰ�ڴ��յ���ack
char* sendbuff_data;//���͵İ�,����������
char* recbuff_data;//�յ��İ�,ֻ��Ҫ��������ͷ
double recordtime[window_len] = { 0,0,0,0 };//ÿ��������ʱ��ʱ��
unsigned long long int packnum = 0;//�ܹ�Ҫ���͵����ݰ�����

//����ack�̣߳�������ϵ��һЩȫ�ֱ�������
DWORD WINAPI receiveACK(LPVOID param)
{
	SOCKET* clientSock = (SOCKET*)param;
	header e_header;//����ͷ
	int header_size = sizeof(e_header);//����ͷ�ֽ���

	while (true)
	{
		//����ack
		if (recvfrom(s, recbuff_data, header_size, 0, (sockaddr*)&router_addr, &routerlen) > 0)
		{
			//�յ��ظ����жϻظ��ĶԲ���
			::memcpy(&e_header, recbuff_data, header_size);
			//�ж��Ƿ�Ϊ�ڴ����յ�ack
			if (e_header.ack == current_hope_ack && checksum((u_short*)&e_header, header_size) == 0)
			{
				::cout << "\n[�ڴ�ACK]       " << current_hope_ack << endl;
				::cout << "[����ACK]:      " << e_header.ack << endl;
				//������յ������һ�����ݰ���ack
				if (e_header.flag == OVER_ACK)
				{
					::cout << "[����ACK]:      �յ�����OVER_ACK" << endl;
					current_hope_ack++;
					break;//ֹͣ����
				}
				//�ڴ�������һ�����ݰ���ack
				current_hope_ack++;
			}
			else
			{
				::cout << "\n[�ڴ�ACK]       " << current_hope_ack << endl;
				cout << "\n[�ظ�ACK]:      [" << e_header.ack << "]�� �յ��ظ�ACK" << endl;
				continue;
			}
		}

	}
	return 0;
}

//�������ݰ��߳�
void read_and_send_file()
{
	//�����ļ�
	string file_name;
	cout << "[�ļ�����]: ACTION" << endl;
	cout << "[�ļ�����]: ����Ҫ�����ļ��ľ���·��" << endl;
	cin >> file_name;
	ifstream fin(file_name.c_str(), ios::in | ios::binary);
	if (!fin.is_open())
	{
		cout << "[�ļ���]: ʧ�ܣ�" << endl;
		exit(-1);
	}
	char* message = new char[maxsize_total];
	char c;
	unsigned long long int len_of_wholeMSG = 0;//�����ļ��ĳ���
	unsigned char temp = fin.get();
	while (fin)
	{
		message[len_of_wholeMSG++] = temp;
		temp = fin.get();
	}
	fin.close();
	cout << "[�ļ�����]: ���سɹ�" << endl;

	header e_header;//����ͷ
	int header_size = sizeof(e_header);//����ͷ�ֽ���
	sendbuff_data = new char[header_size + maxsize_data];//���ͻ�����
	recbuff_data = new char[header_size];//���ջ�����

	current_seq = 0;//������Ҫ���͵�seq
	packnum = len_of_wholeMSG / maxsize_data;//һ��Ҫ���͵İ��ĸ��������к��ܴ�С
	if (len_of_wholeMSG % maxsize_data != 0)
		packnum += 1;
	packnum = packnum - 1;//0,1,����,n-1
	int countpacknum = 0;//������¼���˼�����
	int result = 0;//������¼���ݰ������������ȷ���
	seq_total = packnum - 1;//�ܹ���seq��
	//clock_t start;//��ʱ��
	int thislen = 0;//���Ĵ�С
	ioctlsocket(s, FIONBIO, &non_block); //���÷�����
	time_file = clock();//�����ʱ���С
	cout << "���ݰ��ܸ���\n\n" << packnum << endl;

	//��������ack�߳�
	HANDLE receiveack = CreateThread(nullptr, 0, receiveACK, LPVOID(&s), 0, nullptr);

	for (int i = 0; i < window_len; i++)
	{
		recordtime[i] = clock();
	}

	while (true)
	{
		//�жϳ�ʱ����ǰʱ�� - �ڴ��յ���ack��Ӧ��seq���ݰ����͵�ʱ��
		if (clock() - recordtime[current_hope_ack % window_len] > max_waitingtime)
		{
			current_seq = current_hope_ack;//��ʧ�İ���ʼ��
			cout << "[ACK��ʱ]:      ��ʱδ�յ��ڴ���ack [" << current_hope_ack << "] ���´���" << endl;
			//start = clock();//��ʱ���⣡
		}

		// �����ǰ�����п��� �� ���͵�seq�������ܵ����ݰ�����
		if ((current_seq - (current_hope_ack - 1) <= window_len) & (current_seq <= packnum))
		{
			//��ӡ���ڵĴ�С
			cout << "[ʣ�ര�ڴ�С]: " << window_len - (current_seq - current_hope_ack) << endl;
			//�������ݰ�
			memset(sendbuff_data, 0, header_size + maxsize_data);
			//��������ͷ
			e_header.flag = 0;
			e_header.ack = 0;
			e_header.seq = current_seq;

			//����Ƿ������һ�����ݰ�
			if (current_seq == packnum)
			{
				e_header.length = thislen = len_of_wholeMSG % maxsize_data;//���һ�����ݰ���С
				e_header.flag = OVER;//������־
			}
			else
				e_header.length = thislen = maxsize_data;//������ݰ�

			//������ͷ���뷢�ͻ�����
			memcpy(sendbuff_data, &e_header, header_size);
			//���������뷢�ͻ�����
			int len = current_seq * maxsize_data;
			memcpy(sendbuff_data + header_size, message + len, thislen);
			e_header.checksum = 0;
			memcpy(sendbuff_data, &e_header, header_size);
			//����У���
			e_header.checksum = checksum((u_short*)sendbuff_data, header_size + maxsize_data);
			//�������ͷ��
			memcpy(sendbuff_data, &e_header, header_size);
			result = sendto(s, sendbuff_data, header_size + thislen, 0, (sockaddr*)&router_addr, routerlen);
			//ÿ����һ������Ҫ��¼���͵�ʱ�䣬������Ҫ˵�� a��a+window_len���������ǲ���ͬʱ�ڷ����е�
			//��record_time�����������ȡģ�ظ�����
			recordtime[current_seq % window_len] = clock();
			if (result == -1)
			{
				cout << "[�������ݰ�]:   [" << current_seq << "]�� ����ʧ��" << endl;
				exit(-1);
			}
			cout << "\n[�������ݰ�]:   [" << current_seq << "]�� ���ͳɹ�" << endl;
			current_seq++;
		}

		if (current_hope_ack == packnum + 1)
		{
			CloseHandle(receiveack);
			double time_record = (clock() - time_file) / 1000.0;
			cout << "[��������]:     �ļ�������ϣ�\n" << endl;
			cout << "[����ʱ��]:       " << time_record << " s" << endl;
			cout << "[�ļ���С]:       " << len_of_wholeMSG * 8 << " bit" << endl;
			cout << "[����ͷ��С�ܺ�]: " << header_size * packnum * 8 << " bit" << endl;
			double throu = (double)(len_of_wholeMSG + packnum * header_size) * 8 / time_record;
			cout << "[ƽ��������]:     " << throu << "bit/s" << endl;
			cout << endl;
			break;
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
			cout << "[��������]:   [1] ���͵����ݰ�У���Ϊ��" << e_header.checksum << endl;
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
			cout << "[��������]:   [4] ���͵����ݰ�У���Ϊ��" << e_header.checksum << endl;
		}
		memcpy(sendbuff, &e_header, header_size);

		//���Ͳ��ȴ�����

		while (true)
		{
			over_time_flag = false;//δ��ʱ
			//��һ������ ����SYN
			result = sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			if (result == -1)
			{
				cout << "[��������]:   [" << send_turn << "] ��" << send_turn << "�λ�����Ϣ����ʧ��" << endl;
				exit(-1);//�˳�����
			}
			else
				cout << "[��������]:   [" << send_turn << "] ��" << send_turn << "�λ�����Ϣ���ͳɹ�" << endl;

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
					cout << "[��������]:   [" << send_turn << "] ʧ�� ������ʱ�����·���" << endl;
					continue;
				}

				//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
				memcpy(&e_header, recbuff, header_size);
				cout << "[����У���]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
				if ((e_header.flag == FIN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
				{
					cout << "[��������]:   [2] �������ݰ� �ɹ�" << endl;
					//break;//���յ���ȷ����Ϣ������ѭ��
				}
				else
				{
					cout << "[��������]:   [2] �������ݰ� ʧ�� ����ԭ��: �յ���������ݰ�" << endl;
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
					cout << "[��������]:   [" << send_turn << "] ʧ�� ������ʱ�����·���" << endl;
					continue;
				}

				//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
				memcpy(&e_header, recbuff, header_size);
				cout << "[����У���]: [3] " << checksum((u_short*)&e_header, header_size) << endl;
				if ((e_header.flag == FIN_ACK) && checksum((u_short*)&e_header, header_size) == 0)
				{
					cout << "[��������]:   [3] �������ݰ� �ɹ�" << endl;
					break;//���յ���ȷ����Ϣ������ѭ��
				}
				else
				{
					cout << "[��������]:   [3] �������ݰ� ʧ�� ����ԭ��: �յ���������ݰ�" << endl;
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
					cout << "[��������]:   [" << send_turn << "] ʧ�� ������ʱ�����·���" << endl;
					continue;
				}

				//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
				memcpy(&e_header, recbuff, header_size);
				cout << "[����У���]: [4] " << checksum((u_short*)&e_header, header_size) << endl;
				if (e_header.flag == FINAL_CHECK && checksum((u_short*)&e_header, header_size) == 0)
				{
					cout << "[��������]:   [4] �������ݰ� �ɹ�" << endl;
					break;//���յ���ȷ����Ϣ������ѭ��
				}
				else
				{
					cout << "[��������]:   [4] �������ݰ� ʧ�� ����ԭ��:�յ���������ݰ�" << endl;
					continue;//�ش�
				}
			}

		}
		if (send_turn == 1)
			send_turn += 3;
		else
			break;
	}
	cout << "[�Ĵλ���]:   �ɹ��� �ͻ��˷��͹ر�����" << endl;
}