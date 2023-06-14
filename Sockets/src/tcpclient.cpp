#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winspool.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <string>
#pragma comment(lib, "ws2_32.lib")
#define TCP_MAX_SIZE 65507
using namespace std;

unsigned int counter = 0;
const int N = 255;//64
int stp = 0xFF;
char* argv1 = NULL;

union
{
	char buffer[4];
	unsigned long integer;
} short_ints;


int writeToFile(string buffer, struct sockaddr_in addr)
{
	cout << buffer << " " << buffer.size() << endl;
	//buffer += 4;
	ofstream file_desc;
	file_desc.open("msg.txt", ios::app);
	//uint16_t date, month, year = 0;


	unsigned int num = 0, date = 0, time = 0, time2 = 0, len = 0;
	unsigned int output_date[3], output_time[3], output_time2[3];
	short_ints.buffer[0] = buffer[0];
	short_ints.buffer[1] = buffer[1];
	short_ints.buffer[2] = buffer[2];
	short_ints.buffer[3] = buffer[3];
	date = short_ints.integer;
	cout << "date: " << date << endl;
	date = ntohl(date);
	cout << "date: " << date << endl;
	//memcpy(date, myBuffer + 4, 4);
	//date = (unsigned int)ntohl(date);
	//cout << date << endl;
	//memset(date, 0, 5);
	output_date[2] = date / 10000;
	output_date[1] = date / 100 % 100;
	output_date[0] = date % 100;
	//memset(time, 0, 5);
	//recv(s, (char*)time, 4, 0);
	short_ints.buffer[0] = buffer[4];
	short_ints.buffer[1] = buffer[5];
	short_ints.buffer[2] = buffer[6];
	short_ints.buffer[3] = buffer[7];
	time = short_ints.integer;
	//time = ntohl(time);
	cout << "time: " << time << endl;
	time = ntohl(time);
	cout << "time: " << time << endl;
	//time = ntohl(time);
	//memset(time, 0, 5);
	output_time[0] = time / 10000;
	output_time[1] = time / 100 % 100;
	output_time[2] = time % 100;

	//memset(time2, 0, 5);
	//recv(s, (char*)time2, 4, 0);
	short_ints.buffer[0] = buffer[8];
	short_ints.buffer[1] = buffer[9];
	short_ints.buffer[2] = buffer[10];
	short_ints.buffer[3] = buffer[11];
	time2 = short_ints.integer;
	//time2 = ntohl(time2);
	cout << "time2: " << time2 << endl;
	time2 = ntohl(time2);
	cout << "time: " << time2 << endl;
	//memset(time2, 0, 5);
	output_time2[0] = time2 / 10000;
	output_time2[1] = time2 / 100 % 100;
	output_time2[2] = time2 % 100;
	//recv(s, (char*)len, 4, 0);

	file_desc << (addr.sin_addr.s_addr & 0xff) << '.' << ((addr.sin_addr.s_addr >> 8) & 0xff) << '.' << ((addr.sin_addr.s_addr >> 16) & 0xff) << '.'
		<< ((addr.sin_addr.s_addr >> 24) & 0xff) << ':' << addr.sin_port;
	file_desc << ' ' << setfill('0') << setw(2) << output_date[0] << '.' << setfill('0') << setw(2) << output_date[1] << '.' << output_date[2] << ' ' 
		<< setfill('0') << setw(2) << output_time[0] << ':' << setfill('0') << setw(2) << output_time[1] << ':' << setfill('0') << setw(2) << output_time[2] << ' ' 
		<< setfill('0') << setw(2) << output_time2[0] << ':' << setfill('0') << setw(2) << output_time2[1] << ':' << setfill('0') << setw(2) << output_time2[2] << ' '
		<< string(buffer.begin() + 16, buffer.end()) << endl;
	file_desc.close();

	if (string(buffer.begin() + 16, buffer.end()) == "stop")
	{
		return -1;
	}
	return 0;
}

static int readPacket(int sock, sockaddr_in* addr)
{
	char* buffer = new char[TCP_MAX_SIZE];
	int r = 0, write = 0;
	string b;
	b.clear();
	int flag = 0;
	while ((r = recv(sock, buffer, TCP_MAX_SIZE, 0)) > 0 && write!=-1)
	{
		int s_status = send(sock, "ok", 2, 0);
		cout << "r: " << r << endl;
		b.append((char*)buffer, r);
		if (!flag) {
			write = writeToFile(string(b.begin() + 4, b.end()), *addr);
			flag = 1;
		}
		else {
			write = writeToFile(string(b.begin() + 4, b.end()), *addr);
		}
		counter++;
		b.clear();
	}
	for (int i = 0; i <= r; i++) {
		buffer[i] = '\0';
	}
	//disconnectClient(index, addr);
	delete[] buffer;
	return write;
}

int main(int argc, char* argv[])
{
	// ˜˜˜ Windows ˜˜˜˜˜˜˜ ˜˜˜˜˜˜˜ WSAStartup ˜˜˜˜˜ ˜˜˜˜˜˜˜ ˜˜˜˜˜˜˜˜˜˜˜˜˜ ˜˜˜˜˜˜˜
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);

	int ls = socket(AF_INET, SOCK_STREAM, 0);
	if (ls < 0)
		return -1;

	unsigned long mode = 1;
	ioctlsocket(ls, FIONBIO, &mode);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int socketLast = 0;
	int cs[N] = { 0 };
	struct pollfd pfd[N + 1];
	for (int i = 0; i < N; i++)
	{
		pfd[i].fd = cs[i];
		pfd[i].events = POLLIN | POLLOUT;
	}
	pfd[N].fd = ls;
	pfd[N].events = POLLIN;
	if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		return -1;
	if (listen(ls, 1) < 0)
		return -1;

	while (stp)
	{
		int ev_cnt = WSAPoll(pfd, sizeof(pfd) / sizeof(pfd[0]), 1000);
		if (ev_cnt <= 0)
			continue;
		for (int i = 0; i < socketLast; i++)
		{
			if (pfd[i].revents & POLLHUP || pfd[i].revents & POLLERR)
				closesocket(pfd[i].fd);
			if (pfd[i].revents & POLLIN) {
				char* buffer = (char*)malloc(4);
				int r = recv(pfd[i].fd, buffer, 3, 0);
				if (strcmp(buffer, "put") == 0) {
					//r = recv(pfd[i].fd, buffer, 3, 0);
					break;
				}
					//break;
				else
				{
					int r_status = readPacket(pfd[i].fd, &addr);
					if (r_status==-1) {
						cout << "stop message detected. Powering off..." << endl;
						cout << "Counter: " << counter << endl;
						for (size_t i = 0; i < socketLast; i++)
						{
							//int s_status = send(pfd[i].fd, "ok", 2, 0);
							closesocket(pfd[i].fd);
						}
						closesocket(ls);
						exit(0);
					}
				}
			}
			//recvit(cs[i], addr);
		}
		if (pfd[N].revents & POLLIN)
		{
			int addrlen = sizeof(addr);
			int new_s = accept(ls, (struct sockaddr*)&addr, &addrlen);
			if (new_s < 0)
				break;
			unsigned long mode = 1;
			ioctlsocket(new_s, FIONBIO, &mode);
			cs[socketLast] = new_s;
			pfd[socketLast].fd = new_s;
			socketLast++;
		}
	}
	for (int i = 0; i < socketLast; i++)
		closesocket(cs[i]);
	closesocket(ls);
	WSACleanup();
	return 0;
}