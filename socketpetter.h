#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* HEY REMEMBER THESE THINGS
-WRITE SOME DOCUMENTATION FOR THIS SO I KNOW WHAT I'M DOING IN LATER PROJECTS (LET'S BE HONEST ONLY YOU WILL USE THIS ANYWAY)
-CLEAN UP SOME GROSS NAMES AND STUFF (AKA ORGANIZE YOUR CODE)
-ADD A NEW ARRAY IN SERVERSOCK TO STORE SESSIONS FROM CLIENTS (SESSION WILL BE IN SAME SPOT AS ITS ASSOCIATED SERVERSOCK, EX: SESSIONS[0] IS SOCKS[0]'s SESSION)
-FIGURE OUT IF SESSIONS NEED TO BE ON PINGS
-CONSIDER FUSING HANDLEPING WITH RECV
*/

//CONSTANTS

const int PORT = 2127;
const char *IP = "127.0.0.1";
const int PACKETSIZE = 13;
const int SESSIONSIZE = 2;
const int DATASIZE = PACKETSIZE - (SESSIONSIZE + 1);
const u_long NONBLOCKING = 1;
const int PINGTYPEVALUE = 100;
const int PINGTIMEOUT = 30;

//HELPFUL FUNCTIONS

void SESSION_ENCODE(int input, char *output) { //convert an int into a SESSIONSIZE-byte long char*
	for (int i = 0; i < SESSIONSIZE; i++) {
		int numpart = input;
		numpart = numpart << (8 * (3 - i));
		numpart = numpart >> 24;
		output[i] = (char)numpart;
	};
};

void SESSION_DECODE(char *input, int *output) { //convert a SESSIONSIZE-byte long char* into int
	for (int i = 0; i < SESSIONSIZE; i++) {
		int part = (int) input[(SESSIONSIZE - (i + 1))];
		part = part << (8 * (SESSIONSIZE - (i + 1)));
		*output = *output | part;
	};
};

//CLIENT

typedef struct CLIENTSOCK CLIENTSOCK;

struct CLIENTSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	time_t lastpingrecv;
	time_t lastpingsent;
	int session = 0;
	int alive = 0;
	int lastvalue = 0;
};

int CREATECLIENTSOCK(CLIENTSOCK *csock) {
	csock->lastvalue = WSAStartup(MAKEWORD(2, 2), &csock->wsa);
	if (csock->lastvalue != 0) {
		return 1;
	};

	csock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (csock->socket == INVALID_SOCKET) {
		return 2;
	};

	csock->server.sin_addr.s_addr = inet_addr(IP);
	csock->server.sin_port = htons(PORT);
	csock->server.sin_family = AF_INET;

	csock->lastvalue = connect(csock->socket, (struct sockaddr *) &csock->server, sizeof(csock->server));
	if (csock->lastvalue == SOCKET_ERROR) {
		return 3;
	};

	char buffer[SESSIONSIZE];
	csock->lastvalue = recv(csock->socket, buffer, SESSIONSIZE, 0);
	if (csock->lastvalue == 0 || csock->lastvalue == SOCKET_ERROR) {
		return 6;
	};

	SESSION_DECODE(buffer, &csock->session);

	printf("Received id: %i\n", csock->session);

	csock->lastvalue = ioctlsocket(csock->socket, FIONBIO, (u_long *)&NONBLOCKING);

	int timeout = 50;
	setsockopt(csock->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(int));
	setsockopt(csock->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(int));

	time_t currenttime;
	time(&currenttime);

	csock->lastpingrecv = currenttime;
	csock->lastpingsent = currenttime;

	csock->alive = 1;

	return 0;
};

int CLIENTSENDPACKET(CLIENTSOCK *csock, unsigned int type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = (char) type;

	char sessionbuffer[SESSIONSIZE];
	SESSION_ENCODE(csock->session, sessionbuffer);
	strncpy(buffer + 1, sessionbuffer, SESSIONSIZE);

	strncpy(buffer + (1 +SESSIONSIZE), data, DATASIZE);

	int result = send(csock->socket, buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	};

	return 0;
};

int CLIENTRECVPACKET(CLIENTSOCK *csock, unsigned int *type, char *data) {
	char buffer[PACKETSIZE];

	int result = recv(csock->socket, buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
		return 1;
	};

	*type = (unsigned int) buffer[0];

	if (*type == PINGTYPEVALUE) {
		time_t currenttime;
		time(&currenttime);

		csock->lastpingrecv = currenttime;
	};

	char sessionraw[SESSIONSIZE];
	strncpy(sessionraw, buffer + 1, SESSIONSIZE);

	int session = 0;
	SESSION_DECODE(sessionraw, &session);

	if (session != csock->session) {
		return 2;
	};

	strncpy(data, buffer + (1 + SESSIONSIZE), DATASIZE);

	return 0;
};

void CLIENTHANDLEPING(CLIENTSOCK *csock) {
	int difference = 0;
	time_t currenttime;

	time(&currenttime);

	difference = difftime(currenttime, csock->lastpingrecv);
	if (difference == PINGTIMEOUT) {
		csock->alive = 0;
	};

	difference = difftime(currenttime, csock->lastpingsent);
	if (difference == (PINGTIMEOUT - 2)) { //magic number to give some time for the server to get the ping
		char buffer[PACKETSIZE];
		buffer[0] = (char) PINGTYPEVALUE;

		send(csock->socket, buffer, PACKETSIZE, 0);
		time(&csock->lastpingsent);
	};
};

