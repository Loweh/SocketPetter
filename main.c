#include <stdio.h>
#include <stdlib.h>
#include "socketpetter.h"

int main() {
	/* CLIENT TEST CODE
	CLIENTSOCK sock;
	int error = 0;

	error = CREATECLIENTSOCK(&sock);
	if (error) {
		REMOVECLIENTSOCK(&sock);
		return error;
	};

	SENDCLIENTPACKET(&sock, '!', "snd nudes");

	while (sock.alive) {
		char b[DATASIZE];
		char type = '\0';

		error = RECVCLIENTPACKET(&sock, &type, b);

		if (!error) {
			printf("%s\n", b);
		};
	};

	REMOVECLIENTSOCK(&sock); */

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

		char b[DATASIZE] = "";
		char c[DATASIZE] = "";
		char d[DATASIZE] = "";
		char type = '\0';
		char type2 = '\0';
		char type3 = '\0';

		error = SERVERRECVPACKET(&sock, 0, &type, b);
		if (!error) {
			printf("0 says... %s\n", b);
		};

		error = SERVERRECVPACKET(&sock, 1, &type2, c);
		if (!error) {
			printf("1 says... %s\n", c);
		};

		error = SERVERRECVPACKET(&sock, 2, &type3, d);
		if (!error) {
			printf("2 says... %s\n", d);
		};
	};

	REMOVESERVERSOCK(&sock);
	return 0;
};