//server.cpp
//table time server

//should be a command line interface; things are printed as they occur.
//intend to support commands in the future: object creation, kicking players, changing rules, etc.

//server should hold a big dictionary of entities, keyed with ID's that uniquely identity instances/entities
//clients have typical entity lists, each instance knows its ID.
//every server child thread handles one client: all actions performed by that client are sent to the server.
//each server child thread has two queues: things from the client to the main server thread, and things from the main serer thread to their client.
//player performs action, client executes what it can (movement, flipping a card whose other side is known) and then tells server child thread both the action and any requests for more information (other card sides -> child thread examines action and places in queue -> main server inspects queue from each child at its leisure -> main server executes action by updating entity dictionary -> main server informs child threads that an entity's state has changed by placing action in each child's queue -> child threads inform their clients of state changes as they occur (except client that performed action?)-> clients realize these state changes

//problem: want client to be able to execute actions locally for more smooth experience, but client may not have needed information locally.
//solutions:
	//let client execute what it can locally, and request server for more information. server updates all clients except the action-performing one, and handles any special requests for the action client.
		//pros: maximizes smoothness/response time
		//cons: more complicated
	//clients only execute movement locally. all clients, including the action client, are informed of all state changes.
		//improvements: all action notifications include a unique id telling which player performed it. clients can ignore movement notifications from themselves.
		//pros: very simple, I think it lowers chance of desync
		//cons: things like flipping cards and rolling dice will have a small amount of latency. This is probably perfectly fine. an icon can be placed on the offending item indicating that an action is in the middle of taking place (will probably only be visible for a very short time if it just waits for the action to occur. perhaps display for a minimum amount of time for all players, including the action player?)
		
//each player has a hand zone: things that they can freely see but other players cannot. entities in a hand zone are kept track of by the server. clients that cannot see the item should be notified that they cannot see them, and should draw them face down.
//may wish to implement "secret zones:" same thing but secret objects sohuld not be drawn at all (instead of being drawn face down)
//clients will be notified when an object has been removed from a secret or hand zone. clients should not allow movement of objects in others' secret/hand zones.
//piles and stacks are desired: piles are all of one object and can be limited or limitless. stacks are limited and comprised of unique items. stacks can be shuffled and drawn from, and can be face up or face down. items removed from a stack are instantiated, items placed in the stack are added to the stack's internal list but not drawn.

#define _WIN32_WINNT _WIN32_WINNT_WIN7 //this code not officially supported for windows versions older than windows 7

#include "Entity.hpp"

#include <string>
#include <random>
#include <list>
#include <iostream>

//#include <sys/socket.h> //for unix systems... ugh.
//#include <arpa/inet.h>

#include <winsock2.h> //networking imports for windows.
#include <ws2tcpip.h>
#include <mstcpip.h>

//#include <yaml.c> //TODO include this in compile command

#define DEFAULT_PORT "21877"
#define INET_BUFFER_LEN 256

struct Client {
	SOCKET csock; //client socket
	int cid; //client ID
	bool active;
	int player; //player at the table; used like color in tabletop simulator
	//item for incoming event Q, buffers, etc.
};

bool is_inactive(Client &c) { return !c.active; } //predicate for client list culling

