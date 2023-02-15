//---------------------------------
//	OSC
//---------------------------------
#include "OscReceivedElements.h"
#include "OscPacketListener.h"
#include "OscOutboundPacketStream.h"
#include "UdpSocket.h"
#include "IpEndpointName.h"
#include "iostream"

#include "math.h"

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h> //windows

// ベクター型
typedef struct Vector3_t {
	double x,y,z;
	double absolute() { return sqrt(x * x + y * y + z * z); } // ベクトルの長さ
	
	Vector3_t normalize() {
		double abs = absolute();
		return Vector3_t{ x, y, z } / abs;
	}
	
	Vector3_t operator - (Vector3_t obj) {
		return Vector3_t{ x - obj.x, y - obj.y, z - obj.z, };
	}

	Vector3_t operator / (double obj) {
		return Vector3_t{ x / obj, y / obj, z / obj };
	}

	Vector3_t operator * (double obj) {
		return Vector3_t{ x * obj, y * obj, z * obj };
	}

	void operator = (Vector3_t obj) {
		x = obj.x; y = obj.y; z = obj.z;
	}
};

typedef struct hand_t{
	Vector3_t position = { 0, 0, 0 }; // 手の位置
	Vector3_t rotationAxis = {0, 0, 0}; // 手全体の傾き(クォータニオン), x, y, z
	double rotateSita = 0; // 回転角度(cos(sita))
	double thumb = 0; // 親指
	double fitrstFinger = 0; // 人差し指
	double secondFinger = 0; // 中指
	double thirdFinger = 0; // 薬指
	double pinkyFinger = 0; //小指
};

const int ONEHAND_LANDMARKS_NUM = 22;
const float pi = 3.141592654;
const Vector3_t zero = { 0,0,0 };

void sendOSCMessage(int id, double x, double y, double z);

int main(void) {
	std::cout << sizeof(Vector3_t) << std::endl;

	sendOSCMessage(10, 100, 10, 1);
	return 0;
}

// 行列式を求める関数
// | a.x, a.y, a.z |
// | b.x, b.y, b.z |
// | c.x, c.y, c.z |
double determinant4vec3(Vector3_t a, Vector3_t b, Vector3_t c) {
	return a.x * (b.y * c.z - b.z * c.y) - b.x * (a.y * c.z - a.z * c.y) + c.x * (a.y * b.z - a.z * b.y);
}

