#include "client.h"

Client::Client(char* name, char* port, char* ip) {

	strcpy_s(this->name, MAX_NAME_LENGTH, name);
	strcpy_s(this->port, MAX_PORT_LENGTH, port);
	strcpy_s(this->ip, MAX_IP_LENGTH, ip);

	ZeroMemory(&this->hints, sizeof(this->hints));
	this->hints.ai_family = AF_UNSPEC;
	this->hints.ai_socktype = SOCK_STREAM;
	this->hints.ai_protocol = IPPROTO_TCP;
}

int Client::create_client(Client* &client, char* name, char* port, char* ip) {

	if (!solve_ip(ip)) {
		return 1;
	}
	if (port == NULL) {
		char default_port[6] = DEFAULT_PORT;
		client = new Client(name, default_port, ip);
		return 0;
	}
	else {
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
	}
	client = new Client(name, port, ip);
	return 0;
}

bool Client::solve_ip(char* ip)
{
	if (ip == NULL) {
		return false;
	}
	char temp_ip[16];
	int p_num = 0;
	int length = 0;
	int p_pos[3];
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
		printf("getaddrinfo failed with error: %d\n", iResult);
		return 1;
	}

	// Attempt to connect to an address until one succeeds
	for (this->ptr = this->result; this->ptr != NULL; this->ptr = this->ptr->ai_next) {

		// Create a SOCKET for connecting to server
		this->ConnectSocket = socket(this->ptr->ai_family, this->ptr->ai_socktype, this->ptr->ai_protocol);
		if (this->ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			return 1;
		}

		// Connect to server.
		iResult = connect(this->ConnectSocket, this->ptr->ai_addr, (int)this->ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(this->ConnectSocket);
			this->ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(this->result);

	if (this->ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		return 1;
	}

	int iSendResult = send(this->ConnectSocket, this->name, MAX_NAME_LENGTH, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(this->ConnectSocket);
		return 1;
	}

	iResult = recv(this->ConnectSocket, this->server_name, MAX_NAME_LENGTH, 0);
	if (iResult > 0) {
		printf("Bytes received: %d\n", iResult);
	}
	else if (iResult == 0)
		printf("Connection closing...\n");
	else {
		printf("recv failed with error: %d\n", WSAGetLastError());
		closesocket(this->ConnectSocket);
		return 1;
	}

	this->isConnected = true;
	return 0;
}

void Client::listen_message(Client* client)
{
	int iResult = 0;
	char recvbuf[MAX_MESSAGE_LENGTH];
	// Receive until the peer shuts down the connection
	do {

		iResult = recv(client->ConnectSocket, recvbuf, MAX_MESSAGE_LENGTH, 0);
		if (iResult > 0) {
			if (recvbuf[0] == '@') {
				strcpy_s(client->server_name, MAX_NAME_LENGTH, recvbuf + 1);
			}
			else if (recvbuf[0] == '#') {
				printf("%s: %s\n", client->server_name, recvbuf + 1);
			}
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
					printf("[ERROR]Closing client failed\n");
					return;
				}
				else {
					client->isDeleted = true;
					printf("[SUCCESS]Close server successfully\n");
					return;
				}
			}
			else if (recvbuf[0] == '%') {
				printf("[INFO]Server has restarted, current client will close.\n");
				disconnect = true;
				return;
			}
		}
		else if (iResult == 0)
			printf("Connection closing...\n");
		else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			closesocket(client->ConnectSocket);
			return;
		}

	} while (iResult > 0);
	return;
}

int Client::send_message(char* message)
{
	if (message == NULL) {
		return 1;
	}
	int i = 0;
	while (*(message + i) != '\0')
		i++;
	if (i > MAX_MESSAGE_LENGTH) {
		return 1;
	}
	int iSendResult = send(this->ConnectSocket, message, MAX_MESSAGE_LENGTH, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
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
		const char* default_client_name = "Default client\0";
		strcpy_s(this->name, MAX_NAME_LENGTH, default_client_name);
	}
	else {
		strcpy_s(this->name, MAX_NAME_LENGTH, name);
	}
	return 0;
}
