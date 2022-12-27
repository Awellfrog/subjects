// GBN_client.cpp���������̨Ӧ�ó������ڵ�
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <iostream>
using namespace std;

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996) 

#define SERVER_PORT	12340 //�������ݵĶ˿ں�
#define SERVER_IP	"127.0.0.1"	//��������IP��ַ

const int BUFFER_LENGTH = 1026;
const int SEND_WIND_SIZE = 4;	//���ʹ��ڵĴ�СΪ10��GBN ��Ӧ���� W+1<=N ��W�����ʹ��ڴ�С��N�����кŸ�����
const int RECV_WIND_SIZE = 4;	//���ݽ��մ��ڵĴ�С
const int SEQ_SIZE = 20; //���ն����кŸ�����Ϊ1~20
const int PACKET_NUM_MAX = 100; //������౻���Ϊ PACKET_NUM_MAX ��

BOOL ack[SEQ_SIZE];	//�յ� ack �������Ӧ 0~19 ��ack
unsigned short curSeq;	//��ǰ���ݰ��� seq
int curAck;	//��ǰ�ȴ�ȷ�ϵ� ack
int totalSeq;		//�յ��İ�������
int totalPacket;	//��Ҫ���͵İ�������

typedef struct SRWindow { //�����շ����ڣ�����ҿ�
	int left;
	int right;

} SRWindow;

void printTips(); //��ӡ����ָ��
BOOL lossInLossRatio(float lossRatio); //�����ʧ
void TestGBN(SOCKET socketClient, SOCKADDR_IN *addrServer, float packetLossRatio, float ackLossRatio, char buffer[]); //����GBN����Э��
void Download(SOCKET socketClient, SOCKADDR_IN* addrServer, float packetLossRatio, float ackLossRatio, char buffer[]);
void Upload(SOCKET socketClient, SOCKADDR_IN* addrServer, char buffer[]);
void DownloadSR(SOCKET socketClient, SOCKADDR_IN* addrServer, float packetLossRatio, float ackLossRatio, char buffer[]);
void UploadSR(SOCKET socketClient, SOCKADDR_IN* addrServer, char buffer[]);
bool seqIsAvailable(); //��ǰ���к��Ƿ���ã����Ƿ���Խ������ݱ���������Ȼ�ڵȴ���һ�������ݱ���
void timeoutHandler(); //��ʱ�ش�
void ackHandler(char c); //�ۼ�ȷ�� ACK

int main(int argc, char* argv[]) {
	// �����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
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
	SOCKET socketClient = socket(AF_INET, SOCK_DGRAM, 0);
	SOCKADDR_IN addrServer;

	addrServer.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	addrServer.sin_family = AF_INET;
	addrServer.sin_port = htons(SERVER_PORT);
	//���ջ�����
	char buffer[BUFFER_LENGTH];
	ZeroMemory(buffer, sizeof(buffer));
	int len = sizeof(SOCKADDR);
	//Ϊ�˲���������������ӣ�����ʹ�� -time ����ӷ������˻�ȡ��ǰʱ��
	//ʹ�� -testgbn[X][Y] ���� GBN ���� [X] ��ʾ���ݰ���ʧ����
	//									[Y] ��ʾ ACK ��������
	//printTips();
	int ret;
	int interval = 1; //�յ����ݰ�֮�󷵻� ack �ļ����Ĭ��Ϊ 1 ��ʾÿ�������� ack��0���߸�������ʾ���еĶ������� ack
	char cmd[128];
	float packetLossRatio = 0.2; //Ĭ�ϰ���ʧ�� 0.2
	float ackLossRatio = 0.2; //Ĭ�� ACK ��ʧ�� 0.2
	//��ʱ����Ϊ������ӣ�����ѭ����������
	srand((unsigned)time(NULL));
	for (int i = 0; i < SEQ_SIZE; ++i) { // ????????????????????????????
		ack[i] = TRUE;
	}
	while (true) {
		printTips();
		gets_s(buffer, 1026);
		ret = sscanf(buffer, "%s%f%f", &cmd, &packetLossRatio, &ackLossRatio);
		
		if (!strcmp(cmd, "-time")) {
			sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
			printf("%s\n", buffer);
		}
		else if (!strcmp(cmd, "-testgbn")) { //��ʼ GBN ���ԣ�ʹ�� GBN Э��ʵ�� UDP �ɿ��ļ�����
			TestGBN(socketClient, &addrServer, packetLossRatio, ackLossRatio, buffer);
			continue;
		}
		else if (!strcmp(cmd, "-upload")) {
			//printf("upload\n");
			Upload(socketClient, &addrServer, buffer);
			continue;
		}
		else if (!strcmp(cmd, "-download")) {
			//printf("download\n");
			Download(socketClient, &addrServer, packetLossRatio, ackLossRatio, buffer);
			continue;
		}
		else if (!strcmp(cmd, "-SR_upload")) {
			printf("SR_upload\n");
			UploadSR(socketClient, &addrServer, buffer);
			continue;
		}
		else if (!strcmp(cmd, "-SR_download")) {
			//printf("SR_download\n");
			DownloadSR(socketClient, &addrServer, packetLossRatio, ackLossRatio, buffer);
			continue;
		}
		else if (!strcmp(cmd, "-quit")) {
			sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)&addrServer, sizeof(SOCKADDR));
			ret = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&addrServer, &len);
			cout << "here: " << buffer << endl;
			printf("%s\n", buffer);
			if (!strcmp(buffer, "Good bye!")) {
				break;
			}
		}
	}
	//�ر��׽���
	closesocket(socketClient);
	WSACleanup();
	return 0;
}

