#include "def.h"

class Server {
    
private:
    struct addrinfo* result = NULL;
    struct addrinfo hints;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    
public:
    char port[MAX_PORT_LENGTH];
    char name[MAX_NAME_LENGTH];
    char client_name[MAX_NAME_LENGTH];
    bool isConnected = false;
    bool isDeleted = false;

private:
    Server(char* name, char* port);

public:
    static int create_server(Server* &server, char* name, char* port);
    int init_server();
    int bind_server();
    static void accept_server(Server* server);
    static void listen_message(Server* server);
    int send_message(char* message);
    int close_server();
    int change_name(char* name);
};