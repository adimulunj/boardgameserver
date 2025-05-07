#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <fstream>
#include <sstream>

#include "json.hpp"  


//using json alias
using json = nlohmann::json;


#pragma comment(lib, "ws2_32.lib")

//defining games file
#define GAMES_FILE "games.txt"

//defining port and server global vars and mutex
#define PORT 8080

//printing mutex
std::mutex coutMutex;

//queing and games info (in pair, clientId, client socked)
std::vector<std::queue<std::pair<int, SOCKET>>                                                                                                                                                                                                > queues;
std::vector<std::string> games;
std::vector<std::vector<int>> gamesdata;

//ongoing games info
//vector of games [[game_id, game_type, game_status, client 1, client 2],[0102929, 0, (0 play or 1 end), 1, 2]]
std::vector<std::vector<int>> ongoing_games;

//clients and client mutex
std::vector<SOCKET> clients;
std::mutex clientsMutex;

bool attemptlogin(std::string username, std::string password)
{
    return true;
}


void broadcastMessage(const std::string& message, SOCKET sender) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (SOCKET client : clients) {
        if (client != sender) {
            send(client, message.c_str(), message.length(), 0);
        }
    }
}

void broadcastToClient(const std::string& message, SOCKET sender, SOCKET receiver) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    send(receiver, message.c_str(), message.length(), 0);
}

void handleClient(SOCKET clientSocket, int clientID) {
    //buffer for reading
    char buffer[1024] = {0};

    //loggedIn?
    bool loggedIn = false;

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back(clientSocket);
    }

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[Client " << clientID << "] Connected!\n";
    }

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            break; // disconnected
        }

        buffer[bytesReceived] = '\0';
        std::string received(buffer);

        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << "[Client " << clientID << "] Message: " << received << "\n";
        }
        
        try {
            json receivedJson = json::parse(received);
        
            //first check for log in
            if(loggedIn){

                // Access elements like a dictionary
                if (receivedJson.contains("action")) {

                    std::string action = receivedJson["action"];
                    if (action == "enqueue" && receivedJson.contains("gametype")) {
                        //get game type
                        int gameType = int(receivedJson["gametype"]);

                        std::cout << "Enqueuing Client " << clientID << " to game " << games[gameType] << std::endl;
                        
                        //mutx lock
                        std::unique_lock<std::mutex> lock(clientsMutex);
                        queues[gameType].push({clientID, clientSocket});
                        lock.unlock();

                        //send message to client
                        std::string message = "{\"message\" : \"enqueued\"}";
                        broadcastToClient(message, clientSocket, clientSocket);
                    }
                    else{
                        std::string message = "{\"error\": \"unable to interpret sent action\"}";
                        broadcastToClient(message, clientSocket, clientSocket);
                    }
                }
                else{
                    std::string message = "{\"error\": \"unable to interpret sent message\"}";
                    broadcastToClient(message, clientSocket, clientSocket);
                }
            
                // Optional: use the whole JSON to construct a response
                std::string fullMessage = "[Client " + std::to_string(clientID) + "] sent JSON: " + receivedJson.dump();
            
                broadcastMessage(fullMessage, clientSocket);
            }
            else{
                //not logged in so first message has to be log in
                if(receivedJson.contains("username") && receivedJson.contains("password"))
                {
                    if(attemptlogin(receivedJson["username"], receivedJson["password"])){
                    //std::cout << "logged in client "+ std::to_string(clientID) +" as " + receivedJson["username"] << std::endl;
                    

                    //return logged in message
                    loggedIn = true;
                    std::string message = "{\"message\": \"logged in\"}";
                    broadcastToClient(message, clientSocket, clientSocket);
                    
                    }
                    else{
                        std::string message = "{\"error\": \"login failed\"}";
                        broadcastToClient(message, clientSocket, clientSocket);
                    }
                }
                else{
                    //send error to client
                    std::string message = "{\"error\": \"server side json parsing failed\"}";
                    broadcastToClient(message, clientSocket, clientSocket);
                }
            }
        
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse JSON from Client " << clientID << ": " << e.what() << "\n";
        }
    }

    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << "[Client " << clientID << "] Disconnected.\n";
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    }

    closesocket(clientSocket);
}

void queueingThread()
{
    while(true){
        //loop over every queue and match clients and add them to a game
        for(int i = 0; i < queues.size(); i++)
        {
            std::queue q = queues[i];
            //if there are at least minimum number of players for that game
            //TODO: attempt to connect maximum number of players
            //TODO: group friends to play games together
            if( q.size() >= gamesdata[i][0])
            {
                
                //mutex lock
                std::unique_lock<std::mutex> lock(clientsMutex);
                std::pair<int, SOCKET> client1 = q.front();
                q.pop();

                
                std::pair<int, SOCKET> client2 = q.front();
                q.pop();
                queues[i] = q;

                //TODO: create game_id hash
                int game_id = 0;

                //add clients to a game
                std::vector<int> game = {game_id, i, 0, client1.first, client2.first};
                ongoing_games.push_back(game);

                lock.unlock();

                //notify clients
                std::string message = "{\"message\" : \"in game\"}";
                SOCKET clientSocket = client1.second;
                broadcastToClient(message, clientSocket, clientSocket);

                clientSocket = client2.second;
                broadcastToClient(message, clientSocket, clientSocket);

            }
        }
        //sleep for some time waiting for clients
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool loadGamesData()
{
    std::ifstream inputFile(GAMES_FILE);
    if(!inputFile){
        std::cerr << "unable to open file." << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(inputFile, line))
    {
        std::stringstream ss(line);
        std::string gameName, minPlayersStr, maxPlayersStr;

        // Read name and player counts
        if (!std::getline(ss, gameName, ',') ||
            !std::getline(ss, minPlayersStr, ',') ||
            !std::getline(ss, maxPlayersStr, ',')) {
            std::cerr << "Invalid line format: " << line << std::endl;
            continue;
        }

        games.push_back(gameName);

        // Parse integers and store in gamesdata
        int minPlayers = std::stoi(minPlayersStr);
        int maxPlayers = std::stoi(maxPlayersStr);
        gamesdata.push_back({minPlayers, maxPlayers});

        // Initialize queue
        std::queue<std::pair<int, SOCKET>> gameQ;
        queues.push_back(gameQ);
    }

    inputFile.close();
    return true;
}

int main() {
    //load games data
    if(!loadGamesData())
    {
        std::cerr << "unable to load games data shutting server";
        return 1;
    }

    WSADATA wsaData;
    SOCKET serverSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    //initialize the WinSock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    //start queueing thread
    std::thread  queingT(queueingThread);

    std::cout << "Multi-client server running on port " << PORT << "...\n";

    int clientID = 1;
    std::vector<std::thread> clientThreads;

    while (true) {
        SOCKET clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }

        clientThreads.emplace_back(std::thread(handleClient, clientSocket, clientID++));
        
    }

    // You can optionally join threads if you plan to shut down the server cleanly.
    if(queingT.joinable()) queingT.join();
    
    for (auto &t : clientThreads) {
        if (t.joinable()) t.join();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