/*******************************************************************/
/*		-time �ӷ�������ȡ��ǰʱ��
		-quit �˳��ͻ���
		-upload �ϴ��ļ�
		-download �����ļ�
		-testgbn[X] ���� GBN Э��ʵ�ֿɿ����ݴ���
					[X][0,1] ģ�����ݰ���ʧ�ĸ���
					[Y][0,1] ģ�� ACK ��ʧ�ĸ���
*/
/*******************************************************************/
void printTips() {
	printf("**********************************************************\n");
	printf("|	-time		to get current time		|\n");
	printf("|	-quit		to exit client			|\n");
	printf("|	-upload		to upload files			|\n");
	printf("|	-download	to download files		|\n");
	printf("|	-SR_upload	to upload with SR		|\n");
	printf("|	-SR_download	to download with SR		|\n");
	printf("|	-testgbn[X][Y]	to test the gbn			|\n");
	printf("**********************************************************\n");
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

//****************************************
// Method:	TestGbn
// FullName:	TestGbn
// Access:		public
// Returns:	BOOL
// Qualifier:	����GBN����Э��ģ�飬��������Ͷ�ACK
// Parameter��	SOCKET socketClient
// Parameter��	SOCKADDR_IN addrServer
// Parameter��	float packetLossRatio[0,1] ��������
// Parameter��	float ackLossRatio[0,1] ��ack����
// Parameter��	char buffer[]
//****************************************
void TestGBN(SOCKET socketClient, SOCKADDR_IN *addrServer, float packetLossRatio, float ackLossRatio, char buffer[]) {
	int len = sizeof(SOCKADDR);
	printf("%s\n", "Begin to test GBN protocol, please don't abort the process");
	printf("The loss ratio of packet is %.2f,the loss ratio of ack is %.2f\n", packetLossRatio, ackLossRatio);
	int waitCount = 0;
	int stage = 0;
	BOOL b;
	unsigned char u_code; //״̬��
	unsigned short seq;	//�������к�
	unsigned short recvSeq; //���մ��ڴ�СΪ 1����ȷ�ϵ����к�
	unsigned short waitSeq; //�ȴ������к�
	sendto(socketClient, "-testgbn", strlen("-testgbn") + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
	//printf("here %s\n", cmd);
	BOOL test_GBN_flag = true;
	while (test_GBN_flag) {
		// �ȴ� server �ظ�����Ϊ UDP ����ģʽ
		recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrServer, &len);
		if ((unsigned char)buffer[0] == 255) stage = 3; //�յ�255����ʾ�����Ѿ�ȫ���������
		switch (stage) {
		case 0: //�ȴ����ֽ׶�
			u_code = (unsigned char)buffer[0];
			if (u_code == 205) {
				printf("Ready for file transmission\n");
				buffer[0] = 200;
				buffer[1] = '\0';
				sendto(socketClient, buffer, 2, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
				stage = 1;
				recvSeq = 0;
				waitSeq = 1;
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

			if (!(waitSeq - seq)) { //������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
				++waitSeq;
				if (waitSeq == 21) {
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
			sendto(socketClient, buffer, 2, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			printf("send a ack of %d\n", (unsigned char)buffer[0]);
			break;
		case 3:
			test_GBN_flag = false;
			ZeroMemory(buffer, sizeof(buffer));
			break;
		}
		Sleep(100);
	}
}

//****************************************
// Method:	Download
// FullName:	Download
// Access:		public
// Returns:	BOOL
// Qualifier:	�ͻ��˴ӷ����������ļ�
// Parameter��	SOCKET socketClient
// Parameter��	SOCKADDR_IN addrServer
// Parameter��	float packetLossRatio[0,1] ��������
// Parameter��	float ackLossRatio[0,1] ��ack����
// Parameter��	char buffer[]
//****************************************
const int download_data_size = 5; //��������
void Download(SOCKET socketClient, SOCKADDR_IN* addrServer, float packetLossRatio, float ackLossRatio, char buffer[]) {
	int len = sizeof(SOCKADDR);
	printf("%s\n", "Begin to let client download with GBN protocol, please don't abort the process");
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
	outfile.open("../client_recv.txt");
	char data[1024 * download_data_size];

	sendto(socketClient, "-download", strlen("-download") + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
	//printf("here %s\n", cmd);
	BOOL download_flag = true;
	while (download_flag) {
		// �ȴ� server �ظ�����Ϊ UDP ����ģʽ
		int recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrServer, &len);
		if (recvSize < 0) continue;
		if ((unsigned char)buffer[0] == 255) stage = 3; //�յ�255����ʾ�����Ѿ�ȫ���������
		switch (stage) {
		case 0: //�ȴ����ֽ׶�
			u_code = (unsigned char)buffer[0];
			if (u_code == 205) {
				printf("Ready for file download\n");
				buffer[0] = 200;
				buffer[1] = '\0';
				sendto(socketClient, buffer, 2, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
				stage = 1;
				recvSeq = 0;
				waitSeq = 1;
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

			if (!(waitSeq - seq)) { //������ڴ��İ�����ȷ���գ�����ȷ�ϼ���
				//cout << "wait ��" << waitSeq << endl;
				memcpy(data + 1024 * (waitSeq - 1), &buffer[1], 1024);
				++waitSeq;
				if (waitSeq == download_data_size + 1) {
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
			sendto(socketClient, buffer, 2, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			printf("send a ack of %d\n", (unsigned char)buffer[0]);
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
// Method:	DownloadSR
// FullName:	DownloadSR
// Access:		public
// Returns:	void
// Qualifier:	�ͻ��˴ӷ����������ļ�,ʹ�� SR Э��
// Parameter��	SOCKET socketClient
// Parameter��	SOCKADDR_IN addrServer
// Parameter��	float packetLossRatio[0,1] ��������
// Parameter��	float ackLossRatio[0,1] ��ack����
// Parameter��	char buffer[]
//****************************************
void DownloadSR(SOCKET socketClient, SOCKADDR_IN* addrServer, float packetLossRatio, float ackLossRatio, char buffer[]) {
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
	outfile.open("../client_recv.txt");
	char data[1024 * download_data_size];

	sendto(socketClient, "-SR_download", strlen("-SR_download") + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
	//printf("here %s\n", cmd);
	BOOL download_flag = true;
	while (download_flag) {
		// �ȴ� server �ظ�����Ϊ UDP ����ģʽ
		int recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrServer, &len);
		if (recvSize < 0) continue;
		if ((unsigned char)buffer[0] == 255) stage = 3; //�յ�255����ʾ�����Ѿ�ȫ���������
		switch (stage) {
		case 0: //�ȴ����ֽ׶�
			u_code = (unsigned char)buffer[0];
			if (u_code == 205) {
				printf("Ready for file download\n");
				buffer[0] = 200;
				buffer[1] = '\0';
				sendto(socketClient, buffer, 2, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
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
			sendto(socketClient, buffer, 2, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			printf("send a ack of %d\n", (unsigned char)buffer[0]);
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
// Method:	Upload
// FullName:	Upload
// Access:		public
// Returns:	void
// Qualifier:	�ͻ��������ϴ��ļ�
// Parameter:	SOCKET sockServer
// Parameter:	SOCKADDR_IN *addrClient
// Parameter:	buffer[]
//****************************************
const int upload_data_size = 5; //���ķ�����
void Upload(SOCKET socketClient, SOCKADDR_IN* addrServer, char buffer[]) {
	//���� server ��server ���� 0 ״̬���� client ���� 205 ״̬�루server ���� 1 ״̬��
	//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�����ʱ

	// ���������ݶ����ڴ�
	std::ifstream icin; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	icin.open("../client_send.txt");
	char data[1024 * upload_data_size];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * upload_data_size);
	icin.close();
	//cout << "neirong: " << data << endl;
	sendto(socketClient, "-upload", strlen("-upload") + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));

	totalPacket = sizeof(data) / 1024; //���ݰ�������
	ZeroMemory(buffer, sizeof(buffer));
	int recvSize;
	int waitCount = 0;
	printf("Begin to upload to server with GBN protocol, please don't abort the process\n");
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
			sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			Sleep(100);
			stage = 1;
			break;
		case 1: //�ȴ����� 200 �׶Σ�û���յ�������� +1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
			recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrServer), &length);
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
					//printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), totalPacket);
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
				sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
				++curSeq;
				curSeq %= SEQ_SIZE;
				++totalSeq;
				Sleep(100);
			}
			//�ȴ� Ack�� ��û���յ����򷵻�-1��������+1
			recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrServer), &length);
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
			sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			Sleep(100);
			runFlag = false;
			break;
		}
	}
}

//****************************************
// Method:	SRUpload
// FullName:	SRUpload
// Access:		public
// Returns:	void
// Qualifier:	�ͻ�����������ϴ��ļ���ʹ��SRЭ��
// Parameter:	SOCKET sockServer
// Parameter:	SOCKADDR_IN *addrClient
// Parameter:	buffer[]
//****************************************
void UploadSR(SOCKET socketClient, SOCKADDR_IN* addrServer, char buffer[]) {
	//���� server ��server ���� 0 ״̬���� client ���� 205 ״̬�루server ���� 1 ״̬��
	//server �ȴ� client �ظ� 200 ״̬�룬����յ���server ���� 2 ״̬������ʼ�����ļ���������ʱ�ȴ�����ʱ

	// ���������ݶ����ڴ�
	std::ifstream icin; //��Ӳ�̵��ڴ棬��ʵ��ν������������ڴ�ռ�;
	icin.open("../client_send.txt");
	char data[1024 * download_data_size];
	ZeroMemory(data, sizeof(data));
	icin.read(data, 1024 * download_data_size);
	icin.close();
	sendto(socketClient, "-SR_upload", strlen("-SR_upload") + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
	SRWindow send_window; //���ݷ��ʹ���

	totalPacket = sizeof(data) / 1024; //���ݰ�������
	ZeroMemory(buffer, sizeof(buffer));
	int recvSize;
	int waitCount = 0;
	printf("Begin to upload to server with GBN protocol, please don't abort the process\n");
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
			sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			Sleep(100);
			stage = 1;
			break;
		case 1: //�ȴ����� 200 �׶Σ�û���յ�������� +1����ʱ������˴Ρ����ӡ����ȴ��ӵ�һ����ʼ
			recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrServer), &length);
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
				sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
				++curSeq;
				Sleep(100);
			}
			else if (ack[send_window.left] == false) { //�������������ack��û���յ������ط�
				buffer[0] = send_window.left + 1;
				memcpy(&buffer[1], data + 1024 * send_window.left, 1024); //ÿ�����ƫ��һ�����ݱ���Ƭ��λ�ã�ֱ�����������ݰ����ͳ�ȥ
				printf("send a packet with a seq of %d\n", send_window.left);
				sendto(socketClient, buffer, BUFFER_LENGTH, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
				Sleep(100);
			}
			//�ȴ� Ack�� ��û���յ����򷵻�-1��������+1
			recvSize = recvfrom(socketClient, buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)addrServer), &length);
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
			sendto(socketClient, buffer, strlen(buffer) + 1, 0, (SOCKADDR*)addrServer, sizeof(SOCKADDR));
			Sleep(100);
			runFlag = false;
			break;
		}
	}
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