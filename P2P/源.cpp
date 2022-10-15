#define _CRT_SECURE_NO_WARNINGS

#include "server.h"
#include "client.h"

#pragma comment(lib, "Ws2_32.lib")

bool disconnect = false;

// Is there a connection alive?
bool isConnected(Server* server, Client* client) {

	if (server == NULL && client == NULL) {
		return false;
	}
	if (server == NULL) {
		return client->isConnected;
	}
	if (client == NULL) {
		return server->isConnected;
	} 
	return client->isConnected || server->isConnected;
}

// Start server and create accept thread
int start_server(Server* &server, char* name, char* port) {

	Server::create_server(server, name, port);
	int iResult = 0;
	iResult = server->init_server();
	if (iResult != 0) {
		printf("[ERROR]Server initialization failed.\n");
		return 1;
	}
	iResult = server->bind_server();
	if (iResult != 0) {
		printf("[ERROR]Binding address failed.\n");
		return 1;
	}
	std::thread accept_thread(Server::accept_server, server);
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
	Server* server = NULL;
	Client* client = NULL;
	char buf[MAX_MESSAGE_LENGTH + 10];
	char name[MAX_NAME_LENGTH] = DEFAULT_SERVER_NAME;
	char port[MAX_PORT_LENGTH] = DEFAULT_PORT;
	char target_server_port[MAX_PORT_LENGTH] = DEFAULT_PORT;
	char ip[MAX_IP_LENGTH];

	memset(buf, 0, MAX_MESSAGE_LENGTH + 10);
	memset(ip, 0, MAX_IP_LENGTH);

	// Loop
	while (true) {

		printf("--------------------------------------\n");
		memset(buf, 0, MAX_MESSAGE_LENGTH + 10);
		std::cin.getline(buf, MAX_MESSAGE_LENGTH);
		int i = 0;
		// Clean synchronously
		if (server != NULL && server->isDeleted) {
			delete server;
			server = NULL;
		}
		if (client != NULL && client->isDeleted) {
			delete client;
			client = NULL;
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

		// Start with 'c', create connection as a client
		if (buf[i + 1] == 'c') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}			
			if (server != NULL && server->isConnected) {
				printf("[ERROR]Connection as a server is active, please stop it first.\n");
				continue;
			}
			if (client != NULL && client->isConnected) {
				printf("[ERROR]Connection as a client is active, please stop it first.\n");
				continue;
			}

			// Split ip
			int ip_start = i;
			while (buf[ip_start] != '\0') {
				if (buf[ip_start] == ' ' || buf[ip_start] == '\t' || buf[ip_start] == '\n')
					break;
				ip_start++;
			}
			while (buf[ip_start] != '\0') {
				if (buf[ip_start] == ' ' || buf[ip_start] == '\t' || buf[ip_start] == '\n')
					ip_start++;
				else
					break;
			}
			int ip_end = ip_start;
			while (buf[ip_end] != '\0') {
				if (buf[ip_end] == ' ' || buf[ip_end] == '\t' || buf[ip_end] == '\n')	
					break;
				ip_end++;
			}
			if (ip_end - ip_start <= 0) {
				printf("[ERROR]Please input right ip.\n");
				continue;
			}
			strncpy(ip, buf + ip_start, ip_end - ip_start);

			// Split port
			int port_start = ip_end;
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
			strncpy(target_server_port, buf + port_start, port_end - port_start);
			
			// Create client and connect to server
			int iResult = Client::create_client(client, name, target_server_port, ip);
			if (iResult == 0) {
				printf("[SUCCESS]Client created successfully!\n");
			} else {
				printf("[ERROR]Please input correct ip and port!\n");
				memset(target_server_port, 0, sizeof(target_server_port));
				memset(ip, 0, sizeof(ip));
				continue;
			}

			iResult = client->start_connect();
			if (iResult == 0) {
				printf("[SUCCESS]Setup connection successfully!\n");
			}
			else {
				printf("[ERROR]Connection to server failed!\n");
				memset(target_server_port, 0, sizeof(target_server_port));
				memset(ip, 0, sizeof(ip));
				client->isDeleted = true;
				continue;
			}

			// Start listen mesage thread
			std::thread listen_thread(Client::listen_message, client);
			listen_thread.detach();
			continue;
		} 

		// Start with 'i', setup server
		else if (buf[i + 1] == 'i') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (server != NULL) {
				printf("[ERROR]Server is already active.\n");
				continue;
			}
			iResult = start_server(server, name, port);
			if (iResult != 0) {
				printf("[ERROR]Start error!\n");
				return 1;
			}
			printf("[SUCCESS]Server is activated successfully.\n");
		} 
		
		// Start with 'm', send message to the peer
		else if (buf[i + 1] == 'm') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (!isConnected(server, client)) {
				printf("[ERROR]Connection is disabled, please create it first.\n");
				continue;
			}

			// Initial buf
			char message_buf[MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2];
			memset(message_buf, 0, MAX_MESSAGE_LENGTH + MAX_TIME_LENGTH + 2);
			message_buf[0] = '#';
			char timestamp[MAX_TIME_LENGTH];
			memset(timestamp, 0, MAX_TIME_LENGTH);

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

			// Get timestamp
			SYSTEMTIME time;
			GetLocalTime(&time);
			sprintf(timestamp, "%hd/%hd/%hd %hd:%hd:%hd", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
			
			// Append timestamp
			message_buf[message_end + 2] = '^';
			strncpy(message_buf + message_end + 3, timestamp, MAX_TIME_LENGTH);
			
			// Send message
			int iSendResult = 0; 
			if (server != NULL && server->isConnected) {
				iSendResult = server->send_message(message_buf);
			} else if (client != NULL && client->isConnected) {
				iSendResult = client->send_message(message_buf);
			}
			if (iSendResult != 0) {
				printf("[ERROR]Sending message failed.\n");
				continue;
			} else {
				printf("[Success]Send message successfully.\n");
				continue;
			}	
		} 
		
		// Start with 's', close connection
		else if (buf[i + 1] == 's') {
			if (buf[i + 2] != ' ' && buf[i + 2] != '\t' && buf[i + 2] != '\n' && buf[i + 2] != '\0') {
				printf("[ERROR]Please input correct command, start with '-'.\n");
				continue;
			}
			if (!isConnected(server, client)) {
				printf("[ERROR]Connection is already disabled.\n");
				continue;
			}

			// As a client
			if (client != NULL && client->isConnected) {
				char close_command[2] = { '$', '\0'};
				iResult = client->send_message(close_command);
				if (iResult != 0) {
					printf("[ERROR]Disconnect command can not be sent to server.\n");
					continue;
				} else {
					printf("[SUCCESS]Disconnect command has been sent to server.\n");
				}

				// Wait for reply
				std::this_thread::sleep_for(std::chrono::seconds(2));
				// Wait for asynchronous 
				if (disconnect) {
					iResult = client->close_client();
					if (iResult != 0) {
						server->close_server();
						server->isDeleted = true;
						client->isDeleted = true;
						printf("[ERROR]Disconnection falied, restart bash.\n");
						return -1;
					} 
					else {
						memset(ip, 0, MAX_IP_LENGTH);
						memset(target_server_port, 0, MAX_PORT_LENGTH);
						client->isDeleted = true;
						disconnect = false;
						printf("[SUCCESS]Disconnect successfully.\n");
						continue;
					}
				} else {
					server->close_server();
					server->isDeleted = true;
					client->isDeleted = true;
					printf("[ERROR]Disconnection falied, restart bash.\n");
					return -1;
				}
			} 
			
			// As a server
			else if (server != NULL && server->isConnected) {
				char close_command[2] = { '$', '\0' };
				iResult = server->send_message(close_command);
				if (iResult != 0) {
					printf("[ERROR]Disconnect command can not be sent to client.\n");
				} else {
					printf("[SUCCESS]Disconnect command has been sent to client.\n");
				}

				// Wait for reply
				std::this_thread::sleep_for(std::chrono::seconds(2));
				// Wait for asynchronous
				if (disconnect) {
					iResult = server->close_server();
					if (iResult != 0) {
						client->close_client();
						server->isDeleted = true;
						client->isDeleted = true;
						disconnect = false;
						printf("[ERROR]Disconnection falied, restart bash.\n");
						return -1;
					}
					else {
						server->isDeleted = true;
						disconnect = false;
						printf("[SUCCESS]Disconnect successfully.\n");
						continue;
					}
				}
				else {
					client->close_client();
					server->isDeleted = true;
					client->isDeleted = true;
					disconnect = false;
					printf("[ERROR]Disconnection falied, restart bash.\n");
					return -1;
				}
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
			if (client != NULL) {
				client->change_name(name);
				if (client->isConnected) {
					char name_plus[MAX_NAME_LENGTH + 1] = { '@' };
					strncpy(name_plus + 1, buf + name_start, name_end - name_start);
					iResult = client->send_message(name_plus);
					if (iResult != 0) {
						printf("[ERROR]Your name can not be sent to server.\n");
						continue;
					}
				}
			} 
			else if (server != NULL) {
				server->change_name(name);
				if (server->isConnected) {
					char name_plus[MAX_NAME_LENGTH + 1] = { '@' };
					strncpy(name_plus + 1, buf + name_start, name_end - name_start);
					iResult = server->send_message(name_plus);
					if (iResult != 0) {
						printf("[ERROR]Your name can not be sent to client.\n");
						continue;
					}
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

			// Restart server
			if (server != NULL) {
				printf("[INFO]Your server will be reset.\n");
				iResult = server->close_server();
				if (iResult != 0) {
					printf("[ERROR]Closing server falied.\n");
				}
				else {
					printf("[SUCCESS]Close server successfully.\n");
				}
				delete server;
				server = NULL;
				iResult = start_server(server, name, port);
				if (iResult != 0) {
					printf("[ERROR]Start error!\n");
					return 1;
				}
				printf("[SUCCESS]Restart server successfully.\n");
				continue;
			}
			printf("[SUCCESS]Your port has been changed to %s.\n", port);
		} 
		
		// Start with 'l', list status
		else if (buf[i + 1] == 'l') {
			printf("[INFO]Status are listed below.\n\n");
			printf("\tName: %s\n", name);
			printf("\tPort: %s\n", port);
			if (server != NULL) {
				printf("\tServer: Active\n");	
				if (server->isConnected) {
					printf("\tServer Connected: Yes\n");
					printf("\tClient Name: %s\n", server->client_name);
				}
			} else {
				printf("\tServer: Disabled\n");
			}
			if (client != NULL && client->isConnected) {
				printf("\tClient: Active\n");
				printf("\tTarget Server Name: %s\n", client->server_name);
				printf("\tTarget Server Port: %s\n", client->port);
				printf("\tTarget Server IP: %s\n", client->ip);
			}	
		} else {
			printf("[ERROR]Please input correct command, start with '-'.\n");
			continue;
		}
	}
}


int main(int argc, char* argv[]) {

	// Help information
	printf("Welcome to Chatool!\n\n");
	printf("Each command should be used individualy.\n");
	printf("Usage: \n\n");
	printf("Connect to someone: \n");
	printf("\t-c <ipv4-to-connect> <server-port>\n\n");
	printf("Setup your server: \n");
	printf("\t-i\n\n");
	printf("Send to tartget: \n");
	printf("\t-m <your-message>\n\n");
	printf("Close connection: \n");
	printf("\t-s\n\n");
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
			} else {
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