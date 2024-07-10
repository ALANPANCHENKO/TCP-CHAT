#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#define SERVER_ACCEPT '1'

int clientSocket;
pthread_t receiveThread;
std::string username;

void *receiveMessages(void *arg)
{
    char buffer[1024];
    while (true)
    {
        int bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived <= 0)
        {
            std::cerr << "Error receiving message from server\n";
            break;
        }
        buffer[bytesReceived] = '\0';
        std::cout << buffer << '\n';
    }
    close(clientSocket);
    pthread_exit(nullptr);
}

void *sendMessage(void *arg)
{
    while (true)
    {
        std::string text;
        std::getline(std::cin, text);
        // std::cout << "\033[A\33[2K\r";
        if (!text.empty())
        {

            if (text.substr(0, 6) == "/send ")
            {
                // Extract target index and message
                std::string target, message;
                std::istringstream iss(text);
                iss >> target;
                int spacePos = text.find(" ", 6);
                if (spacePos != std::string::npos)
                {
                    target = text.substr(6, spacePos - 6);
                    message = text.substr(spacePos + 1);
                    // Remove leading whitespace from message
                    message = message.substr(message.find_first_not_of(" \t"));
                    // Add sender's username to the message
                    message = username + " (private): " + message;
                    // Send message to server with /send command
                    std::string sendCommand = "/send " + target + " " + message;
                    send(clientSocket, sendCommand.c_str(), sendCommand.size(), 0);
                }
                else
                {
                    std::cerr << "Unknown command\n";
                }
            }
            // if(text.substr(0, 5) == "/list"){
            //     send(clientSocket, "/list", strlen("/list"), 0);
            // }
            else
            {
                std::cout << "\033[A\33[2K\r";
                std::string message = username + ": " + text;
                if (send(clientSocket, message.c_str(), message.size(), 0) == -1)
                {
                    std::cerr << "Error sending message\n";
                    break;
                }
                std::cout << message << '\n';
            }
        }
    }
    pthread_cancel(receiveThread);
    close(clientSocket);
    pthread_exit(nullptr);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << " <server_ip> <port> <username>\n";
        return 1;
    }
    const char *serverIp = argv[1];
    int PORT = atoi(argv[2]);
    username = argv[3];

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIp, &serverAddr.sin_addr);

    if (connect(clientSocket, (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1)
    {
        std::cerr << "Error connecting to server\n";
        return 1;
    }
    // std::cout << "[server] Connected, checking password\n";
    // Send password
    // send(clientSocket, password.c_str(), password.size(), 0);
    // Get answer (accept/deny)
    char answer[1];
    int received = recv(clientSocket, answer, 1, 0);
    if (received <= 0)
    {
        std::cerr << "Error receiving answer from server\n";
        return 1;
    }
    if (answer[0] != SERVER_ACCEPT)
    {
        std::cerr << "[server] Wrong password\n";
        return 1;
    }

    // Send username
    send(clientSocket, username.c_str(), username.size(), 0);

    // Use pthreads to receive & send messages
    pthread_create(&receiveThread, nullptr, receiveMessages, nullptr);
    pthread_t sendThread;
    pthread_create(&sendThread, nullptr, sendMessage, nullptr);
    pthread_join(receiveThread, nullptr);
    pthread_cancel(sendThread);
    return 0;
}