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
#include<fcntl.h>

#define NUM_FILE 20
#define MAX_CLI 10
#define NAMELEN 20
#define CODELEN 10
#define HEADER 8
#define MAXLINE 1008
#define LISTENQ 10

void initialize();
void sig_end(int signo);
void tcp_ser(int, struct sockaddr*, socklen_t);
void clear_data(int);

struct client_list{
    int fd;
    socklen_t len;
    char name[NAMELEN];
    struct sockaddr_in info;
    int file_sent[NUM_FILE];

    int check;
    char to[MAXLINE], fr[MAXLINE];
    char *toiptr, *tooptr, *friptr, *froptr;
    FILE *ifile, *ofile;

}client_ID[MAX_CLI];

struct client_file{
    int file_amount;
    int file_status[NUM_FILE];
    char file_name[NUM_FILE][NAMELEN];

}client_box_status[MAX_CLI];

int main(int argc, char *argv[]){
    
    if(argc != 2){
        printf("[Usage] ./server <port>\n");
        exit(-1);
    }
    else if((atoi(argv[1]) < 1024) || (atoi(argv[1]) > 65535)){
        printf("[Warning] Port must be between 1024 to 65535\n");
        exit(-1);
    }

    int listenfd;
    struct sockaddr_in serverInfo;
    initialize();

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
    
    signal(SIGINT, sig_end);

    printf("[Status] Server on! Port:%s\n", argv[1]);
    tcp_ser(listenfd, (struct sockaddr *)&serverInfo, sizeof(serverInfo));



    return 0;
}

void sig_end(int signo){
    rmdir("server_temp");
    exit(0);
}

void initialize(){
    //client list & client box status
    int i, j;
    for(i = 0; i < MAX_CLI; i++){
        client_ID[i].fd = -1;
        client_ID[i].len = 0;
        client_ID[i].check = 0;
        memset(client_ID[i].name, 0, NAMELEN);
        memset(client_ID[i].to, 0, MAXLINE);
        memset(client_ID[i].fr, 0, MAXLINE);
        bzero(&client_ID[i].info, sizeof(client_ID[i].info));
        
        client_ID[i].toiptr = client_ID[i].tooptr = client_ID[i].to;
        client_ID[i].friptr = client_ID[i].froptr = client_ID[i].fr;

        client_box_status[i].file_amount = 0;

        for(j = 0; j < NUM_FILE; j++){
            client_ID[i].file_sent[j] = 0;

            client_box_status[i].file_status[j] = 0;
            memset(client_box_status[i].file_name[j], 0, NAMELEN);
        }
    }
}

void tcp_ser(int listenfd, struct sockaddr* serverInfo, socklen_t len){
    int maxi, maxfd, connfd, nready;
    int i, j, val;
    char server_msg[MAXLINE];
    struct sockaddr_in clientInfo;
    socklen_t clientlen;
    fd_set rset, wset;

    maxi = -1;
    maxfd = listenfd+1;
    
    
    while(1){
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_SET(listenfd, &rset);

        //add possible socket to FD_SET
        for(i = 0; i < MAX_CLI; i++){
            if(client_ID[i].fd != -1){
                if(client_ID[i].friptr < &(client_ID[i].fr[MAXLINE]))
                    FD_SET(client_ID[i].fd, &rset);
            }
        }
        
        nready = select(maxfd, &rset, &wset, NULL, NULL);
        while(nready){
            int n;
            memset(server_msg, 0, MAXLINE);

            //when new client tries to connect
            if(FD_ISSET(listenfd, &rset)){
                clientlen = sizeof(clientInfo);

                if((connfd = accept(listenfd, (struct sockaddr*)&clientInfo, &clientlen)) < 0){
                    printf("[Error] Fail in accepting a client\n");
                    exit(-5);
                }

                //find a free client spot
                for(i = 0; i < MAX_CLI; i++){
                    if(client_ID[i].fd == -1){
                        client_ID[i].fd = connfd;
                        //set socket to nonblocking
                        val = fcntl(client_ID[i].fd, F_GETFL, 0);
                        fcntl(client_ID[i].fd, F_SETFL, val | O_NONBLOCK);
                        
                        if(connfd > maxfd)
                            maxfd = connfd+1;
                        if(i > maxi)
                            maxi = i;
                        break;
                    }
                }
                //if server full, i == MAX_CLI
                if(i == MAX_CLI){
                    printf("[Warning] Someone tries to join the server, but it's full\n");
                    strcpy(server_msg, "[Server] Server is currently full, please try again later\n");
                    write(connfd, server_msg, strlen(server_msg));
                    close(connfd);
                }
                else{
                    client_ID[i].len = clientlen;
                    client_ID[i].info = clientInfo;
                }       
                if((--nready) <= 0)
                    break;
            }//end of FD_ISSET(listenfd...)
            
            for(i = 0; i <= maxi; i++){
                if(client_ID[i].fd < 0)
                    continue;
                else{
                    int msg_len = (&(client_ID[i].fr[MAXLINE])-client_ID[i].friptr);
                    if(FD_ISSET(client_ID[i].fd, &rset)){
                        if((n=read(client_ID[i].fd, client_ID[i].friptr, msg_len)) < 0){
                            if(errno != EWOULDBLOCK){
                                printf("[Error] Fail in read socket\n");
                                exit(-6);
                            }
                        }
                        else if(n == 0){
                            printf("[Status] %s has left the server\n", client_ID[i].name);
                            close(client_ID[i].fd);
                            clear_data(i);
                        }
                        else{
                                                        

                        }    
                        if((--nready) <= 0)
                            break;
                    }
                }
            }//end of for(maxi...)
        }//end of while(nready)
        


    }//end of while(1)
    
    return;
}

void clear_data(int current){
    int i;

    client_ID[current].check = 0;
    client_ID[current].len = 0;
    memset(client_ID[current].name, 0, NAMELEN);
    memset(client_ID[current].to, 0, MAXLINE);
    memset(client_ID[current].fr, 0, MAXLINE);
    bzero(&client_ID[current].info, sizeof(client_ID[current].info));
        
    client_ID[current].toiptr = client_ID[current].tooptr = client_ID[current].to;
    client_ID[current].friptr = client_ID[current].froptr = client_ID[current].fr;

    client_box_status[current].file_amount = 0;

    for(i = 0; i < NUM_FILE; i++){
        client_ID[current].file_sent[i] = 0;

        client_box_status[current].file_status[i] = 0;
        memset(client_box_status[current].file_name[i], 0, NAMELEN);
    }

    client_ID[current].fd = -1;

    return;
}
