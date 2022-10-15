#include "roomserver.h"

RoomServer::RoomServer(char* name, char* port) {

	// Initalize memory
	memset(this->name, 0, MAX_NAME_LENGTH);
	memset(this->port, 0, MAX_PORT_LENGTH);

	strcpy_s(this->name, MAX_NAME_LENGTH, name);
	strcpy_s(this->port, MAX_PORT_LENGTH, port);

	for (int i = 0; i < MAX_CLIENT_NUM; i++) {
		this->ClientSocket[i] = INVALID_SOCKET;
		this->isConnected[i] = false;
		memset(this->client_name[i], 0, MAX_NAME_LENGTH);
	}

	ZeroMemory(&this->hints, sizeof(this->hints));
	this->hints.ai_family = AF_INET;
	this->hints.ai_socktype = SOCK_STREAM;
	this->hints.ai_protocol = IPPROTO_TCP;
	this->hints.ai_flags = AI_PASSIVE;
}

int RoomServer::create_server(RoomServer*& roomServer, char* name, char* port) {

	// Use default port
	if (port == NULL) {
		char default_port[6] = DEFAULT_PORT;
		printf("[INFO]Created roomServer with deault port: %s.\n", default_port);
		roomServer = new RoomServer(name, default_port);
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
	roomServer = new RoomServer(name, port);
	return 0;
}

int RoomServer::init_server()
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

int RoomServer::bind_server()
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

void RoomServer::accept_server(RoomServer* roomServer)
{
	int current_id = 1;

	// Accept loop
	while (current_id <= MAX_CLIENT_NUM) {

		// Accept a client socket
		roomServer->ClientSocket[current_id] = accept(roomServer->ListenSocket, NULL, NULL);
		if (roomServer->ClientSocket[current_id] == INVALID_SOCKET) {
			printf("[ERROR]Accept failed with error: %d\n", WSAGetLastError());
			closesocket(roomServer->ListenSocket);
			return;
		}

		// Send server's name to client
		int iSendResult = send(roomServer->ClientSocket[current_id], roomServer->name, MAX_NAME_LENGTH, 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("[ERROR]Send failed with error: %d\n", WSAGetLastError());
			closesocket(roomServer->ClientSocket[current_id]);
			roomServer->ClientSocket[current_id] = INVALID_SOCKET;
			current_id++;
			continue;
		}

		// Recieve client's name, as a handshake
		char name_buf[MAX_NAME_LENGTH];
		memset(name_buf, 0, MAX_NAME_LENGTH);
		int iResult = recv(roomServer->ClientSocket[current_id], name_buf, MAX_NAME_LENGTH, 0);
		if (iResult > 0) {
			/*printf("Bytes received: %d\n", iResult);*/
			strcpy_s(roomServer->client_name[current_id], MAX_NAME_LENGTH, name_buf);
		}
		else if (iResult == 0)
			printf("[INFO]Connection closing...\n");
		else {
			printf("[ERROR]Recv failed with error: %d\n", WSAGetLastError());
			closesocket(roomServer->ClientSocket[current_id]);
			roomServer->ClientSocket[current_id] = INVALID_SOCKET;
			current_id++;
			continue;
		}

		// Set status
		roomServer->isConnected[current_id] = true;
		printf("[SUCCESS]Setup client-%d connection successfully!\n", current_id);

		// Start listen message thread
		std::thread listen_thread(RoomServer::listen_message, roomServer, current_id);
		listen_thread.detach();

		// Broadcast notice
		char notice_disconnect[2] = { ')', '\0' };
		iResult = roomServer->broadcast(notice_disconnect, current_id);
		if (iResult != 0) {
			printf("[ERROR]Login notice can not be broadcasted to other clients.\n");
		}

		current_id++;
	}

	printf("[ERROR]Too much clients online!\n");
	return;
}

void RoomServer::listen_message(RoomServer* roomServer, int id)
{

	// Check
	if (id < 1 || id > MAX_CLIENT_NUM) {
		printf("[ERROR]Invalid client id: %d.\n", id);
		return;
	}

	int iListenResult = 0;
	// Max size: MESSAGE "^" TIME
	char recvbuf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2];
	
	// Receive until the peer shuts down the connection
	do {

		// Zero memory
		memset(recvbuf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2);
		iListenResult = recv(roomServer->ClientSocket[id], recvbuf, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2, 0);
		if (iListenResult > 0) {
			// Start with '@', change name
			if (recvbuf[0] == '@') {
				strcpy_s(roomServer->client_name[id], MAX_NAME_LENGTH, recvbuf + 1);
			}
			// Start with '#', client's messgae with time stamp
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
				
				// Broadcast to other clients
				printf("%s@%s: %s\n", timestamp, roomServer->client_name[id], recvbuf + 1);
				recvbuf[0] = '*';
				int iResult = roomServer->broadcast(recvbuf, id);
				if (iResult != 0) {
					printf("[ERROR]Meesage from client-%d can not be broadcasted to other clients.\n", id);
				}
				else {
					printf("[SUCCESS]Meesage from client-%d has been broadcasted to other clients.\n", id);
				}
			}

			// Start with '$', one client will disconnect
			else if (recvbuf[0] == '$') {
				printf("[INFO]One client will disconnect.\n");
				char reply_disconnect[2] = { '%', '\0' };
				int iResult = roomServer->send_message(reply_disconnect, id);
				if (iResult != 0) {
					printf("[ERROR]Reply command can not be sent to client.\n");
				}
				else {
					printf("[SUCCESS]Reply command has been sent to client.\n");
				}
				char notice_disconnect[2] = {'&', '\0'};
				iResult = roomServer->broadcast(notice_disconnect, id);
				if (iResult != 0) {
					printf("[ERROR]Disconnection notice can not be broadcasted to other clients.\n");
				}
				iResult = roomServer->close_client(id);
				if (iResult != 0) {
					printf("[ERROR]Client-%d closing failed.\n", id);
				}
			}

			// Start with '%', relpy after '$', now this server can disconnect and clean
			else if (recvbuf[0] == '%') {
				printf("[INFO]Client-%d has closed\n", id);
				disconnect = true;
				return;
			}
		}
		else if (iListenResult == 0)
			printf("[INFO]Connection closing...\n");
		else {
			printf("[ERROR]Recv failed with error: %d\n", WSAGetLastError());
			disconnect = true;
			return;
		}

	} while (iListenResult > 0);
	return;
}

