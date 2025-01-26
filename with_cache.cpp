#include "proxy_parse.h"
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <string>
#include <memory>
#include <vector>

#define MAX_BYTES 4096
#define MAX_CLIENTS 400
#define MAX_SIZE 200 * (1 << 20)
#define MAX_ELEMENT_SIZE 10 * (1 << 20)

using namespace std;

class CacheElement
{
public:
    string data;
    int len;
    string url;
    time_t lru_time;
    CacheElement *next;

public:
    CacheElement(string data, int len, string url, time_t lru_time, CacheElement *next)
    {
        this->data = data;
        this->len = len;
        this->url = url;
        this->lru_time = lru_time;
        this->next = next;
    }
};

int addCacheElement(char *data, int size, char *url);
void removeCacheElement();
CacheElement *find(char *url);

int PORT = 8080;
int proxySocketID;
pthread_t tid[MAX_CLIENTS];
sem_t semaphore;
pthread_mutex_t lock;

CacheElement *head;
int cacheSize;

int checkHTTPversion(char *msg)
{
    int version = -1;

    if (strncmp(msg, "HTTP/1.1", 8) == 0)
    {
        version = 1;
    }
    else if (strncmp(msg, "HTTP/1.0", 8) == 0)
    {
        version = 1;
    }
    else
        version = -1;

    return version;
}

int sendErrorMsg(int socket, int status_code)
{
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code)
    {
    case 400:
        snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
        printf("400 Bad Request\n");
        send(socket, str, strlen(str), 0);
        break;

    case 403:
        snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
        printf("403 Forbidden\n");
        send(socket, str, strlen(str), 0);
        break;

    case 404:
        snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
        printf("404 Not Found\n");
        send(socket, str, strlen(str), 0);
        break;

    case 500:
        snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
        // printf("500 Internal Server Error\n");
        send(socket, str, strlen(str), 0);
        break;

    case 501:
        snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
        printf("501 Not Implemented\n");
        send(socket, str, strlen(str), 0);
        break;

    case 505:
        snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
        printf("505 HTTP Version Not Supported\n");
        send(socket, str, strlen(str), 0);
        break;

    default:
        return -1;
    }
    return 1;
}

int connectRemoteServer(char *hostAddr, int portNum)
{
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocket < 0)
    {
        cout << "Error in creating socket" << endl;
        return -1;
    }
    struct hostent *host = gethostbyname(hostAddr);
    if (host == NULL)
    {
        fprintf(stderr, "No such host exist\n");
        return -1;
    }
    struct sockaddr_in serverAddr;
    bzero((char *)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(portNum);
    bcopy((char *)&host->h_addr, (char *)serverAddr.sin_addr.s_addr, host->h_length);
    if (connect(remoteSocket, (struct sockaddr *)&serverAddr, (size_t)sizeof(serverAddr)) < 0)
    {
        fprintf(stderr, "Error in connecting");
        return -1;
    }
    return remoteSocket;
}

int handleRequest(int clientSocketID, ParsedRequest *request, char *tempReq)
{
    char *buf = (char *)malloc(sizeof(char) * MAX_BYTES);
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");
    size_t len = strlen(buf);
    if (ParsedHeader_set(request, "Connection", "close") < 0)
    {
        cout << "Set host header error" << endl;
    }
    if (ParsedHeader_get(request, "Host") == NULL)
    {
        if (ParsedHeader_set(request, "Host", request->host) < 0)
        {
            cout << "Set host header error" << endl;
        }
    }
    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len))
    {
        cout << "Unparse failed" << endl;
    }
    int serverPort = 80;
    if (request->port != NULL)
    {
        serverPort = atoi(request->port);
    }
    int remoteSocketId = connectRemoteServer(request->host, serverPort);
    if (remoteSocketId < 0)
        return -1;
    int bytesSend = send(remoteSocketId, buf, strlen(buf), 0);
    bzero(buf, MAX_BYTES);
    bytesSend = recv(remoteSocketId, buf, MAX_BYTES - 1, 0);
    char *tempBuffer = (char *)malloc(sizeof(char) * MAX_BYTES);
    int tempBufferSize = MAX_BYTES;
    int tempBufferIndex = 0;
    while (bytesSend > 0)
    {
        bytesSend = send(clientSocketID, buf, bytesSend, 0);
        for (int i = 0; i < bytesSend / sizeof(char); i++)
        {
            tempBuffer[tempBufferIndex] = buf[i];
            tempBufferIndex++;
        }
        tempBufferSize += MAX_BYTES;
        tempBuffer = (char *)realloc(tempBuffer, tempBufferSize);
        if (bytesSend < 0)
        {
            perror("Error in sending data to client\n");
            break;
        }
        bzero(buf, MAX_BYTES);
        bytesSend = recv(remoteSocketId, buf, MAX_BYTES - 1, 0);
    }
    tempBuffer[tempBufferIndex] = '\0';
    free(buf);
    addCacheElement(tempBuffer, strlen(tempBuffer), tempReq);
    free(tempBuffer);
    close(remoteSocketId);
    return 0;
}

