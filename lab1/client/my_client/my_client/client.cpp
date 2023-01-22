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
char my_ip[100];//������ip
int port;//�������˿ں�
char name[100];//client���ƣ��ɷ��������أ��ǿ����޸ĵ�
int flag = 1;//��send���������˳������ҵ�����ʱ�������߳�ҲӦ����Ӧֹͣ
char receivebuffer[100];//���ջ�����
char sendbuffer[100];//���ͻ�����

//ʱ�亯������ӡ��ǰʱ�� ������
void Y_M_D()
{
	SYSTEMTIME y_m_d;
	GetLocalTime(&y_m_d);
	cout << "�����ǡ���" << y_m_d.wYear << "��" << y_m_d.wMonth << "��" << y_m_d.wDay << "��" << endl;
}

//ʱ�亯������ӡ��ǰʱ�� ʱ����
void H_M_S()
{
	SYSTEMTIME h_m_s;
	GetLocalTime(&h_m_s);
	cout << h_m_s.wHour << ":" << h_m_s.wMinute << ":" << h_m_s.wSecond;
}

DWORD WINAPI re_ma(LPVOID lparam);
DWORD WINAPI se_ma(LPVOID lparam);

int main() {
	//��ӡ��������Ϣ
	cout << "[�𾴵Ŀͻ�],���!" << endl;
	Y_M_D();
	cout << endl;
	cout << "�����ڳ�ʼ�������ҷ������ˣ�   loading����" << endl;


	//��ʼ��WSDATA
	WSADATA ws;
	//��ʼ��Socket DLL��Э��ʹ�õ�Socket�汾
	//MAKEWORD(2, 2) ����2.2�汾������ϣ��ʹ�õ���߰汾
	//ws ����Socket����ϸ��Ϣ
	int result;
	result = WSAStartup(MAKEWORD(2, 2), &ws);
	//cout << result;
	H_M_S();
	if (result == success)
	{
		cout << " [WSADATA��ʼ��]:       �ɹ�,���õ�socket��߰汾Ϊ2.2��" << endl;
	}
	else
	{
		cout << " WSADATA��ʼ����ʧ��;�������: " << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}

	//��ʼ��һ��socket
	SOCKET s;
	//��ʼ��socket��ipv4����������TCPЭ��
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//s�ķ���ֵ
	//0,1,2�ֱ��ʾ��׼���롢��׼�������׼����
	//���������򿪵��ļ��������������2, ����ʱ�ͷ��� -1. ����INVALID_SOCKET Ҳ������Ϊ -1 
	H_M_S();
	if (s == -1)
	{
		cout << " SOCKET����ʧ��;�������: " << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else
		cout << " [SOCKET����]:          �ɹ���" << endl;

	//����
	//sockaddr_in����socket����͸�ֵ��
	sockaddr_in sa_in;
	//bzero(&sa_in, sizeof(sa_in)); bzero��Ҫ��linux��ʹ�ã�����
	//��ʼ��sa_in
	memset(&sa_in, 0, sizeof(sa_in));
	H_M_S();
	cout << " [���������ip��ַ]:    ʹ��Ĭ��ip������localhost: ";
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
	cout << " [������������˿ں�]:  ����1024-5000֮��ѡ��: ";
	while (1)
	{

		int cin_port;
		cin >> cin_port;
		if (cin_port > 5000 || cin_port < 1024)
		{
			H_M_S();
			cout << " �˿ڲ���֧�ַ�Χ�ڣ�����������: ";
		}
		else
		{
			port = cin_port;
			break;
		}
	}
	//Address familyһ����˵AF_INET����ַ�壩PF_INET��Э���壩,windows�»����޲��
	sa_in.sin_family = AF_INET;
	//ip_address
	sa_in.sin_addr.s_addr = inet_addr(my_ip);
	//port
	sa_in.sin_port = htons(port);
	cout << endl;
	cout << "[������ip��ַ]:     " << my_ip << endl;
	cout << "[������port�˿ں�]: " << port << endl;
	//ִ�����Ӳ���
	result = connect(s, (SOCKADDR*)&sa_in, sizeof(SOCKADDR));
	if (result == success)
	{
		cout << "[Ŀ�Ķ�ip��ַ]:     ���ӳɹ�" << endl;
		cout << "[Ŀ�Ķ�port�˿ں�]: ���ӳɹ�" << endl;
	}
	else
	{
		closesocket(s);//�ͷ�socket
		WSACleanup();//�ͷ�dll
		cout << "ip��port����ʧ��;������ţ�" << WSAGetLastError() << endl;
		return 0;
	}

	recv(s, name, 100, 0);//��ȡ�û�������Ӧserver�˵ĵ�һ��send
	cout << "[����������]:       �ɹ��� �����û�����" << name << endl;
	cout << endl;

	cout << "[tips]: " << endl;
	cout << "[����]:             changename newname ��:changename user1" << endl;
	cout << "[˽����Ϣ]:         sendone username message ��:sendone 230 message"<<endl;
	cout << "[�˳�������]:       quit"<< endl;
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
			cout << " [����Ϣ]:   ";
			cout << receivebuffer;
			cout << "    ---���������" << endl;;
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
			cout << " [�˳�����]: �ټ�! " << endl;
			closesocket(*s);
			return 0;
		}
		sflag = send(*s, sendbuffer, 100, 0);
		if (sflag == SOCKET_ERROR) {
			H_M_S();
			cout << " [��Ϣ����]: ʧ��; �������" << WSAGetLastError() << endl;
			closesocket(*s);
			WSACleanup();
			return 0;
		}
		else {
			H_M_S();
			cout << " [��Ϣ����]: �ɹ�"<<endl;
		}
	}
}
