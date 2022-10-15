#include "client.h"

Client::Client(char* name, char* port, char* ip) {

	// Initalize memory
	memset(this->name, 0, MAX_NAME_LENGTH);
	memset(this->server_name, 0, MAX_NAME_LENGTH);
	memset(this->port, 0, MAX_PORT_LENGTH);
	memset(this->ip, 0, MAX_IP_LENGTH);

	strcpy_s(this->name, MAX_NAME_LENGTH, name);
	strcpy_s(this->port, MAX_PORT_LENGTH, port);
	strcpy_s(this->ip, MAX_IP_LENGTH, ip);

	ZeroMemory(&this->hints, sizeof(this->hints));
	this->hints.ai_family = AF_UNSPEC;
	this->hints.ai_socktype = SOCK_STREAM;
	this->hints.ai_protocol = IPPROTO_TCP;
}

int Client::create_client(Client* &client, char* name, char* port, char* ip) {

	// Input wrong ip address
	if (!solve_ip(ip)) {
		printf("[ERROR]Can not recognize ip.\n");
		return 1;
	}

	// Resolve port
	else {
		int port_num = atoi(port);
		if (port_num == 0) {
			printf("[ERROR]Illegal port: %s.\n", ip);
			return 1;
		}
		if (port_num > 0 && port_num < 1025) {
			printf("[ERROR]Illegal port: %s.\n", ip);
			return 1;
		}
		if (port_num > 65535) {
			printf("[ERROR]Illegal port: %s.\n", ip);
			return 1;
		}
	}

	// Use default ip address
	if (port == NULL) {
		char default_port[6] = DEFAULT_PORT;
		printf("[INFO]Created client with deault port: %s.\n", default_port);
		client = new Client(name, default_port, ip);
		return 0;
	}

	// Create client with ref pointer
	client = new Client(name, port, ip);
	return 0;
}

bool Client::solve_ip(char* ip)
{
	// Only for ipv4
	if (ip == NULL) {
		return false;
	}
	char temp_ip[16];
	int p_num = 0;
	int length = 0;
	int p_pos[3];
	// Split with '.' dot
	while (ip[length] != '\0') {
		if (ip[length] != '.' && (ip[length] < '0' || ip[length] > '9')) {
			return false;
		}
		if (ip[length] == '.') {
			temp_ip[length] = '\0';
			p_pos[p_num] = length;
			p_num++;
		}
		temp_ip[length] = ip[length];
		length++;
	}
	if (p_num != 3) {
		return false;
	}
	int ip_0 = atoi(&temp_ip[0] + 1);
	int ip_1 = atoi(&temp_ip[p_pos[0]] + 1);
	int ip_2 = atoi(&temp_ip[p_pos[1]] + 1);
	int ip_3 = atoi(&temp_ip[p_pos[2]] + 1);
	// 1.0.0.0 ~ 254.254.254.254
	if (ip_0 < 1 || ip_0 > 254) {
		return false;
	}
	if (ip_1 < 0 || ip_1 > 254) {
		return false;
	}
	if (ip_2 < 0 || ip_2 > 254) {
		return false;
	}
	if (ip_3 < 0 || ip_3 > 254) {
		return false;
	}
	return true;
}

