#include "server.h"

Server::Server(char* name, char* port) {

	// Initalize memory
	memset(this->name, 0, MAX_NAME_LENGTH);
	memset(this->client_name, 0, MAX_NAME_LENGTH);
	memset(this->port, 0, MAX_PORT_LENGTH);

	strcpy_s(this->name, MAX_NAME_LENGTH, name);
	strcpy_s(this->port, MAX_PORT_LENGTH, port);

	ZeroMemory(&this->hints, sizeof(this->hints));
	this->hints.ai_family = AF_INET;
	this->hints.ai_socktype = SOCK_STREAM;
	this->hints.ai_protocol = IPPROTO_TCP;
	this->hints.ai_flags = AI_PASSIVE;
}

int Server::create_server(Server* &server, char* name, char* port) {

	// Use default port
	if (port == NULL) {
		char default_port[6] = DEFAULT_PORT;
		printf("[INFO]Created server with deault port: %s.\n", default_port);
		server = new Server(name, default_port);
		return 0;
	}

	// Resolve port
	int port_num = atoi(port);
	if (port_num == 0) {
		return 1;
	}
	if (port_num > 0 && port_num < 1025) {
		return 1;
	}
	if (port_num > 65535) {
		return 1;
	}

	// Create server with ref pointer
	server = new Server(name, port);
	return 0;
}

