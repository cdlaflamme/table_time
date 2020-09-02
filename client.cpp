//main.cpp
//table time

#define _WIN32_WINNT _WIN32_WINNT_WIN7 //this code not officially supported for windows versions older than windows 7

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include "Entity.hpp"
#include "DrawQ.cpp"

#include <string>
#include <list>
#include <iostream>

//#include <sys/socket.h> //for unix systems... ugh.
//#include <arpa/inet.h>

#include <winsock2.h> //networking imports for windows.
#include <ws2tcpip.h>
#include <mstcpip.h>

#define SCREEN_X 800
#define SCREEN_Y 800
#define DEFAULT_PORT "21877"
#define INET_BUFFER_LEN 256

int main(int argc, char *argv[]){

	//command line arguments
	bool DEBUG_MODE = false;
	bool LOCALHOST = false;
	char* PORT = NULL;
	char* host_address = NULL;
	char localHostStr[] = "127.0.0.1";
	char defaultPortStr[] = DEFAULT_PORT;
	
	for (int i=1; i<argc; i++){
		std::string argstr(argv[i]);
		if (argstr == "-d") DEBUG_MODE = true;
		if (argstr == "-l") {
			LOCALHOST = true;
			host_address = localHostStr;
		}
		if (argstr == "-ip" && i+1 < argc) host_address = argv[i+1];
		if (argstr == "-p" && i+1 < argc) PORT = argv[i+1];
	}
	if (PORT == NULL) PORT = defaultPortStr;
	
	if (host_address == NULL && LOCALHOST == false){
		std::cout << "Error: No host supplied. Use '-ip [host ip] to designate a host or use '-l' to use localhost.";
		return 1;
	}
	/*
	//====================== POSIX SERVER CONNECTION ====================	
	struct in_addr srv_in_addr; //server inet address
	struct hostent* srv_host; //server host object used by networking calls
	
	//connect to a server before doing anything else
	if (LOCALHOST){
		printf("Host: localhost\n");
		srv_host = gethostbyname("localhost")
	}
	else{
		printf("Host: %s\n", host_address);
		srv_host = gethostbyname(host_address);
	}
	printf("Port: %d\n", PORT);
	printf("Connecting...\n");
	
	//create address
	memmove((char*)&srv_in_addr, (char*)srv_host->h_addr_list[0], srv_host->h_length);	
	
	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr)); //initialize address struct to 0
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(port);
	srv_addr.sin_addr = srv_in_addr;

	//create socket
	int srv_sock = socket(AF_INET, SOCK_STREAM, 0);

	//connect to foreign address
	if (0!=connect(srv_sock,(struct sockaddr*)&srv_addr, sizeof(srv_addr))){
		printf("Error: Could not connect to server.\n");
		exit(1);
	}
	*/
	
	//====================== WINSOCK SERVER CONNECTION ====================	
	std::cout << "Host: " << host_address << "\n";
	std::cout << "Port: " << PORT << "\n";
	std::cout << "Connecting...\n";
	
	//based on guide from https://docs.microsoft.com/en-us/windows/win32/winsock/getting-started-with-winsock
	//initialize winsock
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0){
		std::cout << "Windows socket library WSAStartup failed, code " << iResult << "\n";
		return 1;
	}
	std::cout << "WSA Startup successful.\n";
	
	//create socket for client
	struct addrinfo 	*result = NULL,
						hints;
	
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	
	iResult = getaddrinfo(host_address, PORT, &hints, &result);
	if (iResult != 0) {
		std::cout << "Windows getaddrinfo failed, code " << iResult << "\n";
		WSACleanup();
		return 1;
	}
	std::cout <<"Parsed host successfully.\n";
	
	SOCKET clientSocket = INVALID_SOCKET;
	clientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	
	if (clientSocket == INVALID_SOCKET) {
		std::cout << "Windows created invalid socket. Was probably given an invalid IP/port.\n";
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}
	std::cout << "Created client socket.\n";
	
	//connect to foreign address
	iResult = connect(clientSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		std::cout << "Unable to connect to server. :(\n";
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
		WSACleanup();
		return 1;
	}
	
	freeaddrinfo(result);
	if (clientSocket == INVALID_SOCKET){
		std::cout << "Client socket suddenly invalid.\n";
		WSACleanup();
		return 1;
	}
	std::cout << "Successfully connected to host.\n";
	
	//====================== ESTABLISHING GUI INFRASTRUCTURE ====================	
	//set up infrastructure.
	sf::RenderWindow window(sf::VideoMode(SCREEN_X, SCREEN_Y), "Table Time"); //create window
	DrawQ drawQ; //create draw queue
	std::list<Entity*> entityList; //entity list
	sf::Clock instanceClock; //used to time everything
	
	//load font
	int FONTSIZE = 20;
	sf::Font basicFont;
	if (!basicFont.loadFromFile("Assets/Titillium/Titillium-Regular.otf")){
		std::cout << "Could not load 'Assets/Titillium/Titillium-Regular.otf'. Exiting.";
		exit(1);
	}
	
	//server communication buffer
	char inet_buffer[INET_BUFFER_LEN];
	//server socket poll structure
	WSAPOLLFD pollFD = {0};
	pollFD.fd = clientSocket;
	pollFD.events = POLLRDNORM;// | POLLWRNORM;

	//TODO perhapos check to see if socket is writeable...
	
	bool menu_up = false;
	//====================== MAIN LOOP ====================	
	while (window.isOpen()){
		//Process Input Events
		sf::Event event;
		while (window.pollEvent(event)){
			//Event: close window
			if (event.type == sf::Event::Closed)
				window.close();
			//Event: keyboard input
			if (event.type == sf::Event::KeyPressed){
				if (event.key.code == sf::Keyboard::Escape){
					menu_up = !menu_up;
				}
				if (event.key.code == sf::Keyboard::Left){
					//TODO place an item in the Q, rather than just sending it
					std::cout << "Pressed left. Sending mesage to server...\n";
					char leftMsg[] = "client pressed left";
					send(clientSocket, leftMsg, sizeof(leftMsg), 0);
				}
			}
		}
		
		//Process Network Events
		//poll server: if bytes available, read them
		iResult = WSAPoll(&pollFD, 1, 0); //poll socket to check if readable. arguments: array of WSAPOLLFD, len of array, timeout (s)
		if (iResult == SOCKET_ERROR || (iResult !=0 && pollFD.revents & POLLERR)){
			std::cout << "Error polling socket.\n";
			closesocket(clientSocket);
			WSACleanup();
			return 1;
		}
		if (iResult != 0 && pollFD.revents & POLLHUP){
			//we got hung up on. :(
			std::cout << "Server unexpectedly closed connection.\n";
			closesocket(clientSocket);
			WSACleanup();
			return 1;
		}			
		if (iResult != 0 && pollFD.revents & POLLRDNORM){
			//there is data to read!
			//for now, we will read all the data and print it into the console.
			//in the future, we should: create a function to read bytes, segment them into events, and add them to the incoming event Q. need a buffer for "the event currently being assembled". to start an event, byte length and prefix probably used.
			iResult = recv(clientSocket, inet_buffer, INET_BUFFER_LEN, 0); //0 is flags parameter, none are needed
			std::cout << "Received " << iResult << " bytes from server: \n" << inet_buffer << "\n";
		}
		
		//Game logic
		if (!menu_up){
			//TODO
		}
		else{
			//pause menu? probably not. maybe only do this if disconnected and entities just shouldn't update
		}

		//====================== DRAW SCREEN ====================

		//Clear screen
		window.clear();
		//Draw stuff
		drawQ.drawToWindow(window);
		//Update Window
		window.display();
	}
	
	closesocket(clientSocket);
	WSACleanup();
	exit(0);
}
