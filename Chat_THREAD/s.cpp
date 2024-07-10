#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <map>
#define SERVER_ACCEPT '1'
#define SERVER_DENY '0'
#define MAX_CLIENTS 10

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

std::map<int, std::string> clientsUsernames;

std::string prefix = "[chat] ";
std::string password;
std::vector<int> clients;

void *handleClient(void *arg)
{
    int clientSocket = *((int *)arg);
    char buffer[1024];

    // Get password
    // int bytesReceived = recv(clientSocket, buffer, 1024, 0);
    // if (bytesReceived <= 0) {
    //   std::cerr << "Error receiving username\n";
    //   close(clientSocket);
    //   pthread_exit(nullptr);
    // }
    // std::string received_password(buffer, bytesReceived);

    // // Check password
    char answer[1] = {SERVER_ACCEPT};
    // if (received_password != password) {
    //   answer[0] = SERVER_DENY;
    //   send(clientSocket, answer, 1, 0);
    //   close(clientSocket);
    //   pthread_exit(nullptr);
    // }
    send(clientSocket, answer, 1, 0);

    // Get username
    int bytesReceived = recv(clientSocket, buffer, 1024, 0);
    if (bytesReceived <= 0)
    {
        std::cerr << "Error receiving username\n";
        close(clientSocket);
        pthread_exit(nullptr);
    }
    std::string username(buffer, bytesReceived);

    // Send join message
    pthread_mutex_lock(&clients_mutex);
    std::string message = username + " connected";
    for (int client : clients)
    {
        send(client, message.c_str(), message.size(), 0);
    }
    pthread_mutex_unlock(&clients_mutex);
    std::cout << prefix << message << '\n';

    pthread_mutex_lock(&clients_mutex);
    clientsUsernames[clientSocket] = username;
    std::string clientsList;
    for (size_t i = 0; i < clients.size(); ++i)
    {
        clientsList += std::to_string(i) + ": " + clientsUsernames[clients[i]] + "\n";
    }
    send(clientSocket, clientsList.c_str(), clientsList.size(), 0);
    pthread_mutex_unlock(&clients_mutex);

    while (true)
    {
        bytesReceived = recv(clientSocket, buffer, 1024, 0);

        // Client disconnected
        if (bytesReceived <= 0)
        {
            pthread_mutex_lock(&clients_mutex);
            auto it = std::find(clients.begin(), clients.end(), clientSocket);
            if (it != clients.end())
            {
                clients.erase(it);
            }
            message = username + " disconnected";
            for (int client : clients)
            {
                send(client, message.c_str(), message.size(), 0);
            }
            pthread_mutex_unlock(&clients_mutex);
            std::cout << prefix << message << '\n';

            close(clientSocket);
            pthread_exit(nullptr);
        }

        if (buffer[0] == '/')
        {
            // Extract command
            std::string command, target, message;
            std::istringstream iss(buffer);
            iss >> command >> target;
            // if (command.substr(0, 5) == "/list")
            // {
            //     clientsUsernames[clientSocket] = username;
            //     std::string clientsList;
            //     for (size_t i = 0; i < clients.size(); ++i)
            //     {
            //         clientsList += std::to_string(i) + ": " + clientsUsernames[clients[i]] + "\n";
            //     }
            //     send(clientSocket, clientsList.c_str(), clientsList.size(), 0);
            // }
            // else
            // {
            //     std::cerr << "Invalid client index\n";
            // }

            if (command.substr(0, 5) == "/send")
            {
                int index = std::stoi(target);
                if (index >= 0 && index < clients.size())
                {
                    std::getline(iss, message);
                    // Remove leading whitespace
                    message = message.substr(message.find_first_not_of(" \t"));
                    // Send message to specified client
                    send(clients[index], message.c_str(), message.size(), 0);
                }
                else
                {
                    std::cerr << "Error send\n";
                }
            }
        }
        else
        {
            // Broadcast message to all clients
            pthread_mutex_lock(&clients_mutex);
            for (int client : clients)
            {
                if (client != clientSocket)
                {
                    send(client, buffer, bytesReceived, 0);
                }
            }
            pthread_mutex_unlock(&clients_mutex);

            // Show chat log on server
            buffer[bytesReceived] = '\0';
            std::cout << prefix << buffer << '\n';
        }
    }
}

int main(int argc, char *argv[])
{
    // if (argc != 2) {
    //   std::cerr << "Usage: " << argv[0] << " <password>\n";
    //   return 1;
    // }
    // password = argv[1];

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(12345);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) ==
        -1)
    {
        std::cerr << "Error binding socket\n";
        return 1;
    }

    socklen_t len = sizeof(serverAddr);
    getsockname(serverSocket, (struct sockaddr *)&serverAddr, &len);
    int port = ntohs(serverAddr.sin_port);
    std::cout << "Server started. Listening on port " << port << '\n';

    if (listen(serverSocket, MAX_CLIENTS) == -1)
    {
        std::cerr << "Error listening on socket\n";
        return 1;
    }

    while (true)
    {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == -1)
        {
            std::cerr << "Error accepting connection\n";
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        clients.push_back(clientSocket);
        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread;
        if (pthread_create(&thread, nullptr, handleClient, (void *)&clientSocket) !=
            0)
        {
            std::cerr << "Error creating thread\n";
        }
    }
    close(serverSocket);
    return 0;
}