#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <iomanip>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdint.h>

#define MAX_BODY_SIZE 65507
#define MAX_CONNECTIONS 65535
using namespace std;

map<uint32_t, string> allMsgs;
set<uint32_t> msgArchive;

set<uint32_t> recvData(int s, int sendCount);
string datagramCreate(uint32_t index);
static int sendData(int s, sockaddr_in *addr, set<uint32_t> msgNums);
void readFile(char *file_name);

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        cout << "Usage: ./udpclient IP:PORT FILENAME" << endl;
        return 0;
    }
    char *ip = strtok(argv[1], ":");
    char *port = strtok(NULL, "\0");
    cout << ip << " " << port << endl;
    sockaddr_in addr;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(atoi(port));

    readFile(argv[2]);
    set <uint32_t> startMsgs;
    set <uint32_t> memSave;
    for (auto it = allMsgs.begin(); it != allMsgs.end(); it++)
    {
        startMsgs.insert(it->first);
    }
    int sendCount = 0;
    sendCount = sendData(s, &addr, startMsgs);
    memSave = startMsgs;
    while ((allMsgs.size() <= 20 && msgArchive.size() != allMsgs.size()) || (allMsgs.size() > 20 && msgArchive.size() != 20))
    {
        set<uint32_t> newData = recvData(s, sendCount);
        if (newData.size() == 0)
        {
            newData = memSave;
        }
        sendCount = sendData(s, &addr, newData);
        memSave = newData;
    }
}

void readFile(char *file_name)
{
    ifstream fdesc(file_name, ios::in);
    string fline;
    uint32_t index = 0;
    while(getline(fdesc, fline))
    {
        if (!fline.empty())
        {
            allMsgs.insert(make_pair(index, fline));
            index++;
        }
        fline.clear();
    }
}

void append_chunk(string &s, const uint8_t *chunk, size_t chunk_num_bytes)
{
    s.append((char *)chunk, chunk_num_bytes);
}

string datagramCreate(uint32_t index)
{
    string payload;
    string msgInfo = allMsgs[index].substr(0, allMsgs[index].find('+') + 12), token;
    string msgData = allMsgs[index].substr(allMsgs[index].find('+') + 13, allMsgs[index].size());
    vector <string> Tokens;
    istringstream _stream(msgInfo);
    while (getline(_stream, token, ' '))
    {
        if (!token.empty())
        {
            Tokens.push_back(token);
        }
        token.clear();
    }
    string date = Tokens[0];
    istringstream _datestream(date);
    vector <string> dateParts;
    while(getline(_datestream, token, '.'))
    {
        if (!token.empty())
        {
            dateParts.push_back(token);
            token.clear();
        }
    }
    /* Data processing */
    uint32_t msgIndex = htonl(index);
    uint8_t day = atoi(dateParts[0].c_str());
    uint8_t month = atoi(dateParts[1].c_str());
    uint16_t year = htons(atoi(dateParts[2].c_str()));
    int16_t AA = htons(atoi(Tokens[1].c_str()));

    append_chunk(payload, (const uint8_t *)&msgIndex, sizeof(uint32_t));
    append_chunk(payload, (const uint8_t *)&day, sizeof(uint8_t));
    append_chunk(payload, (const uint8_t *)&month, sizeof(uint8_t));
    append_chunk(payload, (const uint8_t *)&year, sizeof(uint16_t));
    append_chunk(payload, (const uint8_t *)&AA, sizeof(int16_t));
    append_chunk(payload, (const uint8_t *)Tokens[2].c_str(), 12);
    append_chunk(payload, (const uint8_t *)msgData.c_str(), msgData.size());
    return payload;
}

set <uint32_t> recvData(int s, int sendCount)
{
    struct timeval tv = {0, 100 * 1000};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(s, &fds);
    set <uint32_t> nums;
    int32_t result = select(s + 1, &fds, 0, 0, &tv);
    
    if (result > 0)
    {
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        uint32_t currentNums[20];
        memset(currentNums, 0, 20 * sizeof(uint32_t));
        int i = 0, r;
        while(i != sendCount - 1)
        {
            r = recvfrom(s, &currentNums, 20 * sizeof(uint32_t), 0, (sockaddr *)&addr, &addrLen);
            i++;
        }
        r = recvfrom(s, &currentNums, 20 * sizeof(uint32_t), 0, (sockaddr *)&addr, &addrLen);
        if (r > 0)
        {
            for (size_t i = 0; i < (int) r / 4; i++)
            {

                uint32_t rawInt = ntohl(currentNums[i]);
                if (msgArchive.find(rawInt) == msgArchive.end())
                {
                    msgArchive.insert(rawInt);
                }
            }
            for (size_t i = 0; i < allMsgs.size(); i++)
            {
                if (msgArchive.find(i) == msgArchive.end()) 
                {
                    nums.insert(i);
                    if (nums.size() == 20) break;
                }
            }
        }
    }
    return nums;
}

static int sendData(int s, sockaddr_in *addr, set <uint32_t> msgNums)
{
    cout << msgNums.size() << " message(s) in queue.." << endl;
    for (auto it = msgNums.begin(); it != msgNums.end(); it++)
    {
        string buffer = datagramCreate(*it);
        buffer.reserve(allMsgs[*it].size() + 4);
        int status = sendto(s, buffer.c_str(), buffer.size() + 1, 0, (sockaddr *)addr, sizeof(sockaddr));
    }
    return msgNums.size();
}
