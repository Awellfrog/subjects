#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>
#include <iostream>
using namespace std;

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996)

#define	SERVER_PORT	12340		//�˿ں�
#define	SERVER_IP	"0.0.0.0"	//IP��ַ
const int BUFFER_LENGTH = 1026;	//��������С������̫���� UDP ������֡�а�����ӦС�� 1480 �ֽڣ�
const int SEND_WIND_SIZE = 4;	//���ʹ��ڵĴ�СΪ10��GBN ��Ӧ���� W+1<=N ��W�����ʹ��ڴ�С��N�����кŸ�����
								//����ȡ���к� 0...19 �� 20 ��
								//��������ڴ�С����Ϊ1����Ϊͣ-��Э��
const int RECV_WIND_SIZE = 4;	//���ݽ��մ��ڵĴ�С
const int SEQ_SIZE = 20;		//���кŵĸ������� 0~19 ����20��
								//���ڷ������ݵ�һ���ֽ����ֵΪ0�������ݷ��ͻ�ʧ��
								//��˽��ն����Ϊ1~20���뷢�Ͷ�һһ��Ӧ
const int PACKET_NUM_MAX = 100; //������౻���Ϊ PACKET_NUM_MAX ��

BOOL ack[SEQ_SIZE];	//�յ� ack �������Ӧ 0~19 ��ack
int curSeq;	//��ǰ���ݰ��� seq
int curAck;	//��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;		//�յ��İ�������
int totalPacket;	//��Ҫ���͵İ�������
char buffer[BUFFER_LENGTH];	// ���ݷ��ͽ��ջ�����

typedef struct SRWindow { //�����շ����ڣ�����ҿ�
	int left;
	int right;

} SRWindow;

void getCurTime(char* ptime); //��ȡ��ǰʱ��
bool seqIsAvailable(); //��ǰ���к��Ƿ���ã����Ƿ���Խ������ݱ���������Ȼ�ڵȴ���һ�������ݱ���
void timeoutHandler(); //��ʱ�ش�
void ackHandler(char c); //�ۼ�ȷ�� ACK
void TestGBN(SOCKET sockServer, SOCKADDR_IN* addrClient, char buffer[]);
void Download(SOCKET sockServer, SOCKADDR_IN* addrClient, char buffer[]);
void Upload(SOCKET socketClient, SOCKADDR_IN* addrServer, float packetLossRatio, float ackLossRatio, char buffer[]);
void DownloadSR(SOCKET sockServer, SOCKADDR_IN* addrClient, char buffer[]);
void UploadSR(SOCKET sockServer, SOCKADDR_IN* addrClient, float packetLossRatio, float ackLossRatio, char buffer[]);
BOOL lossInLossRatio(float lossRatio);

