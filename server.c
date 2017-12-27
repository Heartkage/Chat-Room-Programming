#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<stdbool.h>
#include<errno.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<signal.h>
#include<sys/stat.h>
#include<sys/types.h>

#define CODELEN 10
#define MAXLINE 1000
#define LISTENQ 10


int main(int argc, char *argv[]){
    
    if(argc != 2){
        printf("[Usage] ./server <port>\n");
        exit(-1);
    }
    else if((atoi(argv[1]) < 1024) || (atoi(argv[1]) > 65535)){
        printf("[Warning] Port must be between 1024 to 65535\n");
        exit(-1);
    }

    int listenfd, connfd, len;
    int maxi, maxfd, nready;

    struct sockaddr_in serverInfo;

    if((listenfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("[Error] Fail in creating socket\n");
        exit(-2);
    }

    bzero(&serverInfo, sizeof(serverInfo));
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = htonl(INADDR_ANY);
    serverInfo.sin_port = htons(atoi(argv[1]));
    
    if(bind(listenfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) < 0){
        printf("[Error] Fail in binding socket\n");
        exit(-3);
    }

    listen(listenfd, LISTENQ);
    
    int status;
    status = mkdir("server_temp", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    status = mkdir("server_temp/hello", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    status = mkdir("server_temp/hello2", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    printf("hello world\n");

    status = rmdir("server_temp/hello");
    status = rmdir("server_temp/hello2");
    status = rmdir("server_temp");




    return 0;
}
