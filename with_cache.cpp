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
#define SEMAPHORE_NAME "/proxy_semaphore"

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

int main(void)
{
    return 0;
}