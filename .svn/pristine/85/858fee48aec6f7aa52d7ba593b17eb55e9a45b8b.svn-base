/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <sys/select.h>

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);
void unix_error_w(char *msg);
int Rio_writen_w(int fd, void *usrbuf, size_t n);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
void sigint_handler(int sig);
void sigpipe_handler(int sig);
void sigalarm_handler(int sig);
void *thread(void *vargp);
void doit(int connfd, struct sockaddr_in *clientaddr);
sem_t mutex;
typedef struct
{
    int fd;
    struct sockaddr_storage addr;
} client_info_t;
typedef struct
{
    char hostname[MAXLINE];
    char port[MAXLINE];
    char uri[MAXLINE];
} url;
/*
 * parse_uri - URI解析器
 *
 * @param uri        从HTTP代理GET请求中提取的URI字符串，通常是一个完整的URL。
 * @param hostname   预分配的字符串数组，用于存储解析出的主机名，数组长度应至少为MAXLINE字节。
 * @param pathname   预分配的字符串数组，用于存储解析出的路径名，数组长度应至少为MAXLINE字节。
 * @param port       预分配的字符串数组，用于存储解析出的端口号，数组长度应至少为MAXLINE字节。
 *
 * @return           成功解析URI并填充所有输出参数时，返回0。如果解析过程中遇到错误，比如格式不符，返回-1。
 *
 * 该函数分析传入的URI，尝试从中提取主机名、端口号和路径。主要处理"http://"开头的URI。
 * 如果URI不符合预期格式，或在解析过程中遇到任何问题（如找不到主机名结束的分隔符），函数将返回-1。
 * 正常情况下，如果URI符合格式，函数将解析主机名、端口（默认为80，如果未明确指定）和路径，并将它们存储
 * 在相应的输出参数中。
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0)
    {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':')
    {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    }
    else
    {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL)
    {
        pathname[0] = '\0';
    }
    else
    {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - 创建一个格式化的日志条目
 *
 * @param logstring    用于存储生成的格式化日志条目的字符串。
 * @param sockaddr     请求客户端的套接字地址结构体指针，用于获取客户端的IP地址。
 * @param uri          来自客户端请求的URI。
 * @param size         从服务器接收到的字节数。
 *
 * 此函数将时间、客户端IP地址、请求的URI和从服务器接收到的数据大小组合成一个格式化的字符串，
 * 并将此字符串存储在logstring中。该日志条目提供了请求发生时的详细信息，可用于日志记录和审计。
 * 函数首先获取当前时间，然后将时间格式化为可读形式，接着将网络二进制形式的IP地址转换为点分十进制形式，
 * 最后将所有信息格式化进logstring。如果在IP地址转换过程中出现错误，会调用错误处理函数。
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    char host[INET_ADDRSTRLEN];

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (inet_ntop(AF_INET, &sockaddr->sin_addr, host, sizeof(host)) == NULL)
        unix_error("Convert sockaddr_in to string representation failed\n");

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %s %s %zu", time_str, host, uri, size);
}

/**
 * 打印 Unix 风格的错误信息。
 *
 * 该函数将错误信息输出到标准错误输出（stderr），格式为：
 * "<msg>: <strerror(errno)>"
 *
 * @param msg 错误消息字符串，描述错误的上下文。
 */
void unix_error_w(char *msg)
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}

/**
 * 从 rio_t 结构中读取一行数据，并进行错误处理。
 *
 * 该函数包装了 rio_readlineb 函数，提供错误处理功能。
 * 如果读取过程中发生错误，会打印错误信息并返回 0。
 *
 * @param rp 指向 rio_t 结构的指针，表示文件或缓冲区。
 * @param usrbuf 指向用户缓冲区的指针，用于存储读取的数据。
 * @param maxlen 要读取的最大字节数。
 * @return 成功时返回读取的字节数，失败时返回 0。
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t retSize = rio_readlineb(rp, usrbuf, maxlen);
    if (retSize < 0)
    {
        unix_error_w("error in function Rio_readlineb_w");
        return 0;
    }
    else
    {
        return retSize;
    }
}
/**
 * 向指定的文件描述符写入n个字节的数据，并进行错误处理。
 *
 * 该函数包装了 rio_writen 函数，提供错误处理功能。
 * 如果写入的数据量不等于期望的字节数n，会打印错误信息。
 *
 * @param fd 文件描述符，表示要写入数据的文件或缓冲区。
 * @param usrbuf 指向用户缓冲区的指针，包含要写入的数据。
 * @param n 要写入的字节数。
 */
int Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    if (rio_writen(fd, usrbuf, n) != n)
    {
        unix_error_w("error in function Rio_writen_w");
        return -1;
    }
    return 0;
}

/**
 * 从rio_t结构中读取n个字节的数据，并进行错误处理。
 *
 * 该函数包装了rio_readnb函数，提供错误处理功能。
 * 如果读取过程中发生错误，会打印错误信息并返回0。
 *
 * @param rp 指向rio_t结构的指针，表示文件或缓冲区。
 * @param usrbuf 指向用户缓冲区的指针，用于存储读取的数据。
 * @param n 要读取的字节数。
 * @return 成功时返回读取的字节数，失败时返回0。
 */
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t retSize;
    retSize = rio_readnb(rp, usrbuf, n);
    if (retSize < 0)
    {
        unix_error_w("error in function Rio_readnb_w");
        return 0;
    }
    return retSize;
}

