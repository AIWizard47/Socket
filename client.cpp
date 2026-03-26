#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

#define PORT 1420
#define HOST "127.0.0.1"

bool running = true;

bool sendMsg(SOCKET sock, const string& text) {
    string line = text + "\r\n";
    int sent = send(sock, line.c_str(), (int)line.size(), 0);
    return sent != SOCKET_ERROR;
}


bool protocolCheck(SOCKET sock)
{
    char buffer[1024];
    string recMsg = "";

    // Receive until we detect end of protocol (\r\n\r\n OR enough lines)
    while (true)
    {
        int r = recv(sock, buffer, sizeof(buffer), 0);
        if (r <= 0)
        {
            cout << "[Connection closed during handshake]\n";
            return false;
        }

        recMsg.append(buffer, r);

        // Stop condition (you can improve this later)
        if (recMsg.find("\r\n\r\n") != string::npos)
            break;
    }

    // Parse lines
    vector<string> lProtocol;
    string temp = "";

    for (int i = 0; i < recMsg.size(); i++)
    {
        if (recMsg[i] == '\r' && i + 1 < recMsg.size() && recMsg[i+1] == '\n')
        {
            lProtocol.push_back(temp);
            temp = "";
            i++; // skip '\n'
        }
        else
        {
            temp += recMsg[i];
        }
    }

    // Validate protocol structure
    if (lProtocol.size() >= 3 &&
        lProtocol[0] == "sam/1.1" &&
        lProtocol[1] == "open" &&
        lProtocol[2] == "chat")
    {
        cout << "[Protocol OK]\n";
        return true;
    }

    // Debug output
    cout << "[Bad protocol received]\n";
    for (auto &line : lProtocol)
        cout << ">> " << line << endl;

    return false;
}


string recvMsg(SOCKET sock) {
    string result;
    char ch;
    while (true) {
        int r = recv(sock, &ch, 1, 0);
        if (r <= 0) return "";
        if (ch == '\n') break;
        result += ch;
    }
    return result;
}

// Thread 1: constantly listens for incoming messages
void receiveThread(SOCKET sock) {
    while (running) {
        string msg = recvMsg(sock);
        if (msg.empty()) {
            cout << "\n[Disconnected from server]\n";
            running = false;
            break;
        }
        // \r clears the current "You: " prompt line, looks cleaner
        cout << "\r" << msg << "\nYou: " << flush;
    }
}

// Thread 2: reads your input and sends it
void sendThread(SOCKET sock) {
    while (running) {
        cout << "You: " << flush;
        string msg;
        getline(cin, msg);

        if (msg.empty()) continue;

        if (!sendMsg(sock, msg)) {
            cout << "[Send failed]\n";
            running = false;
            break;
        }

        if (msg == "exit") {
            running = false;
            break;
        }
    }
}

int main() {
    cout << "Starting client...\n";

    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        cout << "WSAStartup failed\n"; return -1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cout << "Socket failed\n"; WSACleanup(); return -1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(HOST);

    int attempt = 1;
    while (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Connection attempt " << attempt++ << " failed, retrying...\n";
        Sleep(1000);
    }

    bool pcheck = protocolCheck(sock);
    if(!pcheck)
    {
        cout << "Connection failed due to wrong protocol try again\n\n";
        closesocket(sock);
        WSACleanup();
        return -1;
    }
    cout << "Connected! Type a message and Enter to send. Type 'exit' to quit.\n\n";

    // Spawn receiver on a separate thread — runs at the same time as sender
    thread receiver(receiveThread, sock);
    receiver.detach();

    // Main thread handles sending
    sendThread(sock);

    closesocket(sock);
    WSACleanup();
    return 0;
}
