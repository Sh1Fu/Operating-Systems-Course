#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define FILE_LOGGER "msg.txt"
#define MAX_SOCKETS 255
#define TCP_MAX_SIZE 65507
using namespace std;

int s_c = 0;
int cs[MAX_SOCKETS];
pollfd pfd[MAX_SOCKETS + 1];

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
    snprintf(port, 5, "%d", htons(addr->sin_port));
    client.append((char *)ip, strlen(ip));
    client += ':';
    client.append((char *)port, strlen(port));
    return client;
}

int writeToFile(string buffer, string client)
{
    cout << buffer << " " << buffer.size() << endl;
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

    file_desc << string(buffer.begin() + 6, buffer.begin() + 18) << " " << string(buffer.begin() + 18, buffer.end() - 1) << endl;
    file_desc.close();

    if (string(buffer.begin() + 18, buffer.end() - 1) == "stop")
    {
        return -1;
    }
    return 0;
}

void disconnectClient(int index, sockaddr_in *addr)
{
    string client = findClient(addr);
    cout << "  Peer disconnected: " << client << endl;
    cs[index] = -1;
    close(pfd[index].fd);
    pfd[index].fd = -1;
}

static int readPacket(int sock, sockaddr_in *addr, int index)
{
    char *buffer = new char[TCP_MAX_SIZE];
    int r = 0, write = 0;
    string b;
    b.clear();
    bool first_iter = true;
    while ((r = recv(sock, buffer, TCP_MAX_SIZE, 0)) > 0)
    {
        string client = findClient(addr);
        b.append((char *)buffer, r);
        if (first_iter)
        {
            write = writeToFile(string(b.begin(), b.end()), client);
            first_iter = false;
        }
        else
        {
            write = writeToFile(string(b.begin() + 4, b.end()), client);
        }
        int s_status = send(sock, "ok", 2, 0);
        b.clear();
    }
    //disconnectClient(index, addr);
    delete[] buffer;
    return write;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        cout << "Usage: ./tcpserver <port>" << endl;
        return 0;
    }
    int i;
    uint32_t port = atoi(argv[1]);
    sockaddr_in addr;

    unsigned long mode = 1, opt = 1;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    int fl = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, fl | O_NONBLOCK);

    if (s < 0)
    {
        cout << "socket() error function" << endl;
        return 0;
    }
    if (bind(s, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cout << "bind() function error" << endl;
        return 0;
    }
    if (listen(s, 1) < 0)
    {
        cout << "listen() function error" << endl;
        return -1;
    }
    for (size_t i = 0; i < MAX_SOCKETS; i++)
    {
        cs[i] = -1;
    }
    pfd[MAX_SOCKETS].fd = s;
    pfd[MAX_SOCKETS].events = POLLIN;

    cout << "Listening in " << port << endl;
    while (true)
    {
        int res = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), 100);
        if (res > 0)
        {
            for (size_t i = 0; i < s_c; i++)
            {
                if (pfd[i].revents & POLLIN)
                {
                    char *buffer = (char *)malloc(4);
                    int r = recv(pfd[i].fd, buffer, 4, 0);
                    if (strcmp(buffer, "put") == 0)
                        break;
                    else
                    {
                        int r_status = readPacket(pfd[i].fd, &addr, i);
                        if (r_status == -1)
                        {
                            cout << "stop message detected. Powering off..." << endl;
                            for (size_t i = 0; i < s_c; i++)
                            {
                                close(pfd[i].fd);
                            }
                            close(s);
                            exit(0);
                        }
                    }
                }
                if (pfd[i].revents & POLLHUP || pfd[i].revents & POLLERR)
                {
                    disconnectClient(i, &addr);
                }
            }
            if (pfd[MAX_SOCKETS].revents & POLLIN)
            {

                unsigned long mode = 1;
                unsigned int socklen = sizeof(addr);
                int new_connection = accept(pfd[MAX_SOCKETS].fd, (struct sockaddr *)&addr, &socklen);
                string client = findClient(&addr);
                cout << "  Peer connected: " << client << endl;
                if (new_connection < 0)
                {
                    break;
                }
                s_c++;
                for (size_t i = 0; i < s_c; i++)
                {
                    if (cs[i] == -1)
                    {
                        cs[i] = new_connection;
                        pfd[i].fd = new_connection;
                        pfd[i].events = POLLIN | POLLOUT;
                        break;
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < s_c; i++)
    {
        close(pfd[i].fd);
    }
    close(s);
    return 0;
}