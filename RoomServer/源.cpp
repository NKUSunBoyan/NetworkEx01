#define _CRT_SECURE_NO_WARNINGS

#include "roomserver.h"

#pragma comment(lib, "Ws2_32.lib")

bool disconnect = false;

// Start server and create accept thread
int start_roomserver(RoomServer* &roomServer, char* name, char* port) {

	RoomServer::create_server(roomServer, name, port);
	int iResult = 0;
	iResult = roomServer->init_server();
	if (iResult != 0) {
		printf("[ERROR]Server initialization failed.\n");
		return 1;
	}
	iResult = roomServer->bind_server();
	if (iResult != 0) {
		printf("[ERROR]Binding address failed.\n");
		return 1;
	}
	std::thread accept_thread(RoomServer::accept_server, roomServer);
	accept_thread.detach();

	return 0;
}

// Users input commands here
int bash() {

	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	disconnect = false;

	// Initialize local variables
	RoomServer* roomServer = NULL;
	char buf[MAX_MESSAGE_LENGTH + 10];
	char name[MAX_NAME_LENGTH] = DEFAULT_ROOMSERVER_NAME;
	char port[MAX_PORT_LENGTH] = DEFAULT_PORT;

	memset(buf, 0, MAX_MESSAGE_LENGTH + 10);

	// Loop
	while (true) {

		printf("-------------RoomServer--------------\n");
		memset(buf, 0, MAX_MESSAGE_LENGTH + 10);
		std::cin.getline(buf, MAX_MESSAGE_LENGTH);
		int i = 0;
		// Clean synchronously
		if (roomServer != NULL && roomServer->isDeleted) {
			delete roomServer;
			roomServer = NULL;
		}

		// Recognize simple commands
		// Ignore prefix
		while (buf[i] != '\0') {
			if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n')
				i++;
			else
				break;
		}

		// Start with '-'
		if (buf[i] == '\0' || buf[i] != '-') {
			printf("[ERROR]Please input correct command, start with '-'.\n");
			continue;
		}

		// Start with 'i', setup server
		if (buf[i + 1] == 'i') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (roomServer != NULL) {
				printf("[ERROR]RoomServer is already active.\n");
				continue;
			}
			iResult = start_roomserver(roomServer, name, port);
			if (iResult != 0) {
				printf("[ERROR]Start error!\n");
				return 1;
			}
			printf("[SUCCESS]RoomServer is activated successfully.\n");
		}

		// Start with 'm', send message to the peer
		else if (buf[i + 1] == 'm') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (roomServer == NULL) {
				printf("[ERROR]RoomServer is not active.\n");
				continue;
			}

			// Initial buf
			char message_buf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2];
			memset(message_buf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2);
			message_buf[0] = '#';
			char id_buf[MAX_ID_LENGTH];
			memset(id_buf, 0, MAX_ID_LENGTH);

			// Split message
			int message_start = i;
			while (buf[message_start] != '\0') {
				if (buf[message_start] == ' ' || buf[message_start] == '\t' || buf[message_start] == '\n')
					break;
				message_start++;
			}
			while (buf[message_start] != '\0') {
				if (buf[message_start] == ' ' || buf[message_start] == '\t' || buf[message_start] == '\n')
					message_start++;
				else
					break;
			}
			int message_end = message_start;
			while (buf[message_end] != '\0') {
				if (buf[message_end] == ' ' || buf[message_end] == '\t' || buf[message_end] == '\n')
					break;
				message_end++;
			}
			if (message_end - message_start <= 0) {
				printf("[ERROR]Please input right message.\n");
				continue;
			}
			strncpy(message_buf + 1, buf + message_start, message_end - message_start);
			if (message_end >= MAX_MESSAGE_LENGTH) {
				printf("[ERROR]Message is too long: %d.\n", message_end);
				continue;
			}

			// Split id
			int id_start = message_end;
			while (buf[id_start] != '\0') {
				if (buf[id_start] == ' ' || buf[id_start] == '\t' || buf[id_start] == '\n')
					id_start++;
				else
					break;
			}
			int id_end = id_start;
			while (buf[id_end] != '\0') {
				if (buf[id_end] == ' ' || buf[id_end] == '\t' || buf[id_end] == '\n')
					break;
				id_end++;
			}
			if (id_end - id_start <= 0) {
				printf("[ERROR]Please input right id.\n");
				continue;
			}
			strncpy(id_buf, buf + id_start, id_end - id_start);
			int id = atoi(id_buf);
			if (id < 1 || id > MAX_CLIENT_NUM) {
				printf("[ERROR]Please input correct id: %d.", id);
				continue;
			}
			if (roomServer->isConnected[id] == false) {
				printf("[ERROR]Connection of client-%d is not existed.", id);
				continue;
			}

			// Get timestamp
			char timestamp[MAX_TIME_LENGTH];
			memset(timestamp, 0, MAX_TIME_LENGTH);
			SYSTEMTIME time;
			GetLocalTime(&time);
			sprintf(timestamp, "%hd/%hd/%hd %hd:%hd:%hd", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
			
			// Apend timestamp
			message_buf[message_end + 2] = '^';
			strncpy(message_buf + message_end + 3, timestamp, MAX_TIME_LENGTH);
			
			// Send message
			int iSendResult = roomServer->send_message(message_buf, id);
			if (iSendResult != 0) {
				printf("[ERROR]Sending message to client-%d failed.\n", id);
				continue;
			}	
			else {
				printf("[Success]Send message to client-%d successfully.\n", id);
				continue;
			}
		}

		// Start with 's', close connection
		else if (buf[i + 1] == 's') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (roomServer == NULL) {
				printf("[ERROR]RoomServer is not active.\n");
				continue;
			}

			// Initial buf
			char id_buf[MAX_ID_LENGTH];
			memset(id_buf, 0, MAX_ID_LENGTH);

			// Split id
			int id_start = i;
			while (buf[id_start] != '\0') {
				if (buf[id_start] == ' ' || buf[id_start] == '\t' || buf[id_start] == '\n')
					break;
				id_start++;
			}
			while (buf[id_start] != '\0') {
				if (buf[id_start] == ' ' || buf[id_start] == '\t' || buf[id_start] == '\n')
					id_start++;
				else
					break;
			}
			int id_end = id_start;
			while (buf[id_end] != '\0') {
				if (buf[id_end] == ' ' || buf[id_end] == '\t' || buf[id_end] == '\n')
					break;
				id_end++;
			}
			if (id_end - id_start <= 0) {
				printf("[ERROR]Please input right id.\n");
				continue;
			}
			strncpy(id_buf, buf + id_start, id_end - id_start);
			int id = atoi(id_buf);
			if (id < 1 || id > MAX_CLIENT_NUM) {
				printf("[ERROR]Please input correct id: %d.", id);
				continue;
			}
			if (!roomServer->isConnected[id]) {
				printf("[ERROR]Connection is already disabled.\n");
				continue;
			}

			char notice_disconnect[2] = { '&', '\0' };
			iResult = roomServer->broadcast(notice_disconnect, id);
			if (iResult != 0) {
				printf("[ERROR]Disconnection notice can not be broadcasted to other clients.\n");
			}

			// Wait for reply
			std::this_thread::sleep_for(std::chrono::seconds(5));
			// Wait for asynchronous
			if (disconnect) {
				iResult = roomServer->close_client(id);
				if (iResult != 0) {
					disconnect = false;
					printf("[ERROR]Disconnection falied, restart bash.\n");
					return -1;
				}
				else {
					disconnect = false;
					printf("[SUCCESS]Disconnect client-%d successfully.\n", id);
				}
			}
			else {
				disconnect = false;
				printf("[ERROR]Disconnection falied, restart bash.\n");
				return -1;
			}
		}

		// Enforce quit
		else if (buf[i + 1] == 'q') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			printf("[INFO]Would like to quit and close all connections?(y/n)\n");
			char a = 0;
			iResult = scanf("%c", &a);
			if (iResult == EOF) {
				printf("[INFO]Unexpected input error.\n");
				continue;
			}
			if (a == 'y' || a == 'Y') {
				return 0;
			}
			else {
				continue;
			}
		}

		// Enforce restart
		else if (buf[i + 1] == 'r') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			printf("[INFO]Would like to restart and close all connections?(y/n)\n");
			char a = 0;
			iResult = scanf("%c", &a);
			if (iResult == EOF) {
				printf("[INFO]Unexpected input error.\n");
				continue;
			}
			if (a == 'y' || a == 'Y') {
				return 1;
			}
			else {
				continue;
			}
		}

		// Change name
		else if (buf[i + 1] == 'n') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}

			// Split name
			int name_start = i;
			while (buf[name_start] != '\0') {
				if (buf[name_start] == ' ' || buf[name_start] == '\t' || buf[name_start] == '\n')
					break;
				name_start++;
			}
			while (buf[name_start] != '\0') {
				if (buf[name_start] == ' ' || buf[name_start] == '\t' || buf[name_start] == '\n')
					name_start++;
				else
					break;
			}
			int name_end = name_start;
			while (buf[name_end] != '\0') {
				if (buf[name_end] == ' ' || buf[name_end] == '\t' || buf[name_end] == '\n')
					break;
				name_end++;
			}
			if (name_end - name_start >= MAX_NAME_LENGTH) {
				printf("[ERROR]Your name is too long (less than %d).\n", MAX_NAME_LENGTH);
				continue;
			}
			if (name_end - name_start == 0) {
				printf("[ERROR]Please input your new name.\n");
				continue;
			}
			memset(name, 0, MAX_NAME_LENGTH);
			if (name_end - name_start <= 0) {
				printf("[ERROR]Please input right name.\n");
				continue;
			}
			strncpy(name, buf + name_start, name_end - name_start);
			printf("[SUCCESS]Your name has been changed successfully.\n");

			// Change name in objects and send
			if (roomServer != NULL) {
				roomServer->change_name(name);
				char name_plus[MAX_NAME_LENGTH + 1] = { '@' };
				strncpy(name_plus + 1, buf + name_start, name_end - name_start);
				iResult = roomServer->broadcast(name_plus);
				if (iResult != 0) {
					printf("[ERROR]Your name can not be sent to clients.\n");
					continue;
				}
			}
			continue;
		}

		// Change port
		else if (buf[i + 1] == 'p') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}

			// Split port
			int port_start = i;
			while (buf[port_start] != '\0') {
				if (buf[port_start] == ' ' || buf[port_start] == '\t' || buf[port_start] == '\n')
					break;
				port_start++;
			}
			while (buf[port_start] != '\0') {
				if (buf[port_start] == ' ' || buf[port_start] == '\t' || buf[port_start] == '\n')
					port_start++;
				else
					break;
			}
			int port_end = port_start;
			while (buf[port_end] != '\0') {
				if (buf[port_end] == ' ' || buf[port_end] == '\t' || buf[port_end] == '\n')
					break;
				port_end++;
			}
			if (port_end - port_start <= 0) {
				printf("[ERROR]Please input right port.\n");
				continue;
			}
			strncpy(port, buf + port_start, port_end - port_start);

			// Restart roomServer
			if (roomServer != NULL) {
				printf("[INFO]Your RoomServer will be reset.\n");
				iResult = roomServer->close_server();
				if (iResult != 0) {
					printf("[ERROR]Closing RoomServer falied.\n");
				}
				else {
					printf("[SUCCESS]Close RoomServer successfully.\n");
				}
				delete roomServer;
				roomServer = NULL;
				iResult = start_roomserver(roomServer, name, port);
				if (iResult != 0) {
					printf("[ERROR]Start error!\n");
					return 1;
				}
				printf("[SUCCESS]Restart RoomServer successfully.\n");
				continue;
			}
			printf("[SUCCESS]Your port has been changed to %s.\n", port);
		}

		// Start with 'l', list status
		else if (buf[i + 1] == 'l') {
			printf("[INFO]Status are listed below.\n\n");
			printf("\tName: %s\n", name);
			printf("\tPort: %s\n", port);
			if (roomServer != NULL) {
				printf("\tRoomServer: Active\n");
				printf("\tClients active are shown below:\n");
				for (int j = 1; j <= MAX_CLIENT_NUM; j++) {
					if (roomServer->isConnected[j]) {
						printf("\t\tClient-%d Name: %s\n", j, roomServer->client_name[j]);
					}
				}
			}
			else {
				printf("\tRoomServer: Disabled\n");
			}
		}


		// Start with 'x', close roomServer
		else if (buf[i + 1] == 'x') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (roomServer == NULL) {
				printf("[ERROR]RoomServer is already disable.\n");
				continue;
			}

			char close_command[2] = { '$', '\0' };
			iResult = roomServer->broadcast(close_command);
			if (iResult != 0) {
				printf("[ERROR]Disconnect command can not be sent to all clients.\n");
			}
			else {
				printf("[SUCCESS]Disconnect command has been sent to all clients.\n");
			}

			// Wait for reply
			std::this_thread::sleep_for(std::chrono::seconds(2));
			
			// Wait for asynchronous 
			if (disconnect) {
				iResult = roomServer->close_server();
				roomServer->isDeleted = true;
				disconnect = false;
				if (iResult != 0) {
					printf("[ERROR]Disconnection falied, restart bash.\n");
					return -1;
				}
				else {
					printf("[SUCCESS]Disconnect successfully.\n");
					continue;
				}
			}
			else {
				roomServer->isDeleted = true;
				disconnect = false;
				printf("[ERROR]Disconnection falied, restart bash.\n");
				return -1;
			}
		}

		else {
			printf("[ERROR]Please input correct command, start with '-'.\n");
			continue;
		}
	}
}