void REMOVECLIENTSOCK(CLIENTSOCK *csock) {
	closesocket(csock->socket);
	WSACleanup();
};

//SERVER

typedef struct SERVERCLIENT SERVERCLIENT;

struct SERVERCLIENT {
	SOCKET socket;
	time_t lastpingrecv;
	time_t lastpingsent;
	int alive = 1;
};

typedef struct SERVERSOCK SERVERSOCK;

struct SERVERSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	SERVERCLIENT serverclients[10]; //will probably change this later to a shadow array or something
	int clients = 0;
	int alive = 0;
	int lastvalue = 0;
};

int CREATESERVERSOCK(SERVERSOCK *ssock) {
	ssock->lastvalue = WSAStartup(MAKEWORD(2, 2), &ssock->wsa);
	if (ssock->lastvalue != 0) {
		return 1;
	};

	ssock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ssock->socket == INVALID_SOCKET) {
		return 2;
	};

	ssock->lastvalue = ioctlsocket(ssock->socket, FIONBIO, (u_long *)&NONBLOCKING);

	int timeout = 50;
	setsockopt(ssock->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(int));
	setsockopt(ssock->socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(int));

	ssock->server.sin_addr.s_addr = INADDR_ANY;
	ssock->server.sin_port = htons(PORT);
	ssock->server.sin_family = AF_INET;

	ssock->lastvalue = bind(ssock->socket, (struct sockaddr *) &ssock->server, sizeof(ssock->server));
	if (ssock->lastvalue == SOCKET_ERROR) {
		return 4;
	};

	ssock->alive = 1;

	return 0;
};

void ACCEPTCLIENTSOCK(SERVERSOCK *ssock) {
	listen(ssock->socket, 3);

	int serversize = sizeof(ssock->server);
	SOCKET tempsock = accept(ssock->socket, (struct sockaddr *) &ssock->server, &serversize);

	SERVERCLIENT client;
	client.socket = tempsock;

	time_t currenttime;
	time(&currenttime);
	client.lastpingrecv = currenttime;
	client.lastpingsent = currenttime;


	if (tempsock != INVALID_SOCKET) {
		ssock->serverclients[ssock->clients] = client;

		char buffer[SESSIONSIZE];
		SESSION_ENCODE(ssock->clients, buffer);

		send(ssock->serverclients[ssock->clients].socket, buffer, SESSIONSIZE, 0);

		printf("Gave id %i to new client\n", ssock->clients);

		ssock->clients++;
	};

	Sleep(50);
};

int SERVERSENDPACKET(SERVERSOCK *ssock, int clientnum, unsigned int type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = (char) type;

	char session[SESSIONSIZE];
	SESSION_ENCODE(clientnum, session);
	strncpy(buffer + 1, session, SESSIONSIZE);

	strncpy(buffer + (1 + SESSIONSIZE), data, DATASIZE);

	int result = send(ssock->serverclients[clientnum].socket, buffer, PACKETSIZE, 0);
	if (result == SOCKET_ERROR) {
		return 1;
	}

	return 0;
};

int SERVERRECVPACKET(SERVERSOCK *ssock, int clientnum, unsigned int *type, char *data) {
	char buffer[PACKETSIZE];

	int result = recv(ssock->serverclients[clientnum].socket, buffer, PACKETSIZE, 0);
	if (result == 0 || result == SOCKET_ERROR) {
		return 1;
	};

	*type = (unsigned int) buffer[0];
	if (*type == PINGTYPEVALUE) {
		time_t currenttime;
		time(&currenttime);

		ssock->serverclients[clientnum].lastpingrecv = currenttime;
	};

	char sessionraw[SESSIONSIZE];
	strncpy(sessionraw, buffer + 1, SESSIONSIZE);

	int session = 0;
	SESSION_DECODE(sessionraw, &session);

	if (session != clientnum) {
		return 2;
	};

	strncpy(data, buffer + (1 + SESSIONSIZE), DATASIZE);

	return 0;
};

void SERVERHANDLEPING(SERVERSOCK *ssock, int clientnum) {
	int difference = 0;
	time_t currenttime;

	time(&currenttime);

	difference = difftime(currenttime, ssock->serverclients[clientnum].lastpingrecv);
	if (difference == PINGTIMEOUT && ssock->serverclients[clientnum].alive) {
		printf("%i is dead\n", clientnum);

		ssock->serverclients[clientnum].alive = 0;
		closesocket(ssock->serverclients[clientnum].socket);
	};

	difference = difftime(currenttime, ssock->serverclients[clientnum].lastpingsent);
	if (difference == (PINGTIMEOUT - 2)) { //magic number to give the ping to the client some time to get there
		char buffer[PACKETSIZE];
		buffer[0] = 'd';

		send(ssock->serverclients[clientnum].socket, buffer, PACKETSIZE, 0);
		time(&ssock->serverclients[clientnum].lastpingsent);
	};
};

void REMOVESERVERSOCK(SERVERSOCK *ssock) {
	closesocket(ssock->socket);
	WSACleanup();
};
