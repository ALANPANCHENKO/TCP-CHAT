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

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#define BUFFSIZE 1024

struct Client{
    std::string Name = ""; 
    int Descriptor; 
};

std::vector<Client> Clients;

void reaper(int sig) {
    int status;
    while(wait3(&status, WNOHANG, (struct rusage*)0) >= 0);
}

int main(int argc, char* argv[])
{
    int clientSocket;
    int listener;
    int messageLength;
    char buff[BUFFSIZE];
    socklen_t length = sizeof(struct sockaddr);
    struct sockaddr_in serverAddr;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    inet_aton("127.0.0.1",&serverAddr.sin_addr);
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

    // int clientCount = 3;
    while(1){
        clientSocket = accept(listener, 0, 0);
        if (clientSocket == -1) {
            perror("Failed to accept connection");
            continue;
        }

        printf("New client connected. Client socket descriptor1: %d\n", clientSocket);

        int numC = 1;
        messageLength = recv(clientSocket, buff, BUFFSIZE, 0);
        if (messageLength == -1) {
            perror("Receive error1");
            close(clientSocket);
            continue;
        }

        buff[messageLength] = '\0';
        std::string message(buff);
        std::cout << "Name: " << message << std::endl;

        auto sameName = std::find_if(Clients.begin(), Clients.end(), [&](const Client& client){return client.Name == message;});
    
        if(sameName != Clients.end()){
            std::string request("Server: This username already exist.\n");
            if(send(clientSocket, request.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error1" << std::endl;
            close(clientSocket);
            numC--;
        }

        Clients.push_back(Client{message, clientSocket});
        printf("New client connected. Client socket descriptor2: %d\n", clientSocket);
        // numC=Clients.size()-1;

        // printf("Socket из вектора: %d\n", clientSocket);
        std::string clientName = message;
        message = "Server: Connected.";
        if(send(clientSocket, message.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error2" << std::endl;
        
        // std::cout << "Clients:" << std::endl;
        // for (const auto& client : Clients) {
        // std::cout << "Name: " << client.Name << ", Descriptor: " << client.Descriptor << std::endl;
        // }

         // Вставляем код для обработки сообщений в цикле while(1)
        pid_t pid = fork();
        switch(pid){
            case -1:{
                printf("Error in fork\n");
                exit(1);
            }
            case 0:{
                close(listener);
                    printf("socket в форке - %d \n", clientSocket);
                    messageLength = recv(clientSocket, buff, BUFFSIZE, 0);
                    if(messageLength == -1) std::cout << "Recieve error2" << std::endl;

                    buff[messageLength] = '\0';
                    std::string message(buff);
                    std::cout << "Request message: " << message << std::endl;

                    if(1){
                        if(message == "disconnect"){
                            std::string info("Server: Chat closed.\n");
                            if(send(clientSocket, info.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error3" << std::endl;

                            info = std::string(clientName + " left chat.");
                            for(const auto& client : Clients){
                                if(client.Name != clientName){
                                    std::cout << "Reciever: " << client.Name << " Answer: " << info << std::endl << std::endl;
                                    if(send(client.Descriptor, info.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error4" << std::endl;
                                }
                            }
                            break;
                        }

                        std::string messageForReciever = clientName + ": " + message;

                        for(const auto& client : Clients){
                            if(client.Name != clientName){
                                std::cout << "Reciever: " << client.Name << " Answer: " << messageForReciever << std::endl << std::endl;
                                if(send(client.Descriptor, messageForReciever.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error5" << std::endl;
                            }
                        }
                    }
                close(clientSocket);
                exit(0);
            }
            default:{
                // while(waitpid(-1, NULL, WNOHANG) > 0);
            }
        }
    }
    
    close(listener);

    return 0;
}
