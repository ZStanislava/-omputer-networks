/*
 * Шаблон TCP клиента.
 *
 * Компиляция:
 *	cc -Wall -O2 -o client client.c
 *
 * Завершение работы клиента: Ctrl+D.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <limits.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define PORT 1027
#define MAXLINE 256

#define SA struct sockaddr 

/*
 * Обработчик фатальных ошибок.
 */
void error(const char *s)
{
	perror(s);
	exit(-1);
}   

/*
 * Функции-обертки.
 */
int Socket(int domain, int type, int protocol)
{
	int rc;
	
	rc = socket(domain, type, protocol);
	if(rc == -1) error("socket()");

	return rc;
}

void Connect(int socket, const struct sockaddr *addr, socklen_t addrlen)
{
	int rc;
	
	rc = connect(socket, addr, addrlen);
	if(rc == -1) error("connect()");
}

void Close(int fd)
{
	int rc;
	
	for(;;) {
		rc = close(fd);
		if(!rc) break;
		if(errno == EINTR) continue;
		error("close()");
	}
}

void Inet_aton(const char *str, struct in_addr *addr)
{
	int rc;
	
	rc = inet_aton(str, addr);
	if(!rc) {
		/* Функция inet_aton() не меняет errno в случае ошибки. Чтобы
		сообщение, выводимое error(), было более осмысленным,
		присваиваем errno значение EINVAL. */
				
		errno = EINVAL;
		error("inet_aton()");
	}
}

int Select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *timeout)
{
	int rc;
	
	for(;;) {
		rc = select(n, readfds, writefds, exceptfds, timeout);
		if(rc != -1) break;
		if(errno == EINTR) continue;
		error("select()");
	}
	
	return rc;
}

size_t Read(int fd, void *buf, size_t count)
{
	ssize_t rc;
	
	for(;;) {
		rc = read(fd, buf, count);
		if(rc != -1) break;
		if(errno == EINTR) continue;
		error("read()");
	}
	
	return rc;
}

size_t Write(int fd, const void *buf, size_t count)
{
	ssize_t rc;
	
	for(;;) {
		rc = write(fd, buf, count);
		if(rc != -1) break;
		if(errno == EINTR) continue;
		error("write()");
	}
	
	return rc;
}

/*
 * Запись count байтов в сокет.
 */
size_t writen(int socket, const char *buf, size_t count)
{
	const char *p;
	size_t n, rc;

	/* Проверить корректность переданных аргументов. */
	if(buf == NULL) {
		errno = EFAULT;
		error("writen()");
	}
	
	p = buf;
	n = count;
	while(n) {
		rc = Write(socket, p, n);
		n -= rc;
		p += rc;
	}

	return count;
}

void show_usage()
{
	puts("Usage: client ip_address");	
	exit(-1);
}

void do_work(int socket)
{
	int n;
	//массив дескрипторов
	fd_set readfds;
	char s[MAXLINE];
	ssize_t rc;

	n = MAX(STDIN_FILENO, socket) + 1;	
	
//	s = "response";
//	rc = 9;
			
	for(;;) {
		/* Инициализировать набор дескрипторов. */
		//очищаем его
		FD_ZERO(&readfds);
		//стандартный ввод
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(socket, &readfds);

		//select () позволяет программе отслеживать несколько файловых дескрипторов,
		//ожидание, пока один или несколько файловых дескрипторов не станут «готовыми»
		//для некоторого класса операции ввода - вывода(например, возможный ввод).Файл
		//дескриптор считается готовым, если возможно выполнить
		//соответствующую операцию ввода / вывода без блокировки.

		//nfds - Этот аргумент должен быть установлен на файл с наибольшим номером
		//readfds - Дескрипторы файлов в этом наборе просматриваются на готовность читать
		//writefds - Дескрипторы файлов в этом наборе просматриваются на готовность писать
		//excludefds - Дескрипторы файлов в этом наборе отслеживаются «исключительные условия».

		Select(n, &readfds, NULL, NULL, NULL);

		//не ноль, если присутствует в наборе
		/*if(FD_ISSET(STDIN_FILENO, &readfds)) {
			rc = Read(STDIN_FILENO, s, MAXLINE);
			if(!rc) break;
			writen(socket, s, rc);
		}*/
		
		if(FD_ISSET(socket, &readfds)) {
			rc = Read(socket, s, MAXLINE);
			if(!rc) break;
			Write(STDOUT_FILENO, s, rc);
		}
	}
}

int main(int argc, char **argv)
{
	int socket;
	struct sockaddr_in servaddr;
	
	if(argc != 2) show_usage();

	socket = Socket(PF_INET, SOCK_STREAM, 0);
	
	/* Инициализировать структуру адреса сокета. */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	//из сетевого адреса делаем внутреннее представление
	Inet_aton(argv[1], &servaddr.sin_addr);
	//подключает сокет, на который ссылается файловый дескриптор sockfd по адресу, указанному в addr
	Connect(socket, (SA *) &servaddr, sizeof(servaddr));
	do_work(socket);		
	Close(socket);
	
	return 0;
}
