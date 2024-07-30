/*
 * Шаблон параллельного эхо-сервера TCP, работающего по модели
 * "один клиент - один поток".
 *
 * Компиляция:
 *      gcc -Wall -O2 -lpthread -o server3 server3.c

	-Wall - сообщения о предупреждениях и ошибках
	-O2 - уровень оптимизации (безопасная оптимизация всего)
	-lpthread - связывание с многопоточной библиотекой pthread -> link pthread
	-o имя исполняемого файла (без флага создаст a.out)
	
 */

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

/*
 * Конфигурация сервера.
 */
#define PORT 1027
#define BACKLOG 5
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

int Bind(int socket, struct sockaddr *addr, socklen_t addrlen)
{
        int rc;
        
        rc = bind(socket, addr, addrlen);
        if(rc == -1) error("bind()");

        return rc;
}

int Listen(int socket, int backlog)
{
        int rc;
        
        rc = listen(socket, backlog);
        if(rc == -1) error("listen()");

        return rc;
}

int Accept(int socket, struct sockaddr *addr, socklen_t *addrlen)
{
        int rc;
        
        for(;;) {
                rc = accept(socket, addr, addrlen);
                if(rc != -1) break;
		//EINTR - Системный вызов прервал сигналом, который поступил до момента прихода допустимого соединения
		//ECONNABORTED - Соединение было прервано
                if(errno == EINTR || errno == ECONNABORTED) continue;
                error("accept()");
        }               

        return rc;      
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

size_t Read(int fd, void *buf, size_t count)
{
        ssize_t rc;
        
        for(;;) {
		//количество успешно прочитанных байтов (не более count)
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
		//В случае успеха возвращается количество записанных байтов.
                rc = write(fd, buf, count);
                if(rc != -1) break;
		//EINTR - Системный вызов прервал сигналом, который поступил до момента прихода допустимого соединения
                if(errno == EINTR) continue;
                error("write()");
        }
        //сколько записали (не более count)
        return rc;
}

void *Malloc(size_t size)
{
        void *rc;
        
        rc = malloc(size);
        if(rc == NULL) error("malloc()");
        
        return rc;
}

/*
 * Чтение строки из сокета.
 */
//откуда + куда + сколько 
size_t reads(int socket, char *s, size_t size)
{
        char *p;
        size_t n, rc;
        
        /* Проверить корректность переданных аргументов. */
        if(s == NULL) {
                errno = EFAULT; //неверный адрес
                error("reads()");
        }
	//если хотели считать 0, ничего читать не нужно, уходим
        if(!size) return 0;

        p = s;
        size--;
        n = 0;
        while(n < size) {
		//читаем по одному байту
                rc = Read(socket, p, 1);
                if(rc == 0) break;
		//с новой строки не читаем
                if(*p == '\n') {
                        p++;
                        n++;
                        break;
                }
                p++;
                n++;
        }
        *p = 0;
        
        return n;
}

/*
 * Запись count байтов в сокет.
 */
//куда + откуда + сколько записывать
size_t writen(int socket, const char *buf, size_t count)
{
        const char *p;
        size_t n, rc;

        /* Проверить корректность переданных аргументов. */
        if(buf == NULL) {
                errno = EFAULT; //неверный адрес
                error("writen()");
        }
        
	//теперь указываем на переданное "откуда"
        p = buf;
	//переданное "сколько"
        n = count;
        while(n) {
                rc = Write(socket, p, count);
		//отнимает количество байт, которое удалось записать
                n -= rc;
		//сдвигаем указатель на начало незаписанных байтов
                p += rc;
        }

        return count;
}

void *serve_client(void *arg)
{
        int socket;
        
        /* Перевести поток в отсоединенное (detached) состояние. */
	// когда он завершается, все занимаемые им ресурсы освобождаются и мы не можем отслеживать его завершение
	//pthread_self - получение потоком своего идентификатора
        
	//забираем дескриптор сокета из аргумента
        socket = *((int *) arg);
        int length = rand()%30;
        char* s = Malloc(sizeof(char)*(length+1));
   	int i, j, set_len;
   	char SET[] = "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm123456789&^$#@!~`";
   	srand(time(NULL));
   	set_len = strlen(SET);
        free(arg);
   	for (i = 0; i < length - 1 && i < set_len; i++){
      	        j = rand() % (set_len - i) + i;
      	        s[i] = SET[j];
      	        SET[j] = SET[i];
   	}
   	s[i] = '\n';
	writen(socket,s,length);
	
        Close(socket);

        return NULL;
}

int main(void)
{
    srand(time(NULL));

	//прослушивающий сокет ждет запроса на соединение, ни с кем не соединен
        int lsocket;    /* Дескриптор прослушиваемого сокета. */

	//активный сокет, соединен с удаленным активным сокетом через открытое соединение данных
	//уничтожится при закрытии соединения
        int csocket;    /* Дескриптор присоединенного сокета. */
	
	//adress_family + sin_port + sin_addr + sin_zero (не используется)
        struct sockaddr_in servaddr;

        int *arg;

	//идентификатор потока (по сути число)
                
        /* Создать сокет. */
	//PF_INET - IP версии 4 (PF_UNIX, PF_LOCAL - протокол Unix для локального взаимодействия)
	//SOCK_STREAM - надежный двусторонний обмен потоками байтов 
	//(SOCK_DGRAM - ненадежный обмен на основе передачи датаграмм без установления соединения)
	//Третий параметр указывает номер конкретного протокола в рамках указанного семейства для указанного типа сокета. 
	//Как правило, существует единственный протокол для каждого типа сокета внутри каждого семейства.
	
	//Вернет дескриптор сокета (некоторый идентификатор, например, номер записи в системной таблице)
        lsocket = Socket(PF_INET, SOCK_STREAM , 0);

        /* Инициализировать структуру адреса сокета сервера. */
	//заполняем нулями
        memset(&servaddr, 0, sizeof(servaddr));
	//AF_INET соответствует Internet-домену (AF -> address family) 
	//(AF_UNIX для передачи данных используется файловая система ввода/вывода Unix)
        servaddr.sin_family = AF_INET;
	//htons преобразует u_short из хоста в сетевой порядок байтов TCP/IP (сетевой - человеческий, в памяти - обратный).
        servaddr.sin_port = htons(PORT);
	//аналогично для целого
	//INADDR_ANY - любой локальный интерфейс (= 0)
	//если нужен конкретный адрес = inet_addr ("192.168.78.2")
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        
        /* Связать сокет с локальным адресом протокола. */
	//После создания с помощью socket(2), сокет появляется в адресном пространстве (семействе адресов), но без назначенного адреса. 
	//bind() назначает адрес, заданный в addr, сокету, указываемому дескриптором файла sockfd.
        Bind(lsocket, (SA *) &servaddr, sizeof(servaddr));

        /* Преобразовать неприсоединенный сокет в пассивный. */
	//Вызов listen() помечает сокет, указанный в sockfd как пассивный,
	//то есть как сокет, который будет использоваться для приёма запросов входящих соединений
        Listen(lsocket, BACKLOG);
                        
        for(;;) {

		//извлекает первый запрос на соединение из очереди ожидающих соединений прослушивающего сокета,
		//создаёт новый подключенный сокет и и возвращает новый файловый дескриптор, указывающий на сокет
                csocket = Accept(lsocket, NULL, 0);
		
                arg = Malloc(sizeof(int));
                *arg = csocket;
                serve_client(arg);
        }
        
        return 0;
}
