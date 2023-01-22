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
	UDPЭ��
	rdt 3.0
	ͣ��
*/

const u_short IP = 0x7f01;//127.0.0.1 ����ͷ��Ķ���16λ���м��0ʡ����
const u_short SourceIp = IP;
const u_short RouterIp = IP;
const u_short DestIp = IP;
int ip_32 = 2130706433;//127.0.0.1->0111 1111 0000 0000 0000 0000 0000 0001->2130706433
const u_short SourcePort = 1024;
const u_short RouterPort = 1025;
const u_short DestPort = 1026;
WSADATA wsadata;	//��ʼ��dll
//����ͷ
struct header {
	// u_short 2�ֽ�(2B) ��Ӧ16λ(16 bit)�޷��ű���
	//u_short source_ip;		//Դip 16λ ������32λ��127.0.0.1 �м䶼��0������ʡ�ԣ�������Ӧ����16+16λ��
	//u_short dest_ip;		//Ŀ��ip
	u_short ack;			//0��1
	u_short seq;			//0��1
	u_short flag;			//bigend 0λSYN��1λACK��2λFIN
	u_short source_port;	//Դ�˿�
	u_short dest_port;		//Ŀ�Ķ˿�
	u_short length;			//��Ϣ����
	u_short checksum;		//У���

	header()
	{
		//source_ip = SourceIp;
		//dest_ip = DestIp;
		source_port = SourcePort;
		dest_port = DestPort;
		ack = seq = flag = length = checksum = 0;
	}
};

// ���ܺ���
void establish_connection();		// �������ֽ�������
void receive_MSG();					// ����Ҫ������ļ�������
void break_connection();			// �Ĵλ��ֶϿ�����
u_short checksum(u_short*, size_t);	// ����У��� 

SOCKADDR_IN server_addr;//�����
SOCKADDR_IN router_addr;//·����
SOCKADDR_IN client_addr;//�ͻ���
SOCKET s;				//�׽���
//int serverlen = sizeof(server_addr);
int routerlen = sizeof(router_addr);
int main()
{
	//��ʼ������
	WSAStartup(MAKEWORD(2, 2), &wsadata);
	//Ŀ�Ķˣ�server Դ�ˣ�client
	//�����
	server_addr.sin_addr.s_addr = htonl(ip_32); //Ŀ�Ķ�ip
	server_addr.sin_port = htons(DestPort);		//Ŀ�Ķ˿ں�
	server_addr.sin_family = AF_INET;//IPV4
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
	bind(s, (SOCKADDR*)&server_addr, sizeof(server_addr));

	establish_connection();
	receive_MSG();
	break_connection();
}

//����������������
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
	memset(buf, 0, size_need); //����buff �����ݱ���0����Ϊ16��������
	memcpy(buf, mes, size);	   //���͵�buf ��Ҫ���Ͳ���0
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

	//������α�ײ�,���ڿ��ܻ�дһ��struct������װα�ײ�
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
	cout << "[ϵͳ]: ����˿������ȴ����ӿͻ���\n" << endl;
	header e_header;
	int header_size = sizeof(e_header);
	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	int result = 0;
	bool over_time_flag = false;

	while (true)
	{
		//����������Ϣ
		result = recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen);
		if (result == -1)
		{
			cout << "[��������]:   [1] ��һ�����ֽ���ʧ��" << endl;
			exit(-1);
		}
		memcpy(&e_header, recbuff, header_size);
		cout << "[����У���]: [1] " << checksum((u_short*)&e_header, header_size) << endl;
		if (checksum((u_short*)&e_header, header_size) == 0 && e_header.flag == SYN)
		{
			cout << "[��������]:   [1] �ɹ����յ�һ����������" << endl;
			break;
		}
		else
		{
			cout << "[��������]:   [1] ʧ�ܣ��������ݰ�����" << endl;
		}
	}

	//ACK :��1ʱ�ñ��Ķ�Ϊȷ�ϱ��Ķ�
	//ack :��ack��ΪTCP���Ķ��ײ��С�ȷ�Ϻ��ֶΡ��ľ�����ֵ��
	//ack=x+1˵��Bϣ��A�´η����ı��Ķεĵ�һ�������ֽ�Ϊ���=x+1���ֽڣ�
	//ack=y+1˵��Aϣ��B�´η����ı��Ķεĵ�һ�������ֽ�Ϊ���=y+1���ֽڡ�
	e_header.flag = SYN_ACK;
	e_header.ack = (e_header.seq + 1) % 2;//���������seq��0/1
	e_header.seq = 0;
	e_header.length = 0;
	e_header.checksum = 0;
	e_header.checksum = checksum((u_short*)(&e_header), header_size);
	//���Կ��Բ����÷�����ͨ��
	//������տ��ܶ�ʧ����أ�
	ioctlsocket(s, FIONBIO, &non_block); //���÷�����
	clock_t start;
	clock_t end;
	memcpy(sendbuff, &e_header, header_size);
	while (true)
	{
		result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
		if (result == -1)
		{
			cout << "[��������]:   [2] �ڶ�������������ʧ��" << endl;
			exit(-1);
		}
		else
			cout << "[��������]:   [2] �ڶ������������ͳɹ�" << endl;
		start = end = clock();
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			end = clock();
			if (end - start > max_waitingtime)
			{
				result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
				if (result == -1)
				{
					cout << "[��������]:   [2] �ڶ��������������·���ʧ��" << endl;
					exit(-1);
				}
				else
					cout << "[��������]:   [2] �ڶ��������������·��ͳɹ�" << endl;
				start = end = clock();
			}
		}

		memcpy(&e_header, recbuff, header_size);
		cout << "[����У���]: [3] " << checksum((u_short*)&e_header, header_size) << endl;
		if (checksum((u_short*)&e_header, header_size) == 0 && e_header.flag == ACK)
		{
			cout << "[��������]:   [3] ����������������ճɹ�" << endl;
			break;
		}
		else
		{
			cout << "[��������]:   [3] ����������������մ�������ݰ������´���" << endl;
		}
	}

	//cout << "[��������]: [3] �ɹ� ���������ֳɹ�" << endl;
	cout << "\n[��������]: Congratulations!" << endl;
	cout << "[��������]: �������ͻ��˳ɹ��������ӣ�\n" << endl;
}

