#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string>
#include "socketConnection.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

SOCKET s; //Socket handle

//CONNECTTOHOST – Connects to a remote host
int connectToHost(int PortNo, char* IPAddress)
{
	//Start up Winsock…
	WSADATA wsadata;
	int error = WSAStartup(0x0202, &wsadata);

	//Did something happen?
	if (error)
	{
		printf("error1");
		return false;
	}

	//Did we get the right Winsock version?
	if (wsadata.wVersion != 0x0202)
	{
		WSACleanup(); //Clean up Winsock
		printf("error2");
		return false;
	}

	//Fill out the information needed to initialize a socket…
	SOCKADDR_IN target; //Socket address information
	target.sin_family = AF_INET; // address family Internet
	target.sin_port = htons(PortNo); //Port to connect on
	target.sin_addr.s_addr = inet_addr(IPAddress); //Target IP

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //Create socket
	if (s == INVALID_SOCKET)
	{
		printf("error3");
		return false; //Couldn't create the socket
	}

	//Try connecting...
	if (connect(s, (SOCKADDR *)&target, sizeof(target)) == SOCKET_ERROR)
	{
		printf("error4");
		return false; //Couldn't connect
	}
	else
		printf("Socket connection successful\n");
	return true; //Success
}

void socketSend(const char* command, int len) {
	send(s, command, len, 0);
}

string socketResponse() {
	int iResult;
	char  buffer[200];
	string response;
	iResult = recv(s, buffer, sizeof(buffer), 0);
	response.append(buffer, buffer + iResult);
	return response;
}

//CLOSECONNECTION – shuts down the socket and closes any connection on it
void closeConnection()
{
	//Close the socket if it exists
	if (s) {
		closesocket(s);
	}
	WSACleanup(); //Clean up Winsock
}