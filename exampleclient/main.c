#include <stdio.h>
#include <stdlib.h>
#include "../socketpetter.h"

int main() {
	//CLIENT TEST CODE
	CLIENTSOCK sock;
	int error = 0;

	error = CREATECLIENTSOCK(&sock);
	if (error) {
		REMOVECLIENTSOCK(&sock);
		return error;
	};

	CLIENTSENDPACKET(&sock, 33, "test");

	while (sock.alive) {
		char b[DATASIZE];
		unsigned int type = 0;

		CLIENTHANDLEPING(&sock);

		error = CLIENTRECVPACKET(&sock, &type, b);
		if (!error) {
			if (type != PINGTYPEVALUE) {
				printf("The server says... %s with type %i\n", b, type);
			} else {
				printf("The server pinged the client\n");
			};
		};
	};

	REMOVECLIENTSOCK(&sock);

	return 0;
};