/*******************************************************************************
 * A simple UDP/IP server application
 ******************************************************************************/

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Windows.h"		// Entire Win32 API...
 // #include "winsock2.h"	// ...or Winsock alone
#include "ws2tcpip.h"		// getaddrinfo()

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#include <iostream>			// cout, cerr
#include <string>			// string

int main()
{
	constexpr uint16_t port = 9048;

	const std::string portString = std::to_string(port);


	// -------------------------------------------------------------------------
	// Start up Winsock, asking for version 2.2.
	//
	// WSAStartup()
	// -------------------------------------------------------------------------

	// This object holds the information about the version of Winsock that we
	// are using, which is not necessarily the version that we requested.
	WSADATA wsaData{};
	SecureZeroMemory(&wsaData, sizeof(wsaData));

	// Initialize Winsock. You must call WSACleanup when you are finished.
	// As this function uses a reference counter, for each call to WSAStartup,
	// you must call WSACleanup or suffer memory issues.
	int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (errorCode != NO_ERROR)
	{
		std::cerr << "WSAStartup() failed." << std::endl;
		return errorCode;
	}

	std::cout
		<< "Winsock version: "
		<< static_cast<int>(LOBYTE(wsaData.wVersion))
		<< "."
		<< static_cast<int>(HIBYTE(wsaData.wVersion))
		<< "\n"
		<< std::endl;


	// -------------------------------------------------------------------------
	// Resolve own host name into IP addresses (in a singly-linked list).
	//
	// getaddrinfo()
	// -------------------------------------------------------------------------

	// Object hints indicates which protocols to use to fill in the info.
	addrinfo hints{};
	SecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;			// IPv4
	// For TCP use SOCK_STREAM instead of SOCK_DGRAM.
	hints.ai_socktype = SOCK_DGRAM;		// Best effort
	// Could be 0 for autodetect, but best effort over IPv4 is always UDP.
	hints.ai_protocol = IPPROTO_UDP;	// UDP
	char hostname[30];
	gethostname(hostname, 30);
	addrinfo* info = nullptr;
	errorCode = getaddrinfo(hostname, portString.c_str(), &hints, &info);
	if ((errorCode) || (info == nullptr))
	{
		std::cerr << "getaddrinfo() failed." << std::endl;
		WSACleanup();
		return errorCode;
	}


	// -------------------------------------------------------------------------
	// Create a socket and bind it to own network interface controller.
	//
	// socket()
	// bind()
	// -------------------------------------------------------------------------

	SOCKET serverSocket = socket(
		hints.ai_family,
		hints.ai_socktype,
		hints.ai_protocol);
	if (serverSocket == INVALID_SOCKET)
	{
		std::cerr << "socket() failed." << std::endl;
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}

	errorCode = bind(
		serverSocket,
		info->ai_addr,
		static_cast<int>(info->ai_addrlen));
	if (errorCode != NO_ERROR)
	{
		std::cerr << "bind() failed." << std::endl;
		closesocket(serverSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 2;
	}

	struct sockaddr_in name;
	socklen_t namelen = sizeof(name);
	int err = getsockname(serverSocket, (struct sockaddr*) & name, &namelen);

	char localIPAddr[50];
	const char* p = inet_ntop(AF_INET, &name.sin_addr, localIPAddr, 50);
	std::cout << "local ip address:"  << localIPAddr << std::endl;
	freeaddrinfo(info);

	constexpr size_t BUFFER_SIZE = 65536;
	char buffer[BUFFER_SIZE];

	while (true)
	{
		sockaddr clientAddress{};
		SecureZeroMemory(&clientAddress, sizeof(clientAddress));
		int clientAddressSize = sizeof(clientAddress);
		const int bytesReceived = recvfrom(serverSocket,
			buffer,
			BUFFER_SIZE - 1,
			0,
			&clientAddress,
			&clientAddressSize);
		if (bytesReceived == SOCKET_ERROR)
		{
			std::cerr << "recvfrom() failed." << std::endl;
			break;
		}
		if (bytesReceived == 0)
		{
			std::cerr << "Graceful shutdown." << std::endl;
			break;
		}
		buffer[bytesReceived] = '\0';
		std::string text(buffer, bytesReceived - 1);

		std::cout
			<< "Text received:  " << text << "\n"
			<< "Bytes received: " << bytesReceived << "\n"
			<< std::endl;

		if (text == "quit")
		{
			std::cerr << "Graceful shutdown." << std::endl;
			break;
		}

		const int bytesSent = sendto(serverSocket, buffer, bytesReceived, 0, &clientAddress, clientAddressSize);
		if (bytesSent == SOCKET_ERROR)
		{
			std::cerr << "send() failed." << std::endl;
			break;
		}
	}


	// -------------------------------------------------------------------------
	// Close socket and clean-up after Winsock.
	//
	// WSACleanup()
	// -------------------------------------------------------------------------

	closesocket(serverSocket);
	WSACleanup();
}