void *threadFn(void *socketNew)
{
    sem_wait(&semaphore);
    int s;
    sem_getvalue(&semaphore, &s);
    cout << "Semaphore value: " << s;
    int *t = (int *)socketNew;
    int socket = *t;
    int bytesSendClient, len;
    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));
    bzero(buffer, MAX_BYTES);
    bytesSendClient = recv(socket, buffer, MAX_BYTES, 0);
    while (bytesSendClient > 0)
    {
        len = strlen(buffer);
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {
            bytesSendClient = recv(socket, buffer + len, MAX_BYTES - len, 0);
        }
        else
        {
            break;
        }
    }
    char *tempReq = (char *)malloc(strlen(buffer) * sizeof(char) + 1);
    for (int i = 0; i < strlen(buffer); i++)
    {
        tempReq[i] = buffer[i];
    }
    CacheElement *temp = find(tempReq);
    if (temp != NULL)
    {
        int size = temp->len / sizeof(char);
        int pos = 0;
        char response[MAX_BYTES];
        while (pos < size)
        {
            bzero(response, MAX_BYTES);
            for (int i = 0; i < MAX_BYTES; i++)
            {
                response[i] = temp->data[i];
                pos++;
            }
            send(socket, response, MAX_BYTES, 0);
        }
        cout << "Data retrieved from the cache\n";
        cout << response << endl
             << endl;
    }
    else if (bytesSendClient > 0)
    {
        len = strlen(buffer);
        ParsedRequest *request = ParsedRequest_create();
        if (ParsedRequest_parse(request, buffer, len) < 0)
        {
            cout << "Parsing failed" << endl;
        }
        else
        {
            bzero(buffer, MAX_BYTES);
            if (!strcmp(request->method, "GET"))
            {
                if (request->host && request->path && checkHTTPversion(request->version) == 1)
                {
                    bytesSendClient = handleRequest(socket, request, tempReq);
                    if (bytesSendClient == -1)
                    {
                        sendErrorMsg(socket, 500);
                    }
                }
                else
                {
                    sendErrorMsg(socket, 500);
                }
            }
            else
            {
                cout << "Does not handle methods apart from GET" << endl;
            }
        }
        ParsedRequest_destroy(request);
    }
    else if (bytesSendClient == 0)
    {
        cout << "Client is disconnected" << endl;
    }
    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);
    sem_getvalue(&semaphore, &s);
    cout << "Semaphore post value is " << s << endl;
    free(tempReq);
    return NULL;
}

int main(int argc, char *argv[])
{
    int clientSocketID, clientLen;
    struct sockaddr_in serverAddr, clientAddr;
    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);
    if (argc == 2)
        PORT = atoi(argv[1]);
    else
    {
        cout << "Too few arguments" << endl;
        exit(1);
    }
    cout << "Starting proxy server at PORT: " << PORT << endl;
    proxySocketID = socket(AF_INET, SOCK_STREAM, 0);
    if (proxySocketID < 0)
    {
        perror("Failed to create a socket");
        exit(1);
    }
    int reuse = 1;
    if (setsockopt(proxySocketID, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt failed");
    }
    bzero((char *)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(proxySocketID, (struct sockaddr *)&serverAddr, sizeof(serverAddr) < 0))
    {
        perror("Port is not available\n");
        exit(1);
    }
    cout << "Binding on PORT: " << PORT << endl;
    int listenStatus = listen(proxySocketID, MAX_CLIENTS);
    if (listenStatus < 0)
    {
        perror("Error in listening\n");
        exit(1);
    }
    int i = 0;
    int connectedSocketID[MAX_CLIENTS];
    while (1)
    {
        bzero((char *)&clientAddr, sizeof(clientAddr));
        clientLen = sizeof(clientAddr);
        clientSocketID = accept(proxySocketID, (struct sockaddr *)&clientAddr, (socklen_t *)&clientLen);
        if (clientSocketID < 0)
        {
            perror("Error in connecting to client socket\n");
            exit(1);
        }
        else
        {
            connectedSocketID[i] = clientSocketID;
        }
        struct sockaddr_in *clientPt = (struct sockaddr_in *)&clientAddr;
        struct in_addr ipAddr = clientPt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
        cout << "Client is connected with PORT: " << ntohs(clientAddr.sin_port) << " and IP address is " << str << endl;
        pthread_create(&tid[i], NULL, threadFn, (void *)&connectedSocketID[i]);
        i++;
    }
    close(proxySocketID);
    return 0;
}