// ������
int main(int argc, char* argv[]) {
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData; //�����洢��WSAStartup�������ú󷵻ص�Windows Sockets���ݡ�������Winsock.dllִ�е����ݡ�
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Socket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		// �Ҳ��� winsock.dll
		printf("WSAStartup failed with error: %d\n", err);
		return -1;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
	}
	else {
		printf("The Winsock 2.2 dll was found okay\n");
	}
	SOCKET sockServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	//�����׽���Ϊ������ģʽ
	int iMode = 1;	//1��������	0������
	ioctlsocket(sockServer, FIONBIO, (u_long FAR*) &iMode); //����������
	SOCKADDR_IN addrServer;	//��������ַ
	//addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY); //���߾���
	addrServer.sin_family = AF_INET; //internetwork: UDP, TCP, etc.
	addrServer.sin_port = htons(SERVER_PORT);
	float packetLossRatio = 0.0; //Ĭ�ϰ���ʧ�� 0.0
	float ackLossRatio = 0.0; //Ĭ�� ACK ��ʧ�� 0.0d
	err = bind(sockServer, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
	if (err) {
		err = GetLastError();
		printf("Could not bind the port %d for socket.Error code is %d\n", SERVER_PORT, err);
		WSACleanup();
		return -1;
	}
	else {
		printf("�󶨶˿� %d �ɹ�\n", SERVER_PORT);
	}

	SOCKADDR_IN addrClient;	//�ͻ��˵�ַ
	int length = sizeof(SOCKADDR);
	//char buffer[BUFFER_LENGTH];	// ���ݷ��ͽ��ջ�����
	ZeroMemory(buffer, sizeof(buffer));

	int recvSize;
	for (int i = 0; i < SEQ_SIZE; ++i) { // ????????????????????????????
		ack[i] = TRUE;
	}
	while (true) {
		//���������գ���û���յ����ݣ��򷵻�ֵΪ-1
		recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&addrClient), &length);
		//printf("here still right %d\n", recvSize);
		if (recvSize < 0) {
			Sleep(200);
			continue;
		}
		printf("recv from client: %s\n", buffer);
		if (strcmp(buffer, "-time") == 0) {
			getCurTime(buffer);
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
		}
		else if (strcmp(buffer, "-quit") == 0) {
			strcpy_s(buffer, strlen("Good bye!") + 1, "Good bye!");
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrClient, sizeof(SOCKADDR));
		}
		else if (strcmp(buffer, "-testgbn") == 0) { // ���� GBN ���Խ׶�
			TestGBN(sockServer, &addrClient, buffer);
		}
		else if (strcmp(buffer, "-upload") == 0) {
			//printf("upload\n");
			Upload(sockServer, &addrClient, packetLossRatio, ackLossRatio, buffer);
		}
		else if (strcmp(buffer, "-download") == 0) {
			//printf("download\n");
			Download(sockServer, &addrClient, buffer);
		}
		else if (strcmp(buffer, "-SR_upload") == 0) {
			printf("SR_upload\n");
			UploadSR(sockServer, &addrClient, packetLossRatio, ackLossRatio, buffer);
		}
		else if (strcmp(buffer, "-SR_download") == 0) {
			//printf("SR_download\n");
			DownloadSR(sockServer, &addrClient, buffer);
		}
		Sleep(100);
	}
	//�ر��׽��֣�ж�ؿ�
	closesocket(sockServer);
	WSACleanup();
	return 0;
}

//****************************************
// Method:	getCurTime
// FullName:	getCurTime
// Access:		public
// Returns:	void
// Qualifier:	��ȡ��ǰϵͳ�¼���������� ptime ��
// Parameter��	char *ptime
//****************************************
void getCurTime(char* ptime) {
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm* p;
	time(&c_time);
	p = localtime(&c_time);
	sprintf_s(buffer, "%d/%d/%d %d:%d:%d",
		p->tm_year + 1900,
		p->tm_mon + 1,
		p->tm_mday,
		p->tm_hour,
		p->tm_min,
		p->tm_sec);
	strcpy_s(ptime, sizeof(buffer), buffer);
}

//****************************************
// Method:	seqIsAvailable
// FullName:	seqIsAvailable
// Access:		public
// Returns:	bool
// Qualifier:	��ǰ���к� curSeq �Ƿ����
//****************************************
bool seqIsAvailable() {
	int step;
	step = curSeq - curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;
	//���к��Ƿ��ڵ�ǰ���ʹ���֮��
	if (step >= SEND_WIND_SIZE) {
		return false;
	}
	if (ack[curSeq]) {
		return true;
	}
	return false;
}

//****************************************
// Method:	timeoutHandler
// FullName:	timeoutHandler
// Access:		public
// Returns:	void
// Qualifier:	��ʱ�ش������������������ڵ�����֡��Ҫ�ش�
//****************************************
void timeoutHandler() {
	printf("Timer out error.\n");
	int index;
	for (int i = 0; i < SEND_WIND_SIZE; ++i) { //�������е����ݼ�¼ˢ��
		index = (i + curAck) % SEQ_SIZE;
		ack[index] = TRUE;
	}
	// �ش������е�ȫ������
	totalSeq -= SEND_WIND_SIZE;
	curSeq = curAck;
}