void sigint_handler(int sig)
{
    Sio_puts("Proxy closed by SIGINT.\n");
    _exit(0);
}

void sigpipe_handler(int sig)
{
    Sio_puts("Proxy closed by SIGPIPE.\n");
    _exit(0);
}

void sigalarm_handler(int sig)
{
    Sio_puts("Proxy closed by SIGALRM.\n");
    _exit(0);
}

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    Signal(SIGINT, &sigint_handler);
    Signal(SIGPIPE, &sigpipe_handler);
    Signal(SIGALRM, &sigalarm_handler);
    Sem_init(&mutex, 0, 1);
    /* Check arguments */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    int listen_fd = Open_listenfd(argv[1]);
    socklen_t client_len;
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    while (1)
    {
        client_len = sizeof(struct sockaddr_storage);
        client_info_t *client_info =
            (client_info_t *)Malloc(sizeof(client_info_t));

        client_info->fd =
            Accept(listen_fd, (SA *)&client_info->addr, &client_len);
        pthread_create(&tid, NULL, thread, client_info);
    }
    Close(listen_fd);
    exit(0);
}

/**
 * @brief 处理客户端连接的线程函数
 *
 * 该函数由一个新线程执行，用于处理单个客户端的连接。
 * 它会分离自身并分离传入的参数，然后调用处理函数处理客户端的请求，
 * 最后关闭客户端连接。
 *
 * @param vargp 指向客户端信息结构体的指针
 * @return 无返回值
 */
void *thread(void *client_info_vargp)
{
    Pthread_detach(pthread_self());
    client_info_t client_info = *(client_info_t *)client_info_vargp;
    Free(client_info_vargp);
    doit(client_info.fd, &(client_info.addr));
    Close(client_info.fd);
    return NULL;
}

void doit(int connfd, struct sockaddr_in *clientaddr)
{
    /* 连接server并解析相关参数 */
    char buffer[MAXLINE];
    rio_t connect_rio;
    Rio_readinitb(&connect_rio, connfd);
    if (Rio_readlineb_w(&connect_rio, buffer, MAXLINE) == 0)
    {
        fprintf(stderr, "error: read empty request line\n");
        return;
    }
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    if (sscanf(buffer, "%s %s %s", method, uri, version) != 3)
    {
        fprintf(stderr, "error: mismatched parameters\n");
        return;
    }
    char hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE];
    if (parse_uri(uri, hostname, pathname, port) != 0)
    {
        fprintf(stderr, "error: parse uri wrong\n");
        return;
    }

    /* 构建request header */
    char requestheader[MAXLINE];
    size_t content_length = 0;
    size_t n;
    sprintf(requestheader, "%s /%s %s\r\n", method, pathname, version);
    while ((n = Rio_readlineb_w(&connect_rio, buffer, MAXLINE)) != 0)
    {
        if (!strncasecmp(buffer, "Content-Length", 14))
            sscanf(buffer + 15, "%zu", &content_length);
        sprintf(requestheader, "%s%s", requestheader, buffer);
        if (!strncmp(buffer, "\r\n", 2))
            break;
    }
    if (n == 0)
        return;
    /* 连接client */
    int clientfd;
    rio_t client_rio;
    while ((clientfd = open_clientfd(hostname, port)) < 0)
    {
        fprintf(stderr, "error: open client fd wrong\n");
        fprintf(stderr, "error: hostname-%s\n", hostname);
        fprintf(stderr, "error: port-%s\n", port);
        return;
    }
    Rio_readinitb(&client_rio, clientfd);

    /* proxy发送 */
    char send_buffer[MAXLINE];
    int flag = 0;
    size_t size = 0;
    if (Rio_writen_w(clientfd, requestheader, strlen(requestheader)) == -1)
    {
        flag = 1;
        goto RECEIVE;
    }

    if (strcasecmp(method, "GET"))
    {
        int i = 0;
        for (; i < content_length; i++)
        {
            if (Rio_readnb_w(&connect_rio, send_buffer, 1) == 0)
            {
                flag = 1;
                goto RECEIVE;
            }
            if (Rio_writen_w(clientfd, send_buffer, 1) == -1)
            {
                flag = 1;
                goto RECEIVE;
            }
        }
    }
RECEIVE:
    if (!flag)
    {
        /* 说明没有问题，进行receive操作 */
        char receive_buffer[MAXLINE];
        while ((n = Rio_readlineb_w(&client_rio, receive_buffer, MAXLINE)) != 0)
        {
            size += n;
            if (!strncasecmp(receive_buffer, "Content-Length: ", 14))
                sscanf(receive_buffer + 15, "%zu", &content_length);
            if (Rio_writen_w(connfd, receive_buffer, strlen(receive_buffer)) == -1)
            {
                size = 0;
                goto LOG;
            }
            if (!strncmp(receive_buffer, "\r\n", 2))
                break;
        }
        if (!n)
        {
            size = 0;
            goto LOG;
        }
        for (int i = 0; i < content_length; i++)
        {
            if (Rio_readnb_w(&client_rio, receive_buffer, 1) == 0)
            {
                size = 0;
                goto LOG;
            }
            size++;
            if (Rio_writen_w(connfd, receive_buffer, 1) == -1)
            {
                size = 0;
                goto LOG;
            }
        }
    }

LOG:
    format_log_entry(buffer, clientaddr, uri, size);
    P(&mutex);
    printf("%s\n", buffer);
    V(&mutex);
    Close(clientfd);
}