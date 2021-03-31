#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "constants.h"
#include <errno.h>

struct packetData{
	int socket;
	struct sockaddr_in addr;
	char msg[20];
	size_t msgLen;
	unsigned int time;
};

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

void inviteSender(void *arg) {
	struct packetData *a = (struct packetData *)arg;
	while(1) {
		sendto(a->socket, a->msg, a->msgLen, 0, (const struct sockaddr *)&(a->addr), sizeof(a->addr) );
		pthread_testcancel();
		sleep(a->time);
	}
}

int main() {
	struct sockaddr_in tcpAddr;

	
	int tcpSocket;

	struct packetData sender1;
	struct packetData sender2;

	tcpAddr = createAddr(INADDR_LOOPBACK, 3002);

	if( -1 == (tcpSocket = socket(PF_INET, SOCK_STREAM, 0)) ) {
		write(2, "Error: Can't create socket\n", 27);
		exit(-1);
	}

	//Данные для отправления broadcast сообщений
	if( -1 == createBroadcastSock(&sender1.socket) )
		exit(-1);
	sender1.addr = createAddr(INADDR_BROADCAST, 3000);
	strcpy(sender1.msg, "Waiting for messages");
	sender1.msgLen = 21;
	sender1.time = K;

	if( -1 == createBroadcastSock(&sender2.socket) )
		exit(-1);
	sender2.addr = createAddr(INADDR_BROADCAST, 3001);
	strcpy(sender2.msg, "Messages available");
	sender2.msgLen = 19;
	sender2.time = L;

	//Связываем сокет и адресс
	bind(tcpSocket, (const struct sockaddr *)&tcpAddr, sizeof tcpAddr);
	
	//Создаём очередь для tcp сообщений
	if( -1 == listen(tcpSocket, N)) {
		write(2, "Error: Can't listen to socket\n", 30);
		exit(-1);
	}

	//Начинаем броадкастить 
	pthread_t askThread;
	pthread_t sendThread;
	if ( 0 != pthread_create(&askThread, 0, (void *(*)(void *))&inviteSender, &sender1)) {
		write(2, "Error: Can't create thread\n", 27);
		exit(-1);
	};
	if ( 0 != pthread_create(&sendThread, 0, (void *(*)(void *))&inviteSender, &sender2)) {
		write(2, "Error: Can't create thread\n", 27);
		exit(-1);
	};

	for(int i = 0; i < 20; i++) {
		//Задаём данные о подключении
		struct packetData connection;
		char connectionType[4];

		memset(&connection.addr, 0, sizeof(connection.addr));
		if( -1 == (connection.socket = accept(tcpSocket, 0, 0))) {
			write(2, "Error: Can't create tcp connection\n", 35);
			exit(-1);
		}
		
		//Отправляем готовность получать сообщения
		if( -1 == send(connection.socket, "Ready", 6, 0)){
			write(2, "Error: Can't send msg\n", 22);
			int delete = errno;
			exit(-1);
		}
		//Определяем намериния клиента
		if( -1 == recv(connection.socket, connectionType, 4, 0)){
			write(2, "Error: Can't receive client type\n", 33);
			exit(-1);
		}
		
		//Нужно вынести отпавку/получение в отдельные функции
		if(0 == strcmp(connectionType, "set")) {
			if( -1 == (connection.msgLen = recv(connection.socket, connection.msg, msgMaxSize, 0))){
				write(2, "Error: Can't receive msg\n", 25);
				exit(-1);
			}
		}
		else if(0 == strcmp(connectionType, "get")) {
			if( -1 == send(connection.socket, connection.msg, connection.msgLen, 0)){
				write(2, "Error: Can't send msg\n", 22);
				exit(-1);
			}
		}
		write(1, connection.msg, connection.msgLen - 1);
		write(1, "\n", 1);
		close(connection.socket);
		//sleep(K);
	}


	if ( 0 != pthread_cancel(askThread)) {
		write(2, "Error: Can't cancel thread\n", 27);
		exit(-1);
	}
	if ( 0 != pthread_cancel(sendThread)) {
		write(2, "Error: Can't cancel thread\n", 27);
		exit(-1);
	}

	close(tcpSocket);
	close(sender1.socket);
	close(sender2.socket);
	return 0;
}