//****************************************
// Method:	ackHandler
// FullName:	ackHandler
// Access:		public
// Returns:	void
// Qualifier:	�յ� ack ���ۼ�ȷ�ϣ�ȡ����֡�ĵ�һ���ֽ�
//		���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0 ��ASCII)ʱ�ᷢ��ʧ�ܣ�
//		������˼� 1 �����˴���һ��ԭ
// Parameter:	char c
//****************************************
void ackHandler(char c) {
	unsigned char index = (unsigned char)c - 1;	// ���кż�һ
	printf("Recv a ack of %d\n", index);
	if (curAck <= index) {
		for (int i = curAck; i <= index; ++i) { // �ۼ�ȷ�ϣ����յ���ack֮ǰ�����ݱ���Ϊ����������
			ack[i] = TRUE;
		}
		curAck = (index + 1) % SEQ_SIZE;
	}
	else {	//ack ���������ֵ����������ǰ��������һ�����ڣ�ȷ�ϵ�ǰ����ʣ��� ack
			 //����������´��ڵ� index ֮ǰ�� ack ȷ�Ͻ���
		for (int i = curAck; i < SEQ_SIZE; ++i) {
			ack[i] = TRUE;
		}
		for (int i = 0; i <= index; ++i) {
			ack[i] = TRUE;
		}
		curAck = index + 1; //���´����յ� ack ��λ��
	}
}
//****************************************
// Method:	TestGBN
// FullName:	TestGBN
// Access:		public
// Returns:	void
// Qualifier:	ִ�в���GBN���ݴ���ģ��
// Parameter:	SOCKET sockServer
// Parameter:	SOCKADDR_IN *addrClient
// Parameter:	buffer[]
//****************************************
const int read_test_data_size = 13;
void TestGBN(SOCKET sockServer, SOCKADDR_IN *addrClient, char buffer[]) {
	//���� server ��server ���� 0 ״̬���� client ���� 205 ״̬�루server ���� 1 ״̬��
	//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�����ʱ
	
	// ���������ݶ����ڴ�
	std::ifstream icin; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	icin.open("../test.txt");
	char data[1024 * read_test_data_size];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * read_test_data_size);
	icin.close();
	//cout << data << endl;
	totalPacket = sizeof(data) / 1024; //���ݰ�������
	ZeroMemory(buffer, sizeof(buffer));
	int recvSize;
	int waitCount = 0;
	printf("Begin to test GBN protocol, please don't abort the process\n");
	//����һ�����ֽ׶�
	//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루�Լ�����ģ���ʾ������׼�����ˣ����Է�������
	//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
	int length = sizeof(SOCKADDR);
	printf("Shake hands stage\n");
	int stage = 0;
	bool runFlag = true;
	while (runFlag) {
		switch (stage) {
		case 0: //���� 205 �׶�
			buffer[0] = 205;
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			Sleep(100);
			stage = 1;
			break;
		case 1: //�ȴ����� 200 �׶Σ�û���յ�������� +1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
			recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrClient), &length);
			if (recvSize < 0) {
				++waitCount;
				if (waitCount > 20) {
					runFlag = false;
					printf("Timeout error\n");
					break;
				}
				Sleep(100);
				continue;
			}
			else { // �յ��ͻ��˵Ĵ���
				if ((unsigned char)buffer[0] == 200) {
					printf("Begin a file transfer\n");
					printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
					curSeq = 0;
					curAck = 0;
					totalSeq = 0;
					waitCount = 0;
					stage = 2;
				}
			}
			break;
		case 2:	// ���ݴ���׶�
			if (seqIsAvailable() && totalSeq <= totalPacket) {
				//���͸��ͻ��˵����кŴ� 1 ��ʼ
				buffer[0] = curSeq + 1;
				ack[curSeq] = FALSE;
				//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ�����ɣ��˴����ˣ�δʵ�֣�
				memcpy(&buffer[1], data + 1024 * totalSeq, 1024); //ÿ�����ƫ��һ�����ݱ���Ƭ��λ�ã�ֱ�����������ݰ����ͳ�ȥ
				printf("send a packet with a seq of %d\n", curSeq);
				sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
				++curSeq;
				curSeq %= SEQ_SIZE;
				++totalSeq;
				Sleep(100);
			}
			//�ȴ� Ack�� ��û���յ����򷵻�-1��������+1
			recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrClient), &length);
			if (recvSize < 0) {
				waitCount++;
				//20 �εȴ� ack ��ʱ�ش�
				if (waitCount > 20) {
					timeoutHandler();
					waitCount = 0;
				}
			}
			else { // �յ� ack
				ackHandler(buffer[0]);
				if (totalSeq >= totalPacket && (unsigned char)buffer[0] == totalPacket) stage = 3; //���ݴ������ˣ���������
				waitCount = 0;
			}
			Sleep(100);
			break;
		case 3: // ���ݴ������
			printf("Data send over!\n");
			buffer[0] = 255;
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			Sleep(100);
			runFlag = false;
			break;
		}
	}
}