int Client::start_connect()
{
	// Resolve the server address and port
	int iResult = getaddrinfo(this->ip, this->port, &this->hints, &this->result);
	if (iResult != 0) {
		printf("[ERROR]Getaddrinfo failed with error: %d.\n", iResult);
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	// In winsock, server will return a linked list of ip address
	for (this->ptr = this->result; this->ptr != NULL; this->ptr = this->ptr->ai_next) {

		// Create a SOCKET for connecting to server
		this->ConnectSocket = socket(this->ptr->ai_family, this->ptr->ai_socktype, this->ptr->ai_protocol);
		if (this->ConnectSocket == INVALID_SOCKET) {
			printf("[ERROR]Socket failed with error: %ld.\n", WSAGetLastError());
			return 1;
		}

		// Connect to server.
		iResult = connect(this->ConnectSocket, this->ptr->ai_addr, (int)this->ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(this->ConnectSocket);
			this->ConnectSocket = INVALID_SOCKET;
			printf("[ERROR]Can not connect to server's socket with error: %ld.\n", WSAGetLastError());
			continue;
		}
		break;
	}

	freeaddrinfo(this->result);

	if (this->ConnectSocket == INVALID_SOCKET) {
		printf("[ERROR]Unable to connect to server!\n");
		return 1;
	}

	// Send client's name to server
	int iSendResult = send(this->ConnectSocket, this->name, MAX_NAME_LENGTH, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("[ERROR]Send failed with error: %d\n", WSAGetLastError());
		closesocket(this->ConnectSocket);
		return 1;
	}

	// Recieve server's name, as a handshake
	char name_buf[MAX_NAME_LENGTH];
	memset(name_buf, 0, MAX_NAME_LENGTH);
	iResult = recv(this->ConnectSocket, name_buf, MAX_NAME_LENGTH, 0);
	if (iResult > 0) {
		/*printf("[INFO]Bytes received: %d\n", iResult);*/
		strcpy_s(this->server_name, MAX_NAME_LENGTH, name_buf);
	}
	else if (iResult == 0)
		printf("[INFO]Connection closing...\n");
	else {
		printf("[ERROR]Recv failed with error: %d.\n", WSAGetLastError());
		closesocket(this->ConnectSocket);
		return 1;
	}

	// Set status
	this->isConnected = true;
	return 0;
}

void Client::listen_message(Client* client)
{
	int iResult = 0;
	// Max size: MESSAGE "^" TIME "(" NAME
	char recvbuf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3];
	
	// Receive until the peer shuts down the connection
	do {

		// Zero memory
		memset(recvbuf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3);
		iResult = recv(client->ConnectSocket, recvbuf, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + MAX_NAME_LENGTH + 3, 0);
		if (iResult > 0) {
			// Start with '@', change name
			if (recvbuf[0] == '@') {
				strcpy_s(client->server_name, MAX_NAME_LENGTH, recvbuf + 1);
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
				printf("%s@%s: %s\n", timestamp, client->server_name, recvbuf + 1);
			}
			// Start with '$', peer will disconnect
			else if (recvbuf[0] == '$') {
				printf("[INFO]Server will disconnect, current client will close.\n");
				char reply_disconnect[2] = { '%', '\0' };
				iResult = client->send_message(reply_disconnect);
				if (iResult != 0) {
					printf("[ERROR]Reply command can not be sent to server.\n");
				}
				else {
					printf("[SUCCESS]Reply command has been sent to server.\n");
				}
				int iCloseResult = client->close_client();
				if (iCloseResult != 0) {
					client->isDeleted = true;
					printf("[ERROR]Closing client failed.\n");
					return;
				}
				else {
					client->isDeleted = true;
					printf("[SUCCESS]Close connection successfully.\n");
					return;
				}
			}
			// Start with '%', relpy after '$', now this client can disconnect and clean
			else if (recvbuf[0] == '%') {
				printf("[INFO]Server has restarted, current client will close.\n");
				disconnect = true;
				return;
			}
			// Start with '*', message from chatroom with time and name
			else if (recvbuf[0] == '*') {
				int i = 0;
				char timestamp[MAX_TIME_LENGTH];
				char name_buf[MAX_NAME_LENGTH];
				memset(timestamp, 0, MAX_TIME_LENGTH);
				memset(name_buf, 0, MAX_NAME_LENGTH);
				while (i < MAX_MESSAGE_LENGTH + 1) {
					if (recvbuf[i] == '\0' && recvbuf[i + 1] == '^')
						break;
					i++;
				}
				strcpy_s(timestamp, MAX_TIME_LENGTH, recvbuf + i + 2);
				while (i < MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2) {
					if (recvbuf[i] == '\0' && recvbuf[i + 1] == '(')
						break;
					i++;
				}
				strcpy_s(name_buf, MAX_NAME_LENGTH, recvbuf + i + 2);
				printf("[INFO]This is a message from chatroom.\n");
				printf("%s@%s: %s\n", timestamp, name_buf, recvbuf + 1);
			}
			// Start with '&', one client in chatroom has disconnected
			else if (recvbuf[0] == '&') {
				int i = 0;
				char name_buf[MAX_NAME_LENGTH];
				memset(name_buf, 0, MAX_NAME_LENGTH);
				while (i < MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2) {
					if (recvbuf[i] == '\0' && recvbuf[i + 1] == '(')
						break;
					i++;
				}
				strcpy_s(name_buf, MAX_NAME_LENGTH, recvbuf + i + 2);
				printf("[INFO]Client called %s just disconnected.\n", name_buf);
			}
			// Start with ')', one client in chatroom has logined
			else if (recvbuf[0] == ')') {
				int i = 0;
				char name_buf[MAX_NAME_LENGTH];
				memset(name_buf, 0, MAX_NAME_LENGTH);
				while (i < MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2) {
					if (recvbuf[i] == '\0' && recvbuf[i + 1] == '(')
						break;
					i++;
				}
				strcpy_s(name_buf, MAX_NAME_LENGTH, recvbuf + i + 2);
				printf("[INFO]Client called %s just logined.\n", name_buf);
			}
		}
		else if (iResult == 0)
			printf("[INFO]Connection closing...\n");
		else {
			printf("[ERROR]Recv failed with error: %d.\n", WSAGetLastError());
			closesocket(client->ConnectSocket);
			return;
		}

	} while (iResult > 0);
	return;
}

int Client::send_message(char* message)
{

	if (message == NULL) {
		printf("[ERROR]You can not send empty message.\n");
		return 1;
	}

	int iSendResult = send(this->ConnectSocket, message, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("[ERROR]Send failed with error: %d.\n", WSAGetLastError());
		closesocket(this->ConnectSocket);
		return 1;
	}
	return 0;
}

int Client::close_client()
{
	// shutdown the connection since we're done
	int iResult = closesocket(this->ConnectSocket);
	if (iResult != 0) {
		/*printf("[ERROR]Server closed socket with error: %d\n", WSAGetLastError());*/
	}
	return 0;
}

int Client::change_name(char* name)
{

	if (name == NULL || name[0] == '\0') {
		const char* default_client_name = DEFAULT_CLIENT_NAME;
		strcpy_s(this->name, MAX_NAME_LENGTH, default_client_name);
	}
	else {
		strcpy_s(this->name, MAX_NAME_LENGTH, name);
	}
	return 0;
}