int RoomServer::send_message(char* message, int id)
{

	if (id < 1 || id > MAX_CLIENT_NUM) {
		printf("[ERROR]Invalid client id: %d.\n", id);
		return 1;
	}
	if (message == NULL) {
		return 1;
	}

	int iSendResult = send(this->ClientSocket[id], message, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("[ERROR]Send failed with error: %d\n", WSAGetLastError());
		close_client(id);
		return 1;
	}
	return 0;
}

int RoomServer::broadcast(char* message)
{

	if (message == NULL) {
		printf("[ERROR]You can not send empty message.\n");
		return 1;
	}

	// Max size: MESSAGE "^" TIME "(" NAME
	int i = MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 2;
	char broadcast_buf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3];
	memset(broadcast_buf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3);
	memmove(broadcast_buf, message, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2);

	// Append name (server's name)
	while (broadcast_buf[i] == '\0' && i > 0) {
		i--;
	}
	if (i == 0) {
		printf("[ERROR]Please input legal broadcast message\n");
		return 1;
	}
	if (this->name == NULL) {
		printf("[ERROR]RoomServer's name not found\n");
		return 1;
	}
	if (i >= MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH) {
		printf("[ERROR]Please input legal broadcast message\n");
		return 1;
	}
	broadcast_buf[i + 2] = '(';
	char name_buf[MAX_NAME_LENGTH];
	memset(name_buf, 0, MAX_NAME_LENGTH);
	strcpy_s(name_buf, MAX_NAME_LENGTH, this->name);
	memmove(broadcast_buf + i + 3, name_buf, MAX_NAME_LENGTH);
	
	// Send all
	for (int j = 1; j <= MAX_CLIENT_NUM; j++) {
		int iSendResult = send(this->ClientSocket[j], broadcast_buf, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3, 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("[ERROR]Send to client-%d failed with error: %d\n", j, WSAGetLastError());
			int iResult = close_client(j);
			if (iResult != 0) {
				printf("[ERROR]Client can not be closed correctly\n");
			}
			continue;
		}
	}
	return 0;
}