//****************************************
// Method:	Download
// FullName:	Download
// Access:		public
// Returns:	void
// Qualifier:	�ͻ������������ļ�
// Parameter:	SOCKET sockServer
// Parameter:	SOCKADDR_IN *addrClient
// Parameter:	buffer[]
//****************************************
const int download_data_size = 5; //���ķ�����
void Download(SOCKET sockServer, SOCKADDR_IN* addrClient, char buffer[]) {
	//���� server ��server ���� 0 ״̬���� client ���� 205 ״̬�루server ���� 1 ״̬��
	//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�����ʱ

	// ���������ݶ����ڴ�
	std::ifstream icin; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	icin.open("../server_send.txt");
	char data[1024 * download_data_size];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * download_data_size);
	icin.close();

	totalPacket = sizeof(data) / 1024; //���ݰ�������
	ZeroMemory(buffer, sizeof(buffer));
	int recvSize;
	int waitCount = 0;
	printf("Begin to Download from server with GBN protocol, please don't abort the process\n");
	//����һ�����ֽ׶�
	//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루�Լ�����ģ���ʾ������׼�����ˣ����Է�������
	//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
	int length = sizeof(SOCKADDR);
	printf("Shake hands stage\n");
	int stage = 0;
	bool runFlag = true;
	while (runFlag) {
		switch (stage) {
		case 0: //���� 205 �׶�
			buffer[0] = 205;
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			Sleep(100);
			stage = 1;
			break;
		case 1: //�ȴ����� 200 �׶Σ�û���յ�������� +1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
			recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrClient), &length);
			if (recvSize < 0) {
				++waitCount;
				if (waitCount > 20) {
					runFlag = false;
					printf("Timeout error\n");
					break;
				}
				Sleep(100);
				continue;
			}
			else { // �յ��ͻ��˵Ĵ���
				if ((unsigned char)buffer[0] == 200) {
					printf("Begin a file transfer\n");
					printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
					curSeq = 0;
					curAck = 0;
					totalSeq = 0;
					waitCount = 0;
					stage = 2;
				}
			}
			break;
		case 2:	// ���ݴ���׶�
			if (seqIsAvailable() && totalSeq <= totalPacket) {
				//���͸��ͻ��˵����кŴ� 1 ��ʼ
				buffer[0] = curSeq + 1;
				ack[curSeq] = FALSE;
				//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ�����ɣ��˴����ˣ�δʵ�֣�
				memcpy(&buffer[1], data + 1024 * totalSeq, 1024); //ÿ�����ƫ��һ�����ݱ���Ƭ��λ�ã�ֱ�����������ݰ����ͳ�ȥ
				printf("send a packet with a seq of %d\n", curSeq);
				sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
				++curSeq;
				curSeq %= SEQ_SIZE;
				++totalSeq;
				Sleep(100);
			}
			//�ȴ� Ack�� ��û���յ����򷵻�-1��������+1
			recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrClient), &length);
			if (recvSize < 0) {
				waitCount++;
				//20 �εȴ� ack ��ʱ�ش�
				if (waitCount > 20) {
					timeoutHandler();
					waitCount = 0;
				}
			}
			else { // �յ� ack
				ackHandler(buffer[0]);
				if (totalSeq >= totalPacket && (unsigned char)buffer[0] == totalPacket) stage = 3; //���ݴ������ˣ���������
				waitCount = 0;
			}
			Sleep(100);
			break;
		case 3: // ���ݴ������
			printf("Data send over!\n");
			buffer[0] = 255;
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			Sleep(100);
			runFlag = false;
			break;
		}
	}
}

