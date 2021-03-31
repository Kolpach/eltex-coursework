#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "constants.h"

struct sockaddr_in createAddr(in_addr_t address, in_port_t port) {
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_port = port;

	return addr;
}

int createBroadcastSock(int *fd) {
	if ((*fd = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
		write(2, "Error: Can't create socket\n", 27);
		return -1;
	}
	if ( setsockopt(*fd, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) == -1) {
		write(2, "Error: setsockopt SO_BROADCAST\n", 31);
		return -1;
	}
	if ( setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
		write(2, "Error: setsockopt SO_REUSEADDR\n", 31);
		return -1;
	}
	return 0;	
}

int main() {

	int listeningSocket;
	int tcpSocket;
	struct sockaddr_in clientAddress;
	struct sockaddr_in serverAddress;
	socklen_t serverAdressLen;

	clientAddress = createAddr(INADDR_ANY, 3000);
	serverAddress = createAddr(INADDR_LOOPBACK, 3002);
	
	if( -1 == createBroadcastSock(&listeningSocket)) {
		exit(-1);
	}
	bind(listeningSocket, (const struct sockaddr *) &clientAddress, sizeof(clientAddress));

	srand(213);

	for(int i = 0; i < 5; ++i) {
		char msg[25];
		//Получаем сигнал Waiting for connections
		int len = recv(listeningSocket, &msg, 25, 0);
		if(-1 == len) {
			write(2, "Error: Can't receve msg\n", 24);
			exit(-1);
		}
		write(1, &msg, len-1);
		write(1, "\n", 1);

		//Создаём сокет для соединения
		if (-1 == (tcpSocket = socket(PF_INET, SOCK_STREAM, 0))) {
			write(2, "Error: Can't create socket\n", 27);
			exit(-1);
		}
		//Попытка установления соединения
		if(-1 == connect(tcpSocket, (const struct sockaddr *)&serverAddress, sizeof(serverAddress))) {
			write(2, "Error: Can't connect to server\n", 31);
			exit(-1);
		}

		if( -1 == ( len = recv(tcpSocket, msg, 25, 0))) {
			write(2, "Error: Can't receive msg\n", 25);
			exit(-1);
		}
		//Отправка своего типа
		if( -1 == send(tcpSocket, "set", 4, 0)) {
			write(2, "Error: Can't send to server\n", 28);
			exit(-1);
		}

		strcpy(msg, "123");
		//Отправка сообщения
		if( -1 == send(tcpSocket, msg, 4, 0)) {
			write(2, "Error: Can't send to server\n", 28);
			exit(-1);
		}
		write(1, msg, 3);
		write(1, "\n", 1);
		//Закрытие соединения
		close(tcpSocket);
		sleep(N);
	}
	close(listeningSocket);
	return 0;
}