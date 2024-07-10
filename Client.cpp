#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <iostream>

#define BUFFSIZE 1024

struct Client{
    std::string Name = ""; 
    int Descriptor; 
};

std::vector<Client> Clients;

bool threadTerminated = false;
bool needThreadTerminate = false;

void AsyncSend(int& sock){
    while(1){
        std::string message = "";
        std::cin >> message;

        if(needThreadTerminate){
            threadTerminated = true;
            return;
        }

        if(!message.empty()){
            if(send(sock, message.c_str(), BUFFSIZE, 0) == -1){
                std::cout << "Send error" << std::endl;
            }
        }
        else{
            std::cout << "Message can't be empty, try again.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    int sock;
    char buff[BUFFSIZE];

    if (argc < 4) {
        printf("Wrong input: should be 'ip address, port, name.'\n");
        exit(1);
    }

    std::string name(argv[3]);
    if(name.empty()){
        std::cout << "Name is empty.";
        exit(1);
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Can't get socket\n");
        exit(1);
    }

    struct sockaddr_in addres;

    addres.sin_family = AF_INET;
    addres.sin_port = htons(atoi(argv[2]));
    addres.sin_addr.s_addr = inet_addr(argv[1]);

    if(connect(sock, (struct sockaddr*)&addres, sizeof(addres)) < 0){
        printf("Error in connect\n");
        exit(1);
    }

    printf("CLIENT: Port number - %d\n", ntohs(addres.sin_port));

    if(send(sock, name.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
    if(recv(sock, buff, BUFFSIZE, 0) == -1) std::cout << "Recieve error" << std::endl;

    std::string onNameAnswer(buff);
    std::cout << onNameAnswer << std::endl;
    if(onNameAnswer.find("exist") != std::string::npos){
        exit(1);
    }

    if(recv(sock, buff, BUFFSIZE, 0) == -1) std::cout << "Recieve error" << std::endl;
    std::string usersList(buff);
    std::cout << usersList << std::endl;

    std::thread asyncReciever(AsyncSend, std::ref(sock));
    asyncReciever.detach();

    while(1){
        int msgLen = recv(sock, buff, BUFFSIZE, 0);
        if(msgLen == -1) std::cout << "Recieve error" << std::endl;

        std::string answer(buff);
        std::cout << answer << std::endl;
        if(answer.find("closed") != std::string::npos){
            needThreadTerminate = true;
            break;
        }

        if(answer.find("left chat.") != std::string::npos){

            std::string message("EraseClient " + answer.substr(0, answer.find(" ")));
            if(send(sock, message.c_str(), BUFFSIZE, 0) == -1) std::cout << "Send error" << std::endl;
        }
    }
    while(!threadTerminated){}

    close(sock);
    return 0;
}