int main(int argc, char* argv[]) {

	// Help information
	printf("Welcome to Chatool RoomServer!\n\n");
	printf("Each command should be used individualy.\n");
	printf("Usage: \n\n");
	printf("Setup your server: \n");
	printf("\t-i\n\n");
	printf("Send to client with id: \n");
	printf("\t-m <your-message> <id>\n\n");
	printf("Close any client's connection with id: \n");
	printf("\t-s <id>\n\n");
	printf("Close current RoomServer: \n");
	printf("\t-x\n\n");
	printf("Close bash: \n");
	printf("\t-q\n\n");
	printf("Set your name: \n");
	printf("\t-n <your-username>\n\n");
	printf("Set your port: \n");
	printf("\t-p <server-port-yourself>\n\n");
	printf("List current status: \n");
	printf("\t-l\n\n");
	printf("Restart bash: \n");
	printf("\t-r\n\n");

	// Loop
	while (true) {
		int i = bash();

		// Some expection
		if (i == -1) {
			char a = 0;
			printf("[INFO]Would you like to restart?(y/n)\n");
			int iResult = scanf("%c", &a);
			if (iResult == EOF) {
				printf("[INFO]Unexpected input error.\n");
				continue;
			}
			if (a == 'y' || a == 'Y') {
				continue;
			}
			else {
				break;
			}
		}

		// Continue
		else if (i == 1) {
			continue;
		}

		// Exit
		else {
			printf("[INFO]Bye bye\n");
			break;
		}
	}
	return 0;
}