const int maxsize_total = 100000000;//�ļ��ܵ��ֽ���
const int maxsize_data = 1024;		//����
const unsigned char OVER = 0x8;//OVER=1,FIN=0,ACK=0,SYN=0
const unsigned char OVER_ACK = 0xA;//OVER=1,FIN=0,ACK=1,SYN=0
char* message = new char[maxsize_total];//�洢�����յ�������

void savefile(string name, int len)
{
	string filename = name;
	ofstream fout(filename.c_str(), ofstream::binary);
	for (int i = 0; i < len; i++)
	{
		fout << message[i];
	}
	fout.close();
	cout << "[�ļ�����]:   �ļ��ѳɹ����浽����\n" << endl;
}

//�����ļ�
void receive_MSG()
{
	//�ļ��洢�ĵ�ַ
	cout << "������洢��ַ:��:1.jpg" << endl;
	string a;
	cin >> a;

	//����ͷ
	header e_header;
	//����ͷ�ֽ���
	int header_size = sizeof(e_header);
	//���ջ����� ����ͷ��������
	char* recbuff = new char[header_size + maxsize_data];
	//���ͻ����� ������ͷ��
	char* sendbuff = new char[header_size];
	//�ڴ����յ�seq
	int hope_seq = 0;
	//���͵�ack
	int send_ack = 0;
	//���ܵĴ洢message�����е�ƫ��
	int offset = 0;
	//����Ϊ��������
	ioctlsocket(s, FIONBIO, &block);
	//��ʼ׼������
	while (true)
	{
		//�ȴ��������ݰ�
		while (recvfrom(s, recbuff, header_size + maxsize_data, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			cout << "[��������]:   �������󣬼����ȴ�" << endl;
		}
		//���յ������ݰ�,�����ݰ�ͷ������e_header
		memcpy(&e_header, recbuff, header_size);
		//�ж����ݰ��Ƿ���ȷ��1.���յ�����Ϊ�ڴ���seq��2.У���Ϊ0��
		if (e_header.seq == hope_seq && checksum((u_short*)recbuff, header_size + e_header.length) == 0)
		{
			//�����յ���������������ܵĴ洢����message��
			memcpy(message + offset, recbuff + header_size, e_header.length);
			offset += e_header.length;
			cout << "[����У���]: " << 0 << endl;
			cout << "[���ݽ���]:   �ڴ��� [" << e_header.seq << "]�����ݰ� " << endl;
			cout << "[���ݳ���]:   " << e_header.length << endl;
			//��������һ�������������һ�����͵�ackҪ����OVER_ACK�ı�ǩ
			if (e_header.flag == OVER)
				e_header.flag = OVER_ACK;

			//���ص�ackΪ�ڴ��յ���seq��
			e_header.ack = hope_seq;
			//���У������¼��㲢����У����
			e_header.checksum = 0;
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			//��Ҫ���ص�����ͷ���뷢�ͻ�����
			memcpy(sendbuff, &e_header, header_size);
			//routerԭ�򣬿ͻ���һ�����յ�
			sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			//����hope_seq��ack
			//debug!!!
			send_ack = hope_seq;//���յ���seq�������ۼ�ȷ��ack
			hope_seq = hope_seq + 1;//��һ���ڴ���seq

			if (e_header.flag == OVER_ACK)
			{
				cout << "[��������]:   �ɹ������ļ�!" << endl;
				break;
			}

		}
		//����ȷ�����ص����ۼ�ȷ�ϵ�ack�������µȴ�
		else
		{
			cout << "[��������]:   ���� ["<<e_header.seq<<"] �����ݰ� ���ڴ����� [" << hope_seq <<"]"<< endl;
			e_header.ack = send_ack;
			e_header.checksum = 0;
			e_header.checksum = checksum((u_short*)&e_header, header_size);
			memcpy(sendbuff, &e_header, header_size);
			//routerԭ�򣬿ͻ���һ�����յ�
			sendto(s, sendbuff, header_size, 0, (sockaddr*)&router_addr, routerlen);
			continue;
		}

	}
	//�����ļ�
	savefile(a, offset);

}
//�ɿͻ��˷����ر�����
const u_short FIN = 0x4;//OVER=0,FIN=1,ACK=0,SYN=0
const u_short FIN_ACK = 0x6;//OVER=0,FIN=1,ACK=1,SYN=0
const u_short FINAL_CHECK = 0x20;//FC=1,OVER=0,FIN=0,ACK=0,SYN=0
void break_connection()
{
	//���ڻ�����������
	header e_header;
	int header_size = sizeof(e_header);
	char* sendbuff = new char[header_size];
	char* recbuff = new char[header_size];
	int result = 0;
	// ���յ�һ�λ�������
	while (true)
	{
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			cout << "[��������]:   �������󣬼����ȴ�" << endl;
		}
		//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
		memcpy(&e_header, recbuff, header_size);
		if ((e_header.flag == FIN) && checksum((u_short*)&e_header, header_size) == 0)
		{
			cout << "[��������]:   [1] �ɹ����յ�1�λ�������" << endl;
			break;//���յ���ȷ����Ϣ������ѭ��
		}
		else
		{
			cout << "[��������]:   [2] ʧ�� �յ���������ݰ�,���½���" << endl;
			continue;//�ش�
		}
	}

	//���յ�һ�λ������� �ظ�����
	e_header.ack = 0;
	e_header.seq = 0;
	e_header.flag = FIN_ACK;
	e_header.length = 0;//����������length=0
	//У����������
	e_header.checksum = 0;
	//����У��Ͳ�������Ӧ���

	cout << "[����У���]: [2] " << checksum((u_short*)&e_header, header_size) << endl;
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	memcpy(sendbuff, &e_header, header_size);
	result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	if (result == -1)
	{
		cout << "[��������]:   [2] �������ݰ� ʧ�� ����ԭ��: ���Ӵ���" << endl;
		exit(-1);
	}
	else
		cout << "[��������]:   [2] �������ݰ� �ɹ�" << endl;

	result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	if (result == -1)
	{
		cout << "[��������]:   [3] �������ݰ� ʧ�� ����ԭ��: ���Ӵ���" << endl;
		exit(-1);
	}
	else
		cout << "[��������]:   [3] �������ݰ� �ɹ�" << endl;

	while (true)
	{
		while (recvfrom(s, recbuff, header_size, 0, (sockaddr*)&router_addr, &routerlen) <= 0)
		{
			cout << "[��������]:   �������󣬼����ȴ�" << endl;
		}
		//δ��ʱ���գ��ж���Ϣ�Ƿ���ȷ
		memcpy(&e_header, recbuff, header_size);
		if ((e_header.flag == ACK) && checksum((u_short*)&e_header, header_size) == 0)
		{
			cout << "[��������]:   [4] �������ݰ� �ɹ�" << endl;
			break;//���յ���ȷ����Ϣ������ѭ��
		}
		else
		{
			cout << "[��������]:   [4] �������ݰ� ʧ��" << endl;
			continue;//
		}
	}

	e_header.flag = FINAL_CHECK;
	e_header.checksum = 0;
	cout << "[����У���]: [5] " << checksum((u_short*)&e_header, header_size) << endl;
	e_header.checksum = checksum((u_short*)&e_header, header_size);
	memcpy(sendbuff, &e_header, header_size);
	result = sendto(s, sendbuff, header_size, 0, (SOCKADDR*)&router_addr, routerlen);
	if (result == -1)
	{
		cout << "[��������]:   [5] ��4�λ���ȷ�ϵ�ACK����ʧ��" << endl;
		exit(-1);
	}
	else
		cout << "[��������]:   [5] ��4�λ���ȷ�ϵ�ACK���سɹ�" << endl;
}