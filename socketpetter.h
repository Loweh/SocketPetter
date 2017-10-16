#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* HEY REMEMBER THESE THINGS
-DOCUMENT HOW ALL THIS STUFF WORKS SO YOU KNOW FOR THE FUTURE
-MAKE PRINTS LOOK BETTER, MORE DESCRIPTIVE
-ADD THAT SHADOW ARRAY ALREADY
-MAKE VARIABLE, CONSTANT AND FUNCTION NAMES LESS UGLY
-FIGURE OUT IF SESSIONS NEED TO BE ON PINGS
-FIGURE OUT HOW LONG PINGTIMEOUT SHOULD BE
*/

//CONSTANTS

#define PORT 2127
#define IP "127.0.0.1"
#define PACKETSIZE 13
#define SESSIONSIZE 2
#define DATASIZE (PACKETSIZE - (SESSIONSIZE + 1))
#define NONBLOCKING 1
#define OPERATIONTIMEOUT 50
#define PINGTYPEVALUE 100
#define PINGTIMEOUT 30

//MAIN STRUCTS

typedef struct CLIENTSOCK CLIENTSOCK;

struct CLIENTSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	time_t lastpingrecv;
	time_t lastpingsent;
	int session;
	int alive;
	int lastvalue;
};

typedef struct SERVERCLIENT SERVERCLIENT;

struct SERVERCLIENT {
	SOCKET socket;
	time_t lastpingrecv;
	time_t lastpingsent;
	int alive;
};

typedef struct SERVERSOCK SERVERSOCK;

struct SERVERSOCK {
	WSADATA wsa;
	SOCKET socket;
	struct sockaddr_in server;
	SERVERCLIENT serverclients[10]; //will probably change this later to a shadow array or something
	int sessions[10]; //this too
	int clients;
	int alive;
	int lastvalue;
};

//SESSION RELATED CODE

int NEW_SESSION(SERVERSOCK *ssock) {
	int result = rand() % 16000;

	int keepgoing = 1;
	while (keepgoing) {
		int original = 1;
		for (int i = 0; i < ssock->clients; i++) {
			if (result == ssock->sessions[i]) {
				original = 0;
			};
		};

		if (original) {
			keepgoing = 0;
		};
	};

	return result;
};

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
		int part = (int)input[(SESSIONSIZE - (i + 1))];
		part = part << (8 * (SESSIONSIZE - (i + 1)));
		*output = *output | part;
	};
};

//CLIENT END FUNCTIONS

int CREATECLIENTSOCK(CLIENTSOCK *csock) {
	//no garbage data allowed!!!!!!!
	csock->session = 0;
	csock->alive = 0;
	csock->lastvalue = 0;

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
		return 4;
	};

	SESSION_DECODE(buffer, &csock->session);

	printf("Received id: %i\n", csock->session);

	int nonblocking = NONBLOCKING;
	csock->lastvalue = ioctlsocket(csock->socket, FIONBIO, (u_long *) &nonblocking);
	if (csock->lastvalue == SOCKET_ERROR) {
		return 5;
	};

	int timeout = OPERATIONTIMEOUT;
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

//SERVER END FUNCTIONS

int CREATESERVERSOCK(SERVERSOCK *ssock) {
	//no garbage data allowed!!!!!!!
	ssock->clients = 0;
	ssock->alive = 0;
	ssock->lastvalue = 0;

	ssock->lastvalue = WSAStartup(MAKEWORD(2, 2), &ssock->wsa);
	if (ssock->lastvalue != 0) {
		return 1;
	};

	ssock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ssock->socket == INVALID_SOCKET) {
		return 2;
	};

	int nonblocking = NONBLOCKING;
	ssock->lastvalue = ioctlsocket(ssock->socket, FIONBIO, (u_long *) &nonblocking);
	if (ssock->lastvalue == SOCKET_ERROR) {
		return 3;
	};

	int timeout = OPERATIONTIMEOUT;
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
		client.alive = 1;

		ssock->serverclients[ssock->clients] = client;

		int session = NEW_SESSION(ssock);
		ssock->sessions[ssock->clients] = session;

		char buffer[SESSIONSIZE];
		SESSION_ENCODE(session, buffer);

		send(ssock->serverclients[ssock->clients].socket, buffer, SESSIONSIZE, 0);

		printf("Gave session %i to new client\n", session);

		ssock->clients++;
	};

	Sleep(OPERATIONTIMEOUT);
};

int SERVERSENDPACKET(SERVERSOCK *ssock, int clientnum, unsigned int type, char *data) {
	char buffer[PACKETSIZE];
	buffer[0] = (char) type;

	char session[SESSIONSIZE];
	SESSION_ENCODE(ssock->sessions[clientnum], session);
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

	if (session != ssock->sessions[clientnum]) {
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
		printf("%i is dead\n", ssock->sessions[clientnum]);

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
