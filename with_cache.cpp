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
private:
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

int PORT = 8080;
int proxySocketID;
pthread_t tid[MAX_CLIENTS];
sem_t semaphore;
pthread_mutex_t lock;

CacheElement *head;
int cacheSize;

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