#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#define BUFFSIZE 1024

struct Client {
    std::string Name = "";
    int Descriptor;
};

std::vector<Client> Clients;

void HandleFileTransfer(int senderSock, const std::string& filename, int recipientSock) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Couldn't open file " << filename << std::endl;
        return;
    }

    // Read file contents
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string fileContents = oss.str();

    // Send file contents to recipient
    if (send(recipientSock, fileContents.c_str(), fileContents.size(), 0) == -1) {
        std::cerr << "Error: Send error" << std::endl;
        return;
    }
}

void reaper(int sig) {
    int status;
    while(wait3(&status, WNOHANG, (struct rusage*)0) >= 0);
}

int main(int argc, char* argv[]) {
    int clientSocket;
    int listener;
    int messageLength;
    char buff[BUFFSIZE];
    socklen_t length = sizeof(struct sockaddr);
    struct sockaddr_in serverAddr;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = 0;

    if (bind(listener, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("Failed binding\n");
        exit(1);
    }

    listen(listener, 5);
    if (getsockname(listener, (struct sockaddr*)&serverAddr, &length)) {
        printf("Error when calling getsockname\n");
        exit(1);
    }

    printf("Server: Port Number - %d\n", ntohs(serverAddr.sin_port));
    printf("SERVER: IP %s\n", inet_ntoa(serverAddr.sin_addr));

    signal(SIGCHLD, reaper);

    int clientCount = 2;

    for (int i = 0; i < clientCount; i++) {
        clientSocket = accept(listener, 0, 0);

        messageLength = recv(clientSocket, buff, BUFFSIZE, 0);
        if (messageLength == -1) std::cout << "Receive error" << std::endl;

        buff[messageLength] = '\0';
        std::string message(buff);
        std::cout << "Request message: " << message << std::endl;

        auto sameName = std::find_if(Clients.begin(), Clients.end(), [&](const Client& client){return client.Name == message;});
        if (sameName != Clients.end()) {
            std::string request("Server: This username already exists.\n");
            if (send(clientSocket, request.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
            close(clientSocket);
            i--;
        }

        Clients.push_back(Client{message, clientSocket});
        message = "Server: Connected.";
        if (send(clientSocket, message.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
    }

    for (const auto& user : Clients) {
        std::string availableClients = "";
        int availableClientsCount = Clients.size() - 1;
        availableClients += "Server: " + std::to_string(availableClientsCount) + " available users:\n";

        int clientNum = 1;
        for (const auto& client : Clients) {
            if (client.Name != user.Name) {
                availableClients += std::to_string(clientNum) + ". " + client.Name + "\n";
                clientNum++;
            }
        }

        availableClients += "Server: Chat Started.\n";
        if (send(user.Descriptor, availableClients.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
    }

    for (int i = 0; i < clientCount; i++) {
        clientSocket = Clients[i].Descriptor;
        std::string clientName = Clients[i].Name;
        switch (fork()) {
            case -1: {
                printf("Error in fork\n");
                exit(1);
            }
            case 0: {
                close(listener);

                while (1) {
                    messageLength = recv(clientSocket, buff, BUFFSIZE, 0);
                    if (messageLength == -1) std::cout << "Receive error" << std::endl;

                    buff[messageLength] = '\0';
                    std::string message(buff);
                    std::cout << "Request message: " << message << std::endl;

                    if (message == "disconnect") {
                        std::string info("Server: Chat closed.\n");
                        if (send(clientSocket, info.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;

                        info = std::string(clientName + " left chat.");
                        for (const auto& client : Clients) {
                            if (client.Name != clientName) {
                                std::cout << "Receiver: " << client.Name << " Answer: " << info << std::endl << std::endl;
                                if (send(client.Descriptor, info.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
                            }
                        }
                        break;
                    } else if (message == "sendfile") {
                        std::string filename = message.substr(9); // Extract filename from message
                        for(const auto& client : Clients){
                            if(client.Name != clientName){
                                HandleFileTransfer(clientSocket, filename, client.Descriptor);
                            }
                        }
                       
                    } else {
                            std::string messageForReciever = clientName + ": " + message;
                            for(const auto& client : Clients){
                                if(client.Name != clientName){
                                std::cout << "Reciever: " << client.Name << " Answer: " << messageForReciever << std::endl << std::endl;
                                if(send(client.Descriptor, messageForReciever.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
                            }
                        }
                    }
                }
                close(clientSocket);
                exit(0);
            }
            default: {
                while (waitpid(-1, NULL, WNOHANG) > 0);
            }
        }
    }
    wait(NULL);
    close(listener);

    return 0;
}
