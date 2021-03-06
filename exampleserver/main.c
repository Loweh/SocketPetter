#include <stdio.h>
#include <stdlib.h>
#include "../socketpetter.h"

int main() {
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
				SERVERCLIENT *client = GETSHADOWARRAYELEMENT(&sock.serverclients, i);

				if (type != PINGTYPEVALUE) {
					printf("%i says... %s with type %i\n", client->session, buffer, type);
				} else {
					printf("%i pinged the server\n", client->session);
				};
			};
		};
	};

	REMOVESERVERSOCK(&sock);

	return 0;
};