// 外積を求める公式
Vector3_t crossProduct4vec3(Vector3_t a, Vector3_t b) {
	return Vector3_t{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

// 内積を求める公式
double dot4vec3(Vector3_t a, Vector3_t b) {
	return  a.x * b.x + a.y * b.y + a.z * b.z;
}

void sendOSCMessage(int id, double x, double y, double z)
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

	Vector3_t handsLandmarks[ONEHAND_LANDMARKS_NUM * 2];
	Vector3_t *handsLandmarks_R = &handsLandmarks[0];
	Vector3_t *handsLandmarks_L = &handsLandmarks[ONEHAND_LANDMARKS_NUM];
	memset((char*)handsLandmarks, 0, sizeof(Vector3_t) * ONEHAND_LANDMARKS_NUM * 2);

	// 手の甲の法線を求める
	Vector3_t rhand_normal, lhand_normal;
	Vector3_t base_rhand_normal = {0.485473,-0.436747,0.757343}, base_lhand_normal = {0.111741,0.405115,-0.907411};

	while (1) {
		// 受信
		recv(sock, (char*)handsLandmarks, sizeof(Vector3_t) * ONEHAND_LANDMARKS_NUM * 2, 0);

		// クオータニオンを求める
		hand_t hand_R, hand_L;
		{

			//2転換のベクトルから外積で法線を求める
			Vector3_t O, A, B;
			O = Vector3_t{ handsLandmarks_R[0].x, handsLandmarks_R[0].y, handsLandmarks_R[0].z };
			A = Vector3_t{ handsLandmarks_R[5].x, handsLandmarks_R[5].y, handsLandmarks_R[5].z };
			B = Vector3_t{ handsLandmarks_R[17].x, handsLandmarks_R[17].y, handsLandmarks_R[17].z };
			rhand_normal = crossProduct4vec3(A-O, B-O);

			O = Vector3_t{ handsLandmarks_L[0].x, handsLandmarks_L[0].y, handsLandmarks_L[0].z };
			A = Vector3_t{ handsLandmarks_L[5].x, handsLandmarks_L[5].y, handsLandmarks_L[5].z };
			B = Vector3_t{ handsLandmarks_L[17].x, handsLandmarks_L[17].y, handsLandmarks_L[17].z };
			lhand_normal = crossProduct4vec3(A - O, B - O);

			printf("(%4lf,%4lf,%4lf)\n(%4lf,%4lf,%4lf)\n(%4lf,%4lf,%4lf)\n(%4lf,%4lf,%4lf)\n",
				handsLandmarks_R[0].x, handsLandmarks_R[0].y, handsLandmarks_R[0].z,
				handsLandmarks_R[17].x, handsLandmarks_R[17].y, handsLandmarks_R[17].z,
				handsLandmarks_R[5].x, handsLandmarks_R[5].y, handsLandmarks_R[5].z,
				lhand_normal.normalize().x, lhand_normal.normalize().y, lhand_normal.normalize().z);

			// 回転の方向ベクトルの計算
			hand_R.rotationAxis = Vector3_t{ 1, 1, -(rhand_normal.x + rhand_normal.y) / rhand_normal.z }.normalize();
			hand_L.rotationAxis = Vector3_t{ 1, 1, -(lhand_normal.x + lhand_normal.y) / lhand_normal.z }.normalize();
			printf("(%4lf,%4lf,%4lf)\n", hand_R.rotationAxis.x, hand_R.rotationAxis.y, hand_R.rotationAxis.z);

			// 回転角度の計算。前回の角度との内積
			double sita_R = -std::acos(dot4vec3(rhand_normal, base_rhand_normal) / (rhand_normal.absolute() * base_rhand_normal.absolute()));
			double sita_L = std::acos(dot4vec3(lhand_normal, base_lhand_normal) / (lhand_normal.absolute() * base_lhand_normal.absolute()));
			printf("R:%4lf L:%4lf\n", sita_R / 3.14 * 180, sita_L / 3.14 * 180);

			// 回転の方向ベクトルの長さを回転角度に置き換え
			double sin_r = std::sin(sita_R/2);
			double cos_r = std::cos(sita_R/2);
			hand_R.rotationAxis = hand_R.rotationAxis * sin_r;
			hand_R.rotateSita = cos_r;


			double sin_l = std::sin(sita_L/2);
			double cos_l = std::cos(sita_L/2);
			hand_L.rotationAxis = hand_L.rotationAxis * sin_l;
			hand_L.rotateSita = cos_l;			
		}

		// デバック表示

		std::cout << "(" << hand_R.rotationAxis.x << "," << hand_R.rotationAxis.y << "," << hand_R.rotationAxis.z << "," << hand_R.rotateSita << "),(" << hand_L.rotationAxis.x << "," << hand_L.rotationAxis.y << "," << hand_L.rotationAxis.z << "," << hand_L.rotateSita << ")" << std::endl;

		// 手の位置を求める



		// Set IPAddress and Port
		const std::string ipAddress = "127.0.0.1";
		const int port = 39570;

		UdpTransmitSocket transmitSocket(IpEndpointName(ipAddress.c_str(), port));
		//Buffer
		char buffer[6144];

		osc::OutboundPacketStream p(buffer, 6144);
		p << osc::BeginBundleImmediate
			//Head
			<< osc::BeginMessage("/VMT/Room/Unity") << 3 << 3 << 0.0f << (float)handsLandmarks_R[ONEHAND_LANDMARKS_NUM - 1].x * 2 - 1 << 2 - (float)handsLandmarks_R[ONEHAND_LANDMARKS_NUM - 1].y * 2 << 0.3f << (float)hand_R.rotationAxis.x << (float)hand_R.rotationAxis.y << (float)hand_R.rotationAxis.y << (float)hand_R.rotateSita << osc::EndMessage
			<< osc::BeginMessage("/VMT/Room/Unity") << 2 << 2 << 0.0f << (float)handsLandmarks_L[ONEHAND_LANDMARKS_NUM - 1].x * 2 - 1 << 2 - (float)handsLandmarks_L[ONEHAND_LANDMARKS_NUM - 1].y * 2 << 0.3f << (float)hand_L.rotationAxis.x << (float)hand_L.rotationAxis.y << (float)hand_L.rotationAxis.y << (float)hand_L.rotateSita << osc::EndMessage
			<< osc::EndBundle;
		transmitSocket.Send(p.Data(), p.Size());
		Sleep(1000 / 30);
	}
}