int Server::init_server()
{

	// Setup addrinfo
	int iResult = getaddrinfo(NULL, this->port, &(this->hints), &(this->result));
	if (iResult != 0) {
		printf("[ERROR]Getaddrinfo failed with error: %d\n", iResult);
		return 1;
	}

	// Create a SOCKET for the server to listen for client connections.
	this->ListenSocket = socket(this->result->ai_family, this->result->ai_socktype, this->result->ai_protocol);
	if (this->ListenSocket == INVALID_SOCKET) {
		printf("[ERROR]Socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(this->result);
		return 1;
	}
	return 0;
}

int Server::bind_server()
{
	
	// Bind with this addr
	int iResult = bind(this->ListenSocket, this->result->ai_addr, (int)this->result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("[ERROR]Bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(this->result);
		closesocket(this->ListenSocket);
		return 1;
	}

	freeaddrinfo(this->result);

	// Setup the TCP listening socket
	iResult = listen(this->ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("[ERROR]Listen failed with error: %d\n", WSAGetLastError());
		closesocket(this->ListenSocket);
		return 1;
	}
	return 0;
}

void Server::accept_server(Server* server)
{
	
	// Accept a client socket
	server->ClientSocket = accept(server->ListenSocket, NULL, NULL);
	if (server->ClientSocket == INVALID_SOCKET) {
		printf("[ERROR]Accept failed with error: %d\n", WSAGetLastError());
		closesocket(server->ListenSocket);
		return;
	}

	// No longer need server socket
	closesocket(server->ListenSocket);

	// Send server's name to client
	int iSendResult = send(server->ClientSocket, server->name, MAX_NAME_LENGTH, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("[ERROR]Send failed with error: %d\n", WSAGetLastError());
		closesocket(server->ClientSocket);
		return;
	}

	// Recieve client's name, as a handshake
	char name_buf[MAX_NAME_LENGTH];
	memset(name_buf, 0, MAX_NAME_LENGTH);
	int iResult = recv(server->ClientSocket, name_buf, MAX_NAME_LENGTH, 0);
	if (iResult > 0) {
		/*printf("Bytes received: %d\n", iResult);*/
		strcpy_s(server->client_name, MAX_NAME_LENGTH, name_buf);
	}
	else if (iResult == 0)
		printf("[INFO]Connection closing...\n");
	else {
		printf("[ERROR]Recv failed with error: %d\n", WSAGetLastError());
		closesocket(server->ClientSocket);
		return;
	}

	// Set status
	server->isConnected = true;
	printf("[SUCCESS]Setup connection successfully!\n");

	// Start listen message thread
	std::thread listen_thread(Server::listen_message, server);
	listen_thread.detach();
	return;
}

void Server::listen_message(Server* server)
{
	int iResult = 0;
	// Max size: MESSAGE "^" TIME
	char recvbuf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2];
	
	// Receive until the peer shuts down the connection
	do {

		// Zero memory
		memset(recvbuf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2);
		iResult = recv(server->ClientSocket, recvbuf, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2, 0);
		if (iResult > 0) {

			// Start with '@', change name
			if (recvbuf[0] == '@') {
				strcpy_s(server->client_name, MAX_NAME_LENGTH, recvbuf + 1);
			}

			// Start with '#', peer's messgae with time stamp
			else if (recvbuf[0] == '#') {
				int i = 0;
				char timestamp[MAX_TIME_LENGTH];
				memset(timestamp, 0, MAX_TIME_LENGTH);
				while (i < MAX_MESSAGE_LENGTH + 1) {
					if (recvbuf[i] == '\0' && recvbuf[i + 1] == '^')
						break;
					i++;
				}
				if (recvbuf[i + 2] == '\0') {
					printf("[ERROR]Recieve wrong timestamp.\n");
					continue;
				}
				strcpy_s(timestamp, MAX_TIME_LENGTH, recvbuf + i + 2);
				printf("%s@%s: %s\n", timestamp, server->client_name, recvbuf + 1);
			}	

			// Start with '$', peer will disconnect
			else if (recvbuf[0] == '$') {
				printf("[INFO]Client will disconnect, current server will restart.\n");
				char reply_disconnect[2] = { '%', '\0' };
				iResult = server->send_message(reply_disconnect);
				if (iResult != 0) {
					printf("[ERROR]Reply command can not be sent to client.\n");
				}
				else {
					printf("[SUCCESS]Reply command has been sent to client.\n");
				}
				int iCloseResult = server->close_server();
				if (iCloseResult != 0) {
					server->isDeleted = true;
					printf("[ERROR]Closing server failed\n");
					return;
				} else {
					char temp_name[MAX_NAME_LENGTH];
					char temp_port[MAX_PORT_LENGTH];
					strcpy(temp_name, server->name);
					strcpy(temp_port, server->port);
					server->isDeleted = true;
					printf("[SUCCESS]Close server successfully\n");
					return;
				}
			}

			// Start with '%', relpy after '$', now this server can disconnect and clean
			else if (recvbuf[0] == '%') {
				printf("[INFO]Client has closed, current server will close.\n");
				disconnect = true;
				return;
			}
		}
		else if (iResult == 0)
			printf("[INFO]Connection closing...\n");
		else {
			printf("[ERROR]Recv failed with error: %d\n", WSAGetLastError());
			closesocket(server->ClientSocket);
			return;
		}

	} while (iResult > 0);
	return;
}

int Server::send_message(char* message)
{

	if (message == NULL) {
		printf("[ERROR]You can not send empty message.\n");
		return 1;
	}

	int iSendResult = send(this->ClientSocket, message, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("[ERROR]Send failed with error: %d\n", WSAGetLastError());
		closesocket(this->ClientSocket);
		return 1;
	}
	return 0;
}

int Server::close_server()
{
	// shutdown the connection since we're done
	if (this->isConnected) {

		int iResult = closesocket(this->ClientSocket);
		if (iResult != 0) {
			/*printf("[ERROR]Server closed socket with error: %d\n", WSAGetLastError());*/
		}
	}
	int iResult = closesocket(this->ListenSocket);
	if (iResult != 0) {
		/*printf("[ERROR]Server closed socket with error: %d\n", WSAGetLastError());*/
	}
	return 0;
}

int Server::change_name(char* name)
{
	if (name == NULL || name[0] == '\0') {
		const char* default_server_name = DEFAULT_SERVER_NAME;
		strcpy_s(this->name, MAX_NAME_LENGTH, default_server_name);
	}
	else {
		strcpy_s(this->name, MAX_NAME_LENGTH, name);
	}
	return 0;
}