//====================== MAIN ====================	
int main(int argc, char* argv[]){
	//command line arguments
	bool DEBUG_MODE = false;
	char defaultPortStr[] = DEFAULT_PORT;
	char* PORT = defaultPortStr;
	
	for (int i=1; i<argc; i++){
		std::string argstr(argv[i]);
		if (argstr == "-d") DEBUG_MODE = true;
		if (argstr == "-p" && i+1 < argc) PORT = argv[i+1];
	}
	
	//====================== WINSOCK INITIALIZATION ====================	
	std::cout << "Port: " << PORT << "\n";
	std::cout << "Creating socket...\n";
	
	//based on guide from https://docs.microsoft.com/en-us/windows/win32/winsock/getting-started-with-winsock
	//initialize winsock
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0){
		std::cout << "Windows socket library WSAStartup failed, Code: "<< WSAGetLastError() << std::endl;
		return 1;
	}
	
	//====================== CREATING LISTENER SOCKET ====================	
	//based on guide from https://docs.microsoft.com/en-us/windows/win32/winsock/getting-started-with-winsock
	//create address info
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //default ot IVP4 unless client says otherwise... I think. Defaults to IVP4 here; defers to client because of "AI_PASSIVE" below
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	
	iResult = getaddrinfo(NULL, PORT, &hints, &result);
	if (iResult != 0){
		std::cout << "getaddrinfo failed. Code: "<< WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}
	
	//create listener socket
	SOCKET listenerSocket = INVALID_SOCKET;
	listenerSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listenerSocket == INVALID_SOCKET){
		std::cout << "Error creating socket. Code: "<< WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}
	
	//bind listener socket to port
	iResult = bind(listenerSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR){
		std::cout << "Error: Could not bind to port! Code: "<< WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		closesocket(listenerSocket);
		WSACleanup();
		return 1;
	}
	freeaddrinfo(result);
	
	//start listening on port
	iResult = listen(listenerSocket, SOMAXCONN); //SOMAXCONN: sets maximum backlog of waiting clients to be a "reasonable number"
	if (iResult == SOCKET_ERROR){
		std::cout << "Error: Could not start listening.\n";
		closesocket(listenerSocket);
		WSACleanup();
		return 1;
	}
	
	std::cout << "Listening on port " <<PORT <<"...\n";
	//====================== SET UP INFRASTRUCTURE =====================
	char inet_buffer[INET_BUFFER_LEN];
	bool SHOULD_EXIT = false; //set when we should stop. command line instruction used to set to true. Also true when certain errors occur.
	int nextID = 0;
	std::list<Client> clientList; //list of clients... using list instead of vector so we can just pop() these without trouble
	WSAPOLLFD listenerPollFD;
	listenerPollFD.fd = listenerSocket;
	listenerPollFD.events = POLLRDNORM;
	
	WSAPOLLFD clientPollFD;
	clientPollFD.fd = INVALID_SOCKET;
	clientPollFD.events = POLLRDNORM;	
	
	//====================== MAIN LOOP ====================	
	while (!SHOULD_EXIT){
		//====================== ACCEPTING INCOMING CONNECTIONS ====================	
		iResult = WSAPoll(&listenerPollFD, 1, 0);
		if (iResult == SOCKET_ERROR){
			std::cout << "Error polling listener socket.\n";
			closesocket(listenerSocket);
			SHOULD_EXIT = true;
		}
		if (iResult > 0 && listenerPollFD.revents & POLLRDNORM){
			//a new client is waiting. accept connection.
			std::cout << "Accepting new client. ID: " << nextID << "\n";
			SOCKET newClientSocket = accept(listenerSocket, NULL, NULL);
			Client newClient;
			newClient.csock = newClientSocket;
			newClient.cid = nextID++;
			newClient.active = true;
			clientList.push_back(newClient);
		}
		
		//====================== HANDLE EXISTING CONNECTIONS ====================
		//for each client, check if they're talking:
		std::list<Client>::iterator itr;
		for (itr=clientList.begin(); itr != clientList.end(); ++itr){
			clientPollFD.fd = itr->csock;
			iResult = WSAPoll(&clientPollFD, 1, 0); //I understand that the entire purpose of this command is to be able to poll more than one socket at a time, but this is simpler code to write for now.
			if (iResult == SOCKET_ERROR){
				std::cout << "Error polling client socket.\n";
				closesocket(itr->csock);
				itr->active = false;
			}
			if (iResult > 0 && clientPollFD.revents & POLLRDNORM){
				//data is ready from this client. let's give it a read.
				//for now we are just going to print the data received...
				iResult = recv(itr->csock, inet_buffer, INET_BUFFER_LEN, 0); //0 indicates no special flags
				std::cout << "Received data from client " << itr->cid << ": \n" << inet_buffer << "\n";
			}
		}
		//pop any clients that are no longer active. must do this outside the loop so as to not corrupt the iterator.
		clientList.remove_if(is_inactive);
	}
	
	//close listener socket
	closesocket(listenerSocket);
	//nicely close all client sockets
	std::list<Client>::iterator itr;
	for (itr=clientList.begin(); itr != clientList.end(); ++itr){
		iResult = shutdown(itr->csock, SD_SEND);
		if (iResult == SOCKET_ERROR){
			std::cout << "Error shutting down client socket.\n";
		}
		closesocket(itr->csock);
	}
	WSACleanup();
	return 0;
}
