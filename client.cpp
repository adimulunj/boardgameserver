#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define GAMES_FILE "gamesclient.txt"

SOCKET sock;
std::atomic<bool> running(true);
std::atomic<bool> loggedIn(false);
std::atomic<bool> enqueued(false);
std::atomic<bool> messageRecieved(false);

std::vector<std::string> games;

bool loadGamesData()
{
    std::ifstream inputFile(GAMES_FILE);
    if(!inputFile){
        std::cerr << "unable to open file." << std::endl;
        return false;
    }
    std::string line;
    while(std::getline(inputFile, line))
    {
        games.push_back(line);
    }

    inputFile.close();
    return true;
}

void receiveMessages() {
    char buffer[1024];
    while (running) {
        int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::cout << "[Server]: " << buffer << std::endl;
            
            std::string received(buffer);

            //convert to json
            try{
                json receivedJson = json::parse(received);
                if(receivedJson.contains("message"))
                {
                    if(receivedJson["message"] == "logged in")
                    {
                        loggedIn = true;
                    }
                    else if(receivedJson["message"] == "enqueued")
                    {
                        enqueued = true;
                    }
                } else if(receivedJson.contains("error"))
                {
                    std::cerr << receivedJson["error"] << std::endl;
                }

            } catch(const std::exception& e)
            {
                std::cerr << "error json received from server" <<std::endl;
            }

            //update message recieved
            messageRecieved = true;

        } else if (bytesReceived == 0) {
            std::cout << "Server disconnected." << std::endl;
            running = false;
            break;
        } else {
            std::cerr << "recv failed." << std::endl;
            running = false;
            break;
        }
    }
}

bool enqueue()
{
    while(true)
    {
        std::cout << "enqueue for game: ";
        std::string input;
        std::getline(std::cin, input); 

        if(input != "/quit")
        {
            
            for(int i = 0; i < games.size(); i++)
            {
                
                if(games[i] == input)
                {
                    //make the message
                    std::string message = "{\"action\": \"enqueue\", \"gametype\": " + std::to_string(i) + "}";

                    send(sock, message.c_str(), static_cast<int>(message.length()), 0);

                    //wait for message of enqueue
                    while(!messageRecieved)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    }
                    messageRecieved = false;

                    std::cout << "enqueued for game " + input << std::endl;

                    return true;
                }
            }
        }
        else{
            running = false;
            return false;
        }
    }
}

bool login()
{
    while(!loggedIn)
    {
        std::cout << "username: ";
        std::string username;
        std::getline(std::cin, username);
        if (username == "/quit") {
            running = false;
            return 0;
        }
        std::cout << std::endl;

        std::cout << "password: ";
        std::string password;
        std::getline(std::cin, password);
        if (password == "/quit") {
            running = false;
            return 0;
        }
        std::cout << std::endl;

        std::string message = "{\"username\": \"" + username + "\", \"password\": \"" + password + "\"}";

        send(sock, message.c_str(), static_cast<int>(message.length()), 0);


        //wait for server to respond
        while(!messageRecieved)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        //when response has arrived reset the message recieved bool
        messageRecieved = false;
    }

    std::cout << "client logged in" << std::endl;
    return true;
}



int main() {
    //initialize the games vector
    if(!loadGamesData()) return 1;

    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // Server address setup
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(SERVER_PORT);

    // Connect to server
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to the server at " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    // Start receiving thread
    std::thread recvThread(receiveMessages);
    


    //login
    login();
    
    //enqueue
    enqueue();

    // Main loop to send messages
    std::string input;
    while (running) {
        std::getline(std::cin, input);
        if (input == "/quit") {
            running = false;
            return 0;
        }
        else if (!running)
        {
            return 0;
        }
        send(sock, input.c_str(), static_cast<int>(input.length()), 0);
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
    if (recvThread.joinable()) recvThread.join();

    std::cout << "Client exited.\n";
    return 0;
}
