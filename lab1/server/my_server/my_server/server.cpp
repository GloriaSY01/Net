#define success 0 //�ɹ��ķ��ر��Ϊ0
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
map<string, string>users;//��¼�ͻ��˵�����
map<SOCKET, int>sockets;//��¼ÿ���ͻ��˶Խӵ�socket
using namespace std;
////////һЩ���ܺ���//////////////////
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
//�ж��Ƿ�Ϊ��������
bool judge_changename(char* c) {
	if (c[0] == 'c' && c[1] == 'h' && c[2] == 'a' && c[3] == 'n' && c[4] == 'g' && c[5] == 'e' && c[6] == 'n' && c[7] == 'a' && c[8] == 'm' && c[9] == 'e')
		return true;
	return false;
}
//�ж��Ƿ�Ϊ˽������
bool judge_sendone(char* buff) {
	if (buff[0] == 's' && buff[1] == 'e' && buff[2] == 'n' && buff[3] == 'd' && buff[4] == 'o' && buff[5] == 'n'&&buff[6]=='e') 
		return true;
	return false;
}

//��ȡ������
string newname(char* buff) {
	string s = "";
	for (int i = 11; i <= 99; i++) {
		if (buff[i] == '\0' || buff[i] == ' ' || buff[i] == '/t')
			return s;
		else 
			s += buff[i];
	}
}
//˽�ĵ�Ŀ���û�
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

