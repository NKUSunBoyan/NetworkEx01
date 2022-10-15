#pragma once
#include "def.h"

class Client {

private:
	struct addrinfo* result = NULL;
	struct addrinfo* ptr = NULL;
	struct addrinfo hints;
	SOCKET ConnectSocket = INVALID_SOCKET;
	
public:
	char ip[MAX_IP_LENGTH];
	char port[MAX_PORT_LENGTH];
	char name[MAX_NAME_LENGTH];
	char server_name[MAX_NAME_LENGTH];
	bool isConnected = false;
	bool isDeleted = false;

private:
	Client(char* name, char* port, char* ip);

public:
	static int create_client(Client* &client, char* name, char* port, char* ip);
	static bool solve_ip(char* ip);
	int start_connect();
	static void listen_message(Client* client);
	int send_message(char* message);
	int close_client();
	int change_name(char* name);
};
