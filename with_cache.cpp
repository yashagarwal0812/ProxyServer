#include "proxy_parse.h"
#include <iostream>
#include <cstring>
#include <ctime>

using namespace std;

class CacheElement
{
public:
    string data;
    int len;
    string url;
    time_t lru_time;
    CacheElement *next;

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