//�շ���Ϣ����ӡ�ȶ����߳���ִ�У�ÿһ���ͻ��˶���һ��socket���жԽ�
DWORD WINAPI my_thread(LPVOID lparam)
{
	//��ȡ�ͻ��˴�����socket
	SOCKET cs = (SOCKET)lparam;
	//map��find������
	//��������Ѱ��ֵΪcs��Ԫ�أ����ظ�Ԫ�صĵ����������򣬷���map.end()
	if (sockets.find(cs) == sockets.end())
		sockets.insert(pair<SOCKET, int>(cs, 1));//���뵽��¼socket��map��
	else
		sockets[cs] = 1;

	char name[100];
	//data()����һ��const char*ָ�룬ָ������(���Կ��ַ���ֹ)
	strcpy(name, to_string(cs).data());

	//send �����û��¼�����Ϣ
	send(cs, (const char*)name, 100, 0);
	cout << endl;
	H_M_S();
	cout << " [���û�]: " << to_string(cs).data() << "��������" << endl;

	char sendbuffer[100];//������Ϣ������
	char receivebuffer[100];//������Ϣ������
	int rflag = 1;//����
	int sflag = 1;//����
	int flag = 1;

	while (rflag != -1 && flag != 0)
	{
		rflag = recv(cs, receivebuffer, 100, 0);

		//�ж�quit break,���ٸ�socket
		if (judgequit(receivebuffer))
		{
			// mymap[cs] = 0;
			if (users.find(to_string(cs).data()) == users.end())
			{
				H_M_S();
				cout << " [�û��˳�]: �û�[" << to_string(cs).data() << "]�˳���Ⱥ��" << endl;
				sockets.erase(cs);
				break;
			}
			else
			{
				H_M_S();
				cout << " [�û��˳�]: �û�[" << users[to_string(cs).data()] << "]�˳���Ⱥ��" << endl;
				sockets.erase(cs);
				break;
			}
		}

		//�жϸ��� 
		if (judge_changename(receivebuffer))
		{
			cout << endl;
			//�¸ĵ�����
			string newName = newname(receivebuffer);
			//��¼�����֣�insertҲ����
			users[to_string(cs).data()] = newName;
			//��ӡ������
			H_M_S();
			cout << " [�û�����]: " << to_string(cs).data() << " �޸��ǳ�Ϊ " << newName << "" << endl;
			continue;
		}

		//����˽����Ϣ
		if (judge_sendone(receivebuffer))
		{
			bool stflag = false;
			string s = target_user(receivebuffer);
			for (auto it : sockets)
			{
				//cout << it.first;
				//˽�Ĺ���
				//������
				if (users[to_string(it.first).data()] == s)
				{
					//�������͵���Ϣ
					strcpy(sendbuffer, "�û�");
					//������Ϣ����
					if (users.find(to_string(cs).data()) == users.end()) {
						strcat(sendbuffer, to_string(cs).data());
					}
					else {
						strcat(sendbuffer, users[to_string(cs).data()].c_str());
					}

					strcat(sendbuffer, " ����˵��");
					// cout << message << endl;
					strcat(sendbuffer, (const char*)receivebuffer);
					H_M_S();
					if (users.find(to_string(cs).data()) == users.end()) {
						cout << " [�û�˽��]: " << to_string(cs).data() << "˵: " << receivebuffer << endl;
					}
					else {
						cout << " [�û�˽��]: " << users[to_string(cs).data()] << "˵: " << receivebuffer << endl;
					}
					send(it.first, sendbuffer, 100, 0);
					stflag = true;
					break;
				}

				//û����
				if (to_string(it.first).data() == s) {
					strcpy(sendbuffer, "�û�");
					if (users.find(to_string(cs).data()) == users.end()) {
						strcat(sendbuffer, to_string(cs).data());
					}
					else {
						strcat(sendbuffer, users[to_string(cs).data()].c_str());
					}
					strcat(sendbuffer, " (˽��): ");
					strcat(sendbuffer, (const char*)receivebuffer);
					H_M_S();
					if (users.find(to_string(cs).data()) == users.end()) {
						cout << " [�û�˽��]: " << to_string(cs).data() << "˵: " << receivebuffer << endl;
					}
					else {
						cout << " [�û�˽��]: " << users[to_string(cs).data()] << "˵: " << receivebuffer << endl;
					}
					send(it.first, sendbuffer, 100, 0);
					stflag = true;
					break;
				}
			}

			//���߷�������Ŀͻ��˷���ʧ��
			if (stflag == false)
			{
				strcpy(sendbuffer, "����ʧ��");
				send(cs, sendbuffer, 100, 0);
				H_M_S();
				cout << " [˽����Ϣ]: ����ʧ��" << endl;
			}
			continue;
		}

		// Ⱥ��
		if (rflag > 0)
		{

			strcpy(sendbuffer, "�û�");

			if (users.find(to_string(cs).data()) == users.end()) {
				strcat(sendbuffer, to_string(cs).data());
			}
			else {
				strcat(sendbuffer, users[to_string(cs).data()].c_str());
			}

			strcat(sendbuffer, " ˵��");

			//if (mymap[cs] == 1)
			//		strcat(sendbuffer, "��ӭ�������ԣ�");
				

			strcat(sendbuffer, (const char*)receivebuffer);
			cout << endl;

			//�������˴�ӡ
			H_M_S();
			if (sockets[cs] == 1)
			{	
				strcpy(sendbuffer, "�µ��û�");
				strcat(sendbuffer, to_string(cs).data());
				strcat(sendbuffer, "����!");
				sockets[cs] += 1;
				if (users.find(to_string(cs).data()) == users.end())
					cout << " [�û�����]: " << to_string(cs).data() << "˵: " << "��ӭ�������ԣ�" << endl;
				else
					cout << " [�û�����]: " << users[to_string(cs).data()] << "˵: " <<  "��ӭ�������ԣ�" << endl;
			}
			else {
				if (users.find(to_string(cs).data()) == users.end()) 
					cout << " [�û�����]: " << to_string(cs).data() << "˵: " << receivebuffer << endl;
				else 
					cout << " [�û�����]: " << users[to_string(cs).data()] << "˵: " << receivebuffer << endl;
			}

			for (auto it : sockets) {

				if (it.first != cs && it.second == 2) {
					///cout<<sendbuffer<<endl;
					auto ans = send(it.first, sendbuffer, 100, 0);
					if (ans == SOCKET_ERROR) {
						cout << "[Ⱥ����Ϣ]: " << it.first << "����ʧ�ܣ�" << "������ţ� " << endl;
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

//������
//����socket���󶨶˿ںš���ʼ���������������Ϊÿһ���ͻ��˴���ͨ�ŵ�socket
int main()
{
	//��ӡ��������Ϣ
	cout << "[�𾴵Ĺ���Ա],���!" << endl;
	Y_M_D();
	cout << endl;
	cout << "�����ڳ�ʼ�������ҷ������ˣ�loading����" << endl;


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
		cout << " [WSADATA��ʼ��]: �ɹ�,���õ�socket��߰汾Ϊ2.2��" << endl;
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
		cout << " [SOCKET����]:    �ɹ���" << endl;

	//��bind
	//sockaddr_in����socket����͸�ֵ��
	sockaddr_in sa_in;
	//bzero(&sa_in, sizeof(sa_in)); bzero��Ҫ��linux��ʹ�ã�����
	//��ʼ��sa_in
	memset(&sa_in, 0, sizeof(sa_in));
	H_M_S();
	cout << " [����ip��ַ]:    ʹ��Ĭ��ip������localhost: ";
	cin >> my_ip;
	if ((string)my_ip == "localhost")
	{
		memset(&my_ip, 0, sizeof(my_ip));
		strcat(my_ip, "127.0.0.1");
	}
	//cout << my_ip << endl;
	H_M_S();
	cout << " [������˿ں�]:  ����1024-5000֮��ѡ��: ";
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
	cout << "[ip��ַ]:     " << my_ip << endl;
	cout << "[port�˿ں�]: " << port << endl;

	//ִ�а󶨲���
	result = bind(s, (SOCKADDR*)&sa_in, sizeof(SOCKADDR));
	if (result == success)
	{
		cout << "[ip��ַ]:     �󶨳ɹ�" << endl;
		cout << "[port�˿ں�]: �󶨳ɹ�" << endl;
	}
	else
	{
		closesocket(s);//�ͷ�socket
		WSACleanup();//�ͷ�dll
		cout << "ip��port��ʧ��;������ţ�" << WSAGetLastError() << endl;
		return 0;
	}

	//����
	result = listen(s, 20);
	if (result == success)
	{
		cout << "\n������: ";
		H_M_S();
		cout << endl;
		cout << "[�����ҳ�ʼ��]: �ɹ�!" << endl;
		cout << "[����]:         20��" << endl;
		cout << "[����]:         ��ʼ!" << endl;
	}
	else
	{
		closesocket(s);//�ͷ�socket
		WSACleanup();//�ͷ�dll
		cout << "����ʧ��; ������ţ�" << WSAGetLastError() << endl;
		return 0;
	}

	cout << endl;

	cout << "------�ȴ�һ�����û����룡------" << endl;

	while (true)
	{

		sockaddr_in client_sa_in;
		int addr_temp = sizeof(sockaddr_in);
		//����һ���ض�socket����ȴ������е���������δ�յ����������

		//s��socket��������
		//addr������Զ�̶˵�ַ��
		//namelen�����ص�ַ���ȡ�

		//�Խӿͻ��˵�socket
		SOCKET c_s;
		c_s = accept(s, (SOCKADDR*)&client_sa_in, &addr_temp);
		if (c_s == -1)
		{
			H_M_S();
			closesocket(s);//�ͷ�socket
			WSACleanup();//�ͷ�dll
			cout << " ���ӿͻ���ʧ��;������ţ�" << WSAGetLastError() << endl;
		}
		else
		{
			HANDLE cthread = CreateThread(NULL, 0, my_thread, LPVOID(c_s), 0, NULL);
			CloseHandle(cthread);
		}

	}
	closesocket(s);//�ͷ�socket��Դ
	WSACleanup();//�ͷ�dll��Դ
	cout << endl;
	cout << "[����˹ر�]: �ټ���" << endl;
}

