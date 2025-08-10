#include <iostream>
#include <unistd.h>
#include <string.h>
#include <functional>
#include "util.h"
#include "Buffer.h"
#include "InetAddress.h"
#include "ThreadPool.h"

using namespace std;

void oneClient(int msgs, int wait){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    errif(sockfd == -1, "socket create error");
    InetAddress *addr = new InetAddress("127.0.0.1", 8080);
    errif(connect(sockfd, (struct sockaddr *)&addr->addr, addr->addr_len) == -1, "socket connect error");

    Buffer *sendBuffer = new Buffer();
    Buffer *readBuffer = new Buffer();

    sleep(wait);
    int count = 0;
    while(count < msgs){
        sendBuffer->RetrieveAll();
        sendBuffer->Append("I'm client! count: " + std::to_string(count++));
        ssize_t write_bytes = write(sockfd, sendBuffer->Peek(), sendBuffer->GetReadablebytes());
        if(write_bytes == -1){
            printf("socket already disconnected, can't write any more!\n");
            break;
        }
        int already_read = 0;
        char buf[1024];    //这个buf大小无所谓
        while(true){
            bzero(&buf, sizeof(buf));
            ssize_t read_bytes = read(sockfd, buf, sizeof(buf));
            if(read_bytes > 0){
                readBuffer->Append(buf, read_bytes);
                already_read += read_bytes;
            } else if(read_bytes == 0){         //EOF
                printf("server disconnected!\n");
                exit(EXIT_SUCCESS);
            }
            if(already_read >= sendBuffer->GetReadablebytes()){
                printf("count: %d, message from server: %s\n", count++, readBuffer->RetrieveAllAsString().c_str());
                break;
            } 
        }
        readBuffer->RetrieveAll();
    }
    delete addr;
}

int main(int argc, char *argv[]) {
    int threads = 100;
    int msgs = 100;
    int wait = 0;
    int o;
    const char *optstring = "t:m:w:";
    while ((o = getopt(argc, argv, optstring)) != -1) {
        switch (o) {
            case 't':
                threads = stoi(optarg);
                break;
            case 'm':
                msgs = stoi(optarg);
                break;
            case 'w':
                wait = stoi(optarg);
                break;
            case '?':
                printf("error optopt: %c\n", optopt);
                printf("error opterr: %d\n", opterr);
                break;
        }
    }

    ThreadPool *poll = new ThreadPool(threads);
    // sleep(1);
    std::function<void()> func = std::bind(oneClient, msgs, wait);
    for(int i = 0; i < threads; ++i){
        poll->add(func);
    }
    delete poll;
    return 0;
}
