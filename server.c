#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include "constants.h"
#include <errno.h>

// Нужно заменить closeAll на tryCloseAndExit, который срабатывает только один раз.
// 
// 
// 
// 
// 
// 


struct packetData{
	int socket;
	struct sockaddr_in addr;
	char msg[20];
	size_t msgLen;
	unsigned int time;
};

static pthread_t allThreads[20];
static size_t threadsCount;
static int allSockets[20];
static size_t socketsCount;

static int tcpSocket;
static char exitFlag = 0;

void closeAll() {
	for (size_t i = 0; i < threadsCount; ++i) {
		pthread_close(allThreads[i]);
	}
	for (size_t i = 0; i < socketsCount; ++i) {
		shutdown(allSockets[i], SHUT_RDWR);
		close(allSockets[i]);
	}
}

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
	while(0 == exitFlag) {
		sendto(a->socket, a->msg, a->msgLen, 0, (const struct sockaddr *)&(a->addr), sizeof(a->addr) );
		sleep(a->time);
	}
}

//Select для одного потока
int waitReadThread(int socket, time_t seconds) {
	struct timeval tv;
	fd_set rfds;
	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(socket, &rfds);
	return select(socket + 1, &rfds, NULL, NULL, &tv);
}

//прием сообщений из очереди
int tcpReceiveMsg(struct packetData *connection) {
	//Если нет входящих сообщений, возможно это выход
	while (0 == waitReadThread(connection->socket, 2)) {
		if(exitFlag) {
			return -1;
		}
	}
	if( -1 == (connection->msgLen = recv(connection->socket, connection->msg, msgMaxSize, 0))){
		write(2, "Error: Can't receive msg\n", 25);
		return -1;
	}
	return 0;
}

//отправка сообщений из очереди
int tcpSendMsg(struct packetData *connection) {
	if( -1 == send(connection->socket, connection->msg, connection->msgLen, 0)){
		write(2, "Error: Can't send msg\n", 22);
		return -1;
	}
	return 0;
}

void tcpServer() {
	while(0 == exitFlag) {
		//Задаём данные о подключении
		struct packetData connection;
		char connectionType[4];

		memset(&connection.addr, 0, sizeof(connection.addr));

		//Если нет входящих подключений, возможно это выход
		while (0 == waitReadThread(tcpSocket, 2)) {
			if(exitFlag) {
				return ;
			}
		}

		if( -1 == (connection.socket = accept(tcpSocket, 0, 0))) {
			write(2, "Error: Can't create tcp connection\n", 35);
			exitFlag = 1;
			return ;
		}
		
		//Отправляем готовность получать сообщения
		if( -1 == send(connection.socket, "Ready", 6, 0)){
			write(2, "Error: Can't send msg\n", 22);
			exitFlag = 1;
			return ;
		}
		//Если нет входящих сообщений, возможно это выход
		while (0 == waitReadThread(connection.socket, 2)) {
			if(exitFlag) {
				return ;
			}
		}
		//Определяем намериния клиента
		if( -1 == recv(connection.socket, connectionType, 4, 0)){
			write(2, "Error: Can't receive client type\n", 33);
			exitFlag = 1;
			return ;
		}
		
		//Отправка/прием сообщений
		if(0 == strcmp(connectionType, "set")) {
			if( -1 == tcpReceiveMsg(&connection)) {
				exitFlag = 1;
				return ;
			}
			write(1, "Received ", 9);
		}
		else if(0 == strcmp(connectionType, "get")) {
			if( -1 == tcpSendMsg(&connection)) {
				exitFlag = 1;
				return ;
			}
			write(1, "Send ", 5);
		}
		write(1, connection.msg, connection.msgLen - 1);
		write(1, "\n", 1);
		shutdown(connection.socket, SHUT_RDWR);
		close(connection.socket);
	}
}

int main() {
	pthread_t askThread;
	pthread_t sendThread;
	pthread_t tcpThread;
	struct packetData sender1;
	struct packetData sender2;
	struct sockaddr_in tcpAddr;

	tcpAddr = createAddr(INADDR_LOOPBACK, 3002);

	if( -1 == (tcpSocket = socket(PF_INET, SOCK_STREAM, 0)) ) {
		write(2, "Error: Can't create socket\n", 27);
		exit(-1);
	}

	//Данные для отправления broadcast сообщений
	if( -1 == createBroadcastSock(&sender1.socket) ) {
		closeAll();
		exit(-1);
	}
	sender1.addr = createAddr(INADDR_BROADCAST, 3000);
	strcpy(sender1.msg, "Waiting for messages");
	sender1.msgLen = 21;
	sender1.time = K;

	if( -1 == createBroadcastSock(&sender2.socket) ) {
		closeAll();
		exit(-1);
	}
	sender2.addr = createAddr(INADDR_BROADCAST, 3001);
	strcpy(sender2.msg, "Messages available");
	sender2.msgLen = 19;
	sender2.time = L;

	//Связываем сокет и адресс
	bind(tcpSocket, (const struct sockaddr *)&tcpAddr, sizeof tcpAddr);
	
	//Создаём очередь для tcp сообщений
	if( -1 == listen(tcpSocket, N)) {
		write(2, "Error: Can't listen to socket\n", 30);
		closeAll();
		exit(-1);
	}

	//Начинаем броадкастить 
	if ( 0 != pthread_create(&askThread, 0, (void *(*)(void *))&inviteSender, &sender1)) {
		write(2, "Error: Can't create thread\n", 27);
		closeAll();
		exit(-1);
	};
	if ( 0 != pthread_create(&sendThread, 0, (void *(*)(void *))&inviteSender, &sender2)) {
		write(2, "Error: Can't create thread\n", 27);
		closeAll();
		exit(-1);
	};

	//Обработка tcp соединений
	if ( 0 != pthread_create(&tcpThread, 0, (void *(*)())&tcpServer, 0)) {
		write(2, "Error: Can't create thread\n", 27);
		closeAll();
		exit(-1);
	};
	write(1, "Type \"exit\" to close server\n", 28);
	while(!exitFlag) {
		char keyWord[10];

		if (0 == waitReadThread(0, 2) || exitFlag) {
			continue;
		}
		read(0, keyWord, 10);
		if(0 == strncmp("exit", keyWord, 4))
			break;
	}
	closeAll();
	exit(0);
}