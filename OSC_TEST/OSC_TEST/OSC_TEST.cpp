//---------------------------------
//	OSC
//---------------------------------
#include "OscReceivedElements.h"
#include "OscPacketListener.h"
#include "OscOutboundPacketStream.h"
#include "UdpSocket.h"
#include "IpEndpointName.h"
#include "iostream"

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h> //windows

void sendOSCMessage(int id, float x, float y, float z);

int main(void) {
	std::cout << "HELLO OSC" << std::endl;
	sendOSCMessage(10, 100, 10, 1);
	return 0;
}


void sendOSCMessage(int id, float x, float y, float z)
{

	WSAData wsaData;
	WSAStartup(MAKEWORD(2, 0), &wsaData);   //MAKEWORD(2, 0)はwinsockのバージョン2.0ってこと
	int sock = socket(AF_INET, SOCK_DGRAM, 0);  //AF_INETはIPv4、SOCK_DGRAMはUDP通信、0は？

	sock = socket(AF_INET, SOCK_DGRAM, 0);  //AF_INETはIPv4、SOCK_DGRAMはUDP通信、0は？

	// アドレス等格納
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;  //IPv4
	addr.sin_port = htons(50008);   //通信ポート番号設定
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.2"); // INADDR_ANYはすべてのアドレスからのパケットを受信する
	bind(sock, (struct sockaddr *)&addr, sizeof(addr));

	while (1) {
		char buff[512];
		memset(buff, 0, sizeof(buff));
		recv(sock, buff, sizeof(buff), 0);
		float H0_x, H0_y, H0_z;
		float H1_x, H1_y, H1_z;
		sscanf_s(buff, "%f %f %f %f %f %f", &H0_x, &H0_y, &H0_z, &H1_x, &H1_y, &H1_z, (unsigned)sizeof(buff));
		std::cout << buff << std::endl;
		std::cout << "(" << H0_x << "," << H0_y << "," << H0_z << "),(" << H1_x << "," << H1_y << "," << H1_z << ")" << std::endl;

		// Set IPAddress and Port
		const std::string ipAddress = "127.0.0.1";
		const int port = 39570;

		UdpTransmitSocket transmitSocket(IpEndpointName(ipAddress.c_str(), port));
		//Buffer
		char buffer[6144];
		osc::OutboundPacketStream p(buffer, 6144);
		p << osc::BeginBundleImmediate
			//Head
			<< osc::BeginMessage("/VMT/Room/Unity") << 2 << 2 << 0.0f << H0_x*2-1 << 2-H0_y*2 << 0.3f << 0.0f << 0.0f << 0.0f << 0.0f << osc::EndMessage
			<< osc::BeginMessage("/VMT/Room/Unity") << 1 << 1 << 0.0f << H1_x * 2 - 1 << 2 - H1_y * 2 << 0.3f << 0.0f << 0.0f << 0.0f << 0.0f << osc::EndMessage
			<< osc::EndBundle;
		transmitSocket.Send(p.Data(), p.Size());
		Sleep(1000 / 30);
	}
}

