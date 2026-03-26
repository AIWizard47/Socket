#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <winsock2.h>
using namespace std;
#pragma comment(lib,"ws2_32.lib")

#define PORT 1420
#define BACKLOG 5

mutex clientsMtx;
mutex coutMtx;
vector<SOCKET> Slist;

void safeLog(const string& msg) {
    lock_guard<mutex> lock(coutMtx);
    cout << msg << "\n";
}

bool sendMsg(SOCKET sock, const string& text) {
    string line = text + "\r\n";
    int sent = send(sock, line.c_str(), (int)line.size(), 0);
    return sent != SOCKET_ERROR;
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

void broadcast(SOCKET sender, const string& msg) {
    lock_guard<mutex> lock(clientsMtx);
    for (SOCKET s : Slist) {
        if (s != sender) {
            sendMsg(s, msg);
        }
    }
}

void removeClient(SOCKET sock) {
    lock_guard<mutex> lock(clientsMtx);
    Slist.erase(std::remove(Slist.begin(), Slist.end(), sock), Slist.end()); // std::remove fixes the ambiguity
}

void handleClient(SOCKET clientSocket, int id) {
    safeLog("[Client " + to_string(id) + " connected]");

    while (true) {
        string msg = recvMsg(clientSocket);

        if (msg.empty()) {
            safeLog("[Client " + to_string(id) + " disconnected]");
            break;
        }

        safeLog("Client " + to_string(id) + ": " + msg);

        if (msg == "exit") {
            sendMsg(clientSocket, "Goodbye!");
            break;
        }

        broadcast(clientSocket, "Client " + to_string(id) + ": " + msg);
    }

    removeClient(clientSocket);
    closesocket(clientSocket);
}

int main() {
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        cout << "WSAStartup failed\n"; return -1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket failed\n"; WSACleanup(); return -1;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        cout << "Bind failed\n"; return -1;
    }
    if (listen(serverSocket, BACKLOG) == SOCKET_ERROR) {
        cout << "Listen failed\n"; return -1;
    }

    cout << "Server listening on port " << PORT << "...\n";

    int clientCount = 1;
    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);

        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == INVALID_SOCKET) continue;

        // protocol matching to connect with client.
        string protocol = "sam/1.1\r\n"
                        "open\r\n"
                        "chat\r\n";
        sendMsg(clientSocket,protocol);

        {
            lock_guard<mutex> lock(clientsMtx);
            Slist.push_back(clientSocket);
        }

        thread t(handleClient, clientSocket, clientCount++);
        t.detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
