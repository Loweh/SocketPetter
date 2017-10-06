#include <stdio.h>
#include <stdlib.h>
#include "socketpetter.h"

int main() {
	//CLIENT TEST CODE
	CLIENTSOCK sock;
	int error = 0;

	error = CREATECLIENTSOCK(&sock);
	if (error) {
		REMOVECLIENTSOCK(&sock);
		return error;
	};

	CLIENTSENDPACKET(&sock, 100, "snd nudes");

	while (sock.alive) {
		time_t currenttime;
		time(&currenttime);

		error = (int) difftime(currenttime, sock.lastping);
		if (error > PINGTIMEOUT && sock.alive) {
			sock.alive = 0;
		} else {
			char b[DATASIZE];
			unsigned int type = 0;

			error = CLIENTRECVPACKET(&sock, &type, b);

			if (!error) {
				printf("The server says... %s with type %i\n", b, type);
			};
		};
	};

	REMOVECLIENTSOCK(&sock);

	// SERVER TEST CODE
	/*SERVERSOCK sock;
	int error = 0;
	int a = 0;

	error = CREATESERVERSOCK(&sock);
	if (error) {
		REMOVESERVERSOCK(&sock);
		return error;
	};

	while (sock.alive) {
		ACCEPTCLIENTSOCK(&sock);

		char buffer[DATASIZE] = "";
		unsigned int type = 0;
		time_t currenttime;
		time(&currenttime);

		for (int i = 0; i < sock.clients; i++) {
			error = (int)difftime(currenttime, sock.clientlist[i].lastping);
			if (error == 30 && !sock.clientlist[i].dead) {
				sock.clientlist[i].dead = 1;
				closesocket(sock.clientlist[i].sock);

				printf("%i is dead\n", i);
			} else {
				error = SERVERRECVPACKET(&sock, i, &type, buffer);
				if (!error) {
					printf("%i says... %s with type %i\n", i, buffer, type);
				};
			};
		};
	};

	REMOVESERVERSOCK(&sock);*/

	return 0;
};