//****************************************
// Method:	SRDownload
// FullName:	SRDownload
// Access:		public
// Returns:	void
// Qualifier:	�ͻ������������ļ���ʹ��SRЭ��
// Parameter:	SOCKET sockServer
// Parameter:	SOCKADDR_IN *addrClient
// Parameter:	buffer[]
//****************************************
void DownloadSR(SOCKET sockServer, SOCKADDR_IN* addrClient, char buffer[]) {
	//���� server ��server ���� 0 ״̬���� client ���� 205 ״̬�루server ���� 1 ״̬��
	//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�����ʱ

	// ���������ݶ����ڴ�
	std::ifstream icin; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	icin.open("../server_send.txt");
	char data[1024 * download_data_size];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * download_data_size);
	icin.close();

	SRWindow send_window; //���ݷ��ʹ���

	totalPacket = sizeof(data) / 1024; //���ݰ�������
	ZeroMemory(buffer, sizeof(buffer));
	int recvSize;
	int waitCount = 0;
	printf("Begin to Download from server with GBN protocol, please don't abort the process\n");
	//����һ�����ֽ׶�
	//���ȷ�������ͻ��˷���һ�� 205 ��С��״̬�루�Լ�����ģ���ʾ������׼�����ˣ����Է�������
	//�ͻ����յ� 205 ֮��ظ�һ�� 200 ��С��״̬�룬��ʾ�ͻ���׼�����ˣ����Խ���������
	int length = sizeof(SOCKADDR);
	printf("Shake hands stage\n");
	int stage = 0;
	bool runFlag = true;
	while (runFlag) {
		switch (stage) {
		case 0: //���� 205 �׶�
			buffer[0] = 205;
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			Sleep(100);
			stage = 1;
			break;
		case 1: //�ȴ����� 200 �׶Σ�û���յ�������� +1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
			recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrClient), &length);
			if (recvSize < 0) {
				++waitCount;
				if (waitCount > 20) {
					runFlag = false;
					printf("Timeout error\n");
					break;
				}
				Sleep(100);
				continue;
			}
			else { // �յ��ͻ��˵Ĵ���
				if ((unsigned char)buffer[0] == 200) {
					printf("Begin a file transfer\n");
					printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
					curSeq = 0;
					curAck = 0;
					send_window.left = 0;
					send_window.right = SEND_WIND_SIZE;
					stage = 2;
				}
			}
			break;
		case 2:	// ���ݴ���׶�
			if (curSeq < totalPacket && curSeq >= send_window.left && curSeq < send_window.right) { //���ݱ��ڷ��ʹ����ڣ�����
				//���͸��ͻ��˵����кŴ� 1 ��ʼ
				buffer[0] = curSeq + 1;
				ack[curSeq] = false; //����һ�����ݱ��󣬿�ʼ��ʱ���յ�ack��ʱ����0����ʱ�ط�
				//���ݷ��͵Ĺ�����Ӧ���ж��Ƿ�����ɣ��˴����ˣ�δʵ�֣�
				memcpy(&buffer[1], data + 1024 * curSeq, 1024); //ÿ�����ƫ��һ�����ݱ���Ƭ��λ�ã�ֱ�����������ݰ����ͳ�ȥ
				printf("send a packet with a seq of %d\n", curSeq);
				sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
				++curSeq;
				Sleep(100);
			}
			else if (ack[send_window.left] == false) { //�������������ack��û���յ������ط�
				buffer[0] = send_window.left + 1;
				memcpy(&buffer[1], data + 1024 * send_window.left, 1024); //ÿ�����ƫ��һ�����ݱ���Ƭ��λ�ã�ֱ�����������ݰ����ͳ�ȥ
				printf("send a packet with a seq of %d\n", send_window.left);
				sendto(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
				Sleep(100);
			}
			//�ȴ� Ack�� ���յ������´���λ��
			recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrClient), &length);
			if (recvSize >= 0) { // �յ� ack
				printf("Recv a ack of %d\n", (unsigned char)buffer[0] - 1);
				ack[(unsigned char)buffer[0] - 1] = true; //���Ϊ�ѽ���ack
				while (send_window.left < curSeq && ack[send_window.left]) { //������ݱ��Ѿ����͹��ˣ����ҽ��յ���ack���򽫴��������ƶ������ʵ�λ��
					send_window.left++;
					send_window.right++;
				}
				if (send_window.left >= totalPacket) stage = 3; //���ݴ������ˣ���������
			}
			Sleep(100);
			break;
		case 3: // ���ݴ������
			printf("Data send over!\n");
			buffer[0] = 255;
			sendto(sockServer, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			Sleep(100);
			runFlag = false;
			break;
		}
	}
}

