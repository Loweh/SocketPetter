#include <stdio.h>
#include <stdlib.h>
#include "socketpetter.h"

int main() {
	//CLIENT TEST CODE
	/*CLIENTSOCK sock;
	int error = 0;

	error = CREATECLIENTSOCK(&sock);
	if (error) {
		REMOVECLIENTSOCK(&sock);
		return error;
	};

	CLIENTSENDPACKET(&sock, 33, "snd nudes");

	while (sock.alive) {
		char b[DATASIZE];
		unsigned int type = 0;

		CLIENTHANDLEPING(&sock);

		error = CLIENTRECVPACKET(&sock, &type, b);
		if (!error) {
			printf("The server says... %s with type %i\n", b, type);
		};
	};

	REMOVECLIENTSOCK(&sock);*/

	// SERVER TEST CODE
	SERVERSOCK sock;
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
		for (int i = 0; i < sock.clients; i++) {
			SERVERHANDLEPING(&sock, i);

			error = SERVERRECVPACKET(&sock, i, &type, buffer);
			if (!error) {
				printf("%i says... %s with type %i\n", i, buffer, type);
			};
		};
	};

	REMOVESERVERSOCK(&sock);

	return 0;
};