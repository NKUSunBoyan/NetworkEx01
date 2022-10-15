#pragma once
#define _CRT_SECURE_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define DEFAULT_PORT "27015"
#define DEFAULT_SERVER_NAME "Alice"
#define DEFAULT_CLIENT_NAME "Bob"
#define MAX_NAME_LENGTH 100
#define MAX_PORT_LENGTH 6
#define MAX_IP_LENGTH 16
#define MAX_MESSAGE_LENGTH 500
#define MAX_TIME_LENGTH 30

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string.h>
#include <iostream>
#include <thread>

extern bool disconnect;