//****************************************
// Method:	Upload
// FullName:	Upload
// Access:		public
// Returns:	BOOL
// Qualifier:	�ͻ�����������ϴ��ļ�
// Parameter��	SOCKET socketClient
// Parameter��	SOCKADDR_IN addrServer
// Parameter��	float packetLossRatio[0,1] ��������
// Parameter��	float ackLossRatio[0,1] ��ack����
// Parameter��	char buffer[]
//****************************************
const int upload_data_size = 5; //��������
void Upload(SOCKET sockServer, SOCKADDR_IN* addrClient, float packetLossRatio, float ackLossRatio, char buffer[]) {
	int len = sizeof(SOCKADDR);
	printf("%s\n", "Begin to upload from client with GBN protocol, please don't abort the process");
	printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
	int waitCount = 0;
	int stage = 0;
	BOOL b;
	unsigned char u_code; //״̬��
	unsigned short seq;	//�������к�
	unsigned short recvSeq; //���մ��ڴ�СΪ 1����ȷ�ϵ����к�
	unsigned short waitSeq; //�ȴ������к�

	// ���������ݶ����ڴ�
	std::ofstream outfile; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	outfile.open("../server_recv.txt");
	char data[1024 * download_data_size];
	ZeroMemory(buffer, sizeof(buffer));
	//printf("here %s\n", cmd);
	BOOL upload_flag = true;
	while (upload_flag) {
		// �ȴ� �ͻ��� �ظ�����Ϊ UDP ����ģʽ
		//ZeroMemory(buffer, sizeof(buffer));
		int recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrClient, &len);
		if (recvSize < 0) continue;
		if ((unsigned char)buffer[0] == 255) stage = 3; //�յ�255����ʾ�����Ѿ�ȫ���������
		switch (stage) {
		case 0: //�ȴ����ֽ׶�
			u_code = (unsigned char)buffer[0];
			if (u_code == 205) {
				printf("Ready for file upload\n");
				buffer[0] = 200;
				//buffer[1] = '\0';
				sendto(sockServer, buffer, 2, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
				stage = 1;
				recvSeq = 0;
				waitSeq = 1;
			}
			break;
		case 1: //�ȴ��������ݽ׶�
			seq = (unsigned short)buffer[0];
			if (seq > 255) continue;
			//�����ģ����Ƿ�ʧ
			b = lossInLossRatio(packetLossRatio);
			if (b) {
				printf("The packet with seq of %d loss\n", seq);
				continue;
			}
			printf("recv a packet with a seq of %d\n", seq);
			//Sleep(1000);//??????????????????
			if (!(waitSeq - seq)) { //������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
				//cout << "wait ��" << waitSeq << endl;
				memcpy(data + 1024 * (waitSeq - 1), &buffer[1], 1024);
				++waitSeq;
				if (waitSeq == upload_data_size + 1) {
					waitSeq = 1;
				}
				//�������
				//printf("%s\n", buffer[1]);
				buffer[0] = seq;

				recvSeq = seq;
				buffer[1] = '\0';
			}
			else { //�����ǰһ������û���յ�����ȴ� Seq Ϊ 1 �����ݰ��������򲻷��� ACK����Ϊ��û����һ����ȷ�� ACK��
				if (!recvSeq) continue;
				buffer[0] = recvSeq;
				buffer[1] = '\0';
			}
			b = lossInLossRatio(ackLossRatio);
			if (b) {
				printf("The ack of %d loss\n", (unsigned char)buffer[0]);
				continue;
			}
			sendto(sockServer, buffer, 2, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			printf("send a ack of %d\n", (unsigned char)buffer[0]);
			break;
		case 3:
			upload_flag = false;
			outfile.write(data, 1024 * download_data_size);
			outfile.close();
			ZeroMemory(buffer, sizeof(buffer));
			break;
		}
		Sleep(100);
	}
}

//****************************************
// Method:	UploadSR
// FullName:	UploadSR
// Access:		public
// Returns:	void
// Qualifier:	�ͻ��˴ӷ����������ļ�,ʹ�� SR Э��
// Parameter��	SOCKET socketClient
// Parameter��	SOCKADDR_IN addrServer
// Parameter��	float packetLossRatio[0,1] ��������
// Parameter��	float ackLossRatio[0,1] ��ack����
// Parameter��	char buffer[]
//****************************************
void UploadSR(SOCKET sockServer, SOCKADDR_IN* addrClient, float packetLossRatio, float ackLossRatio, char buffer[]) {
	int len = sizeof(SOCKADDR);
	printf("%s\n", "Begin to let client download with GBN protocol, please don't abort the process");
	printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
	int waitCount = 0;
	int stage = 0;
	BOOL b;
	unsigned char u_code; //״̬��
	unsigned short seq;	//�������к�
	BOOL recv_mark[PACKET_NUM_MAX]; //���ĳ����ŵ������Ƿ񱻽���
	memset(recv_mark, false, sizeof(recv_mark)); //��ʼ��Ϊû�����ݱ�������
	SRWindow recv_window; //���ݽ��մ���
	// ���������ݶ����ڴ�
	std::ofstream outfile; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	outfile.open("../server_recv.txt");
	char data[1024 * download_data_size];

	//printf("here %s\n", cmd);
	BOOL download_flag = true;
	while (download_flag) {
		// �ȴ� server �ظ�����Ϊ UDP ����ģʽ
		int recvSize = recvfrom(sockServer, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrClient, &len);
		if (recvSize < 0) continue;
		if ((unsigned char)buffer[0] == 255) stage = 3; //�յ�255����ʾ�����Ѿ�ȫ���������
		switch (stage) {
		case 0: //�ȴ����ֽ׶�
			u_code = (unsigned char)buffer[0];
			if (u_code == 205) {
				printf("Ready for file upload\n");
				buffer[0] = 200;
				buffer[1] = '\0';
				sendto(sockServer, buffer, 2, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
				stage = 1;
				recv_window.left = 0;
				recv_window.right = RECV_WIND_SIZE;
			}
			break;
		case 1: //�ȴ��������ݽ׶�
			seq = (unsigned short)buffer[0];
			//�����ģ����Ƿ�ʧ
			b = lossInLossRatio(packetLossRatio);
			if (b) {
				printf("The packet with seq of %d loss\n", seq);
				continue;
			}
			printf("recv a packet with a seq of %d\n", seq);
			if (seq - 1 >= recv_window.left && seq - 1 < recv_window.right) { //���seq�����ݽ��մ����ڣ�����գ�������ACK
				//cout << "wait ��" << waitSeq << endl;
				memcpy(data + 1024 * (seq - 1), &buffer[1], 1024);
				recv_mark[seq - 1] = true;
				while (recv_mark[recv_window.left]) { //�����ƶ��������������������ݻ�δ���յ���λ��
					recv_window.left++;
					recv_window.right++;
				}
				//�������
				//printf("%s\n", buffer[1]);
				buffer[0] = seq;
				buffer[1] = '\0';
			}
			else if (seq - 1 < recv_window.left) { //����յ��ط����ѽ��չ������ݱ���ֱ�ӷ���ack����
				buffer[0] = seq;
				buffer[1] = '\0';
			}
			b = lossInLossRatio(ackLossRatio);
			if (b) {
				printf("The ack of %d loss\n", (unsigned char)buffer[0]);
				continue;
			}
			sendto(sockServer, buffer, 2, 0, (SOCKADDR*)addrClient, sizeof(SOCKADDR));
			printf("send a ack of %d\n", (unsigned char)buffer[0]);
			Sleep(100);
			break;
		case 3:
			download_flag = false;
			outfile.write(data, 1024 * download_data_size);
			outfile.close();
			ZeroMemory(buffer, sizeof(buffer));
			break;
		}
		Sleep(100);
	}
}

//****************************************
// Method:	lossInLossRatio
// FullName:	lossInLossRatio
// Access:		public
// Returns:	BOOL
// Qualifier:	���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ����ʧ�򷵻� TRUE�����򷵻� FALSE
// Parameter��	float lossRatio[0,1]
//****************************************
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		return TRUE;
	}
	return FALSE;
}