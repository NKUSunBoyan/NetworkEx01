#include "def.h"

class RoomServer {

private:
    struct addrinfo* result = NULL;
    struct addrinfo hints;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket[MAX_CLIENT_NUM + 1];

public:
    char port[MAX_PORT_LENGTH];
    char name[MAX_NAME_LENGTH];
    char client_name[MAX_CLIENT_NUM + 1][MAX_NAME_LENGTH];
    bool isConnected[MAX_CLIENT_NUM + 1];
    bool isDeleted = false;

private:
    RoomServer(char* name, char* port);

public:
    static int create_server(RoomServer*& roomServer, char* name, char* port);
    int init_server();
    int bind_server();
    static void accept_server(RoomServer* roomServer);
    static void listen_message(RoomServer* roomServer, int id);
    int broadcast(char* message);
    int broadcast(char* message, int id);
    int send_message(char* message, int id);
    int close_server();
    int close_client(int id);
    int change_name(char* name);
};
