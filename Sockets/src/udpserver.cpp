#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <set>
#include <iomanip>
#include <string.h>
#include <stdint.h>
#pragma comment(lib, "ws2_32.lib")

#define USHORT_MAX 0xffff
#define UDP_MAX_LEN 65507
#define FILE_LOGGER "msg.txt"
using namespace std;

map<string, set<uint32_t, greater<uint32_t>>> clients;
SOCKET *sockets_desc = new SOCKET[100];
struct sockaddr_in *addr = new struct sockaddr_in[100];

uint16_t last_index;
bool stop_hook = false;

union
{
    uint32_t integer;
    char buffer[4];
} convert;

union
{
    char buffer[2];
    uint16_t integer;
    int16_t ninteger;
} short_ints;

string findClient(struct sockaddr_in *addr)
{
    char ip[16];
    char port[5];
    string client;
    inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));
    client.clear();
    _itoa(htons(addr->sin_port), port, 10);
    client.append((char *)ip, strlen(ip));
    client += ':';
    client.append((char *)port, strlen(port));
    return client;
}

int writeToFile(string buffer, string client)
{
    ofstream file_desc;
    file_desc.open(FILE_LOGGER, ios::app);
    uint16_t day, month, year = 0;
    day = buffer[0];
    month = buffer[1];
    short_ints.buffer[0] = buffer[3];
    short_ints.buffer[1] = buffer[2];
    year = short_ints.integer;
    file_desc << client << " ";
    file_desc << setw(2) << setfill('0') << day << '.' << setw(2) << setfill('0') << month << '.' << year << " ";

    short_ints.buffer[0] = buffer[5];
    short_ints.buffer[1] = buffer[4];
    int16_t AA = short_ints.ninteger;
    file_desc << AA << " ";
    file_desc << string(buffer.begin() + 6, buffer.begin() + 18) << " " << string(buffer.begin() + 18, buffer.end() -1) << endl;
    file_desc.close();
    if (string(buffer.begin() + 18, buffer.end() -1) == "stop")
    {
        return -1;
    }
    return 0;
}

int analyseDatagram(SOCKET sock, int index)
{
    char *buf_input = new char[UDP_MAX_LEN];
    socklen_t addrLen = sizeof(addr[index]);
    int status = recvfrom(sock, buf_input, UDP_MAX_LEN, 0, (struct sockaddr *)&addr[index], &addrLen);
    char num_bytes[4];
    int number = 0, result = 0, i;
    string buf;
    string client;
    buf.append((char *)buf_input, status);
    for (i = 0; i < 3; i++)
    {
        number += buf_input[i] & 0xff;
        number = (number << 8);
    }
    number += buf_input[i] & 0xff;
    if (status > 0)
    {
        set<uint32_t, greater<uint32_t>> msgs;
        client = findClient(&addr[index]);
        if (clients.find(client) == clients.end())
        {
            cout << "New client connected: " << client << endl;
            clients.insert(make_pair(client, msgs));
        }
        if (clients[client].find(number) == clients[client].end())
        {
            clients[client].insert(number);
            result = writeToFile(string(buf.begin() + 4, buf.end()), client);
        }
    }
    buf.clear();
    delete[] buf_input;
    return (result == -1 ? -1 : 0);
}

void configListeners(uint16_t port_start, uint16_t port_end)
{
    uint16_t ports_count = 0;
    uint16_t step;
    if (port_start > port_end)
    {
        uint16_t tmp = port_start;
        port_start = port_end;
        port_end = tmp;
    }
    ports_count = port_end - port_start;
	if (port_end == port_start) 
	{
		ports_count = 1;
	}
    cout << "Listening on ports: ";
    for (step = 0; step <= ports_count; step++)
    {
        unsigned long mode = 1;
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            cout << "socket() func error " << WSAGetLastError() << endl;
            return;
        }
        sockets_desc[step] = s;
        ioctlsocket(sockets_desc[step], FIONBIO, &mode);
        memset(&addr[step], 0, sizeof(addr[step]));
        addr[step].sin_family = AF_INET;
        addr[step].sin_addr.s_addr = htonl(INADDR_ANY);
        addr[step].sin_port = htons(port_start + step);
        if (bind(sockets_desc[step], (struct sockaddr *)&addr[step], sizeof(addr[step])) < 0)
        {
            cout << "bind() func error " << WSAGetLastError() << endl;
            return;
        }
        cout << port_start + step << ' ';
    }
    cout << endl;
}

void sendToClient(SOCKET s, int index)
{
    string client = findClient(&addr[index]);
    char *buffer = new char[80];
    memset(buffer, 0, 80);
    int i = 0;
    for (auto it = clients[client].begin(); it != clients[client].end(); it++)
    {
        convert.integer = htonl(*it);
        for (int j = 0; j < 4; j++)
        {
            buffer[i] = convert.buffer[j];
            i++;
        }
    }
    sendto(s, buffer, clients[client].size() * 4, 0, (struct sockaddr *)&addr[index], sizeof(addr[index]));
    delete[] buffer;
}

void disconnectClient(int index)
{
    string client = findClient(&addr[index]);
    clients.erase(client);
    cout << "Old client disconnected: " << client << endl;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cout << "Usage: ./udpserver <port1> <port2>" << endl;
        return 0;
    }
    WSADATA ws;
    WSAStartup(MAKEWORD(2, 2), &ws);
    uint16_t port1, port2, ports_count;
    port1 = atoi(argv[1]);
    port2 = atoi(argv[2]);
    ports_count = port2 - port1;
    configListeners(port1, port2);
    WSAEVENT *event = new WSAEVENT[ports_count];
    for (int i = 0; i < ports_count; i++)
    {
		event[i] = WSACreateEvent();
        WSAEventSelect(sockets_desc[i], event[i], FD_READ | FD_WRITE | FD_CLOSE);
    }
    while (true)
    {
		WSANETWORKEVENTS ne;
        DWORD dw = WSAWaitForMultipleEvents(ports_count, event, FALSE, 1000, FALSE);
		DWORD res = 0;
        for (int i = 0; i < ports_count; i++)
        {
            if ((res = WSAEnumNetworkEvents(sockets_desc[i], event[i], &ne)) == 0)
            {
                if (ne.lNetworkEvents & FD_READ)
                {
                    int value = analyseDatagram(sockets_desc[i], i);
                    sendToClient(sockets_desc[i], i);
                    if (value == -1)
                    {
                        cout << "stop message detected. Power off...." << endl;
						for (int i = 0; i < ports_count; i++) 
						{
							WSACloseEvent(event[i]);
							closesocket(sockets_desc[i]);
						}

                        WSACleanup();
                        delete[] sockets_desc;
                        return 0;
                    }
                }
                if (ne.lNetworkEvents & FD_CLOSE)
                {
                    disconnectClient(i);
                }
            }
        }
    }
    return 0;
}