int RoomServer::broadcast(char* message, int id)
{
	if (id < 1 || id > MAX_CLIENT_NUM) {
		printf("[ERROR]Invalid client id: %d.\n", id);
		return 1;
	}
	if (message == NULL) {
		return 1;
	}

	// Max size: MESSAGE "^" TIME "(" NAME
	int i = MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 2;
	char broadcast_buf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3];
	memset(broadcast_buf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3);
	memmove(broadcast_buf, message, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2);

	// Append name (client-id's name)
	while (i > 0 && broadcast_buf[i] == '\0') {
		i--;
	}
	if (i < 0) {
		printf("[ERROR]Please input legal broadcast message\n");
		return 1;
	}
	if (this->client_name[id] == NULL) {
		printf("[ERROR]Client's name not found\n");
		return 1;
	}
	if (i >= MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH) {
		printf("[ERROR]Please input legal broadcast message\n");
		return 1;
	}
	broadcast_buf[i + 2] = '(';
	char name_buf[MAX_NAME_LENGTH];
	memset(name_buf, 0, MAX_NAME_LENGTH);
	strcpy_s(name_buf, MAX_NAME_LENGTH, this->client_name[id]);
	memmove(broadcast_buf + i + 3, name_buf, MAX_NAME_LENGTH);
	
	// Send all except id
	for (int j = 1; j <= MAX_CLIENT_NUM; j++) {
		if (j != id && this->ClientSocket[j] != INVALID_SOCKET && this->isConnected[j]) {
			int iSendResult = send(this->ClientSocket[j], broadcast_buf, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3, 0);
			if (iSendResult == SOCKET_ERROR) {
				printf("[ERROR]Send to client-%d failed with error: %d\n", j, WSAGetLastError());
				int iResult = close_client(j);
				if (iResult != 0) {
					printf("[ERROR]Client can not be closed correctly\n");
				}
				continue;
			}
		}
	}
	return 0;
}

int RoomServer::close_client(int id) {

	// Close and clean
	if (id < 1 || id > MAX_CLIENT_NUM) {
		printf("[ERROR]Invalid client id: %d.\n", id);
		return 1;
	}
	memset(this->client_name[id], 0, MAX_NAME_LENGTH);
	this->isConnected[id] = false;
	if (this->ClientSocket[id] == INVALID_SOCKET || this->ClientSocket[id] == NULL) {
		return 0;
	}
	int iResult = closesocket(this->ClientSocket[id]);
	if (iResult != 0) {
		printf("[ERROR]Closing client-%d failed\n", id);
		return 1;
	}
	return 0;
}

int RoomServer::close_server() {

	// shutdown the connection since we're done
	for (int i = 1; i <= MAX_CLIENT_NUM; i++) {
		if (this->isConnected[i]) {
			int iResult = closesocket(this->ClientSocket[i]);
			if (iResult != 0) {
				/*printf("[ERROR]Server closed socket with error: %d\n", WSAGetLastError());*/
			}
		}
	}
	int iResult = closesocket(this->ListenSocket);
	if (iResult != 0) {
		/*printf("[ERROR]Server closed socket with error: %d\n", WSAGetLastError());*/
	}
	for (int i = 1; i <= MAX_CLIENT_NUM; i++) {
		close_client(i);
	}
	return 0;
}

int RoomServer::change_name(char* name)
{
	if (name == NULL || name[0] == '\0') {
		const char* default_server_name = DEFAULT_ROOMSERVER_NAME;
		strcpy_s(this->name, MAX_NAME_LENGTH, default_server_name);
	}
	else {
		strcpy_s(this->name, MAX_NAME_LENGTH, name);
	}
	return 0;
}