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

#define HEADER 8
#define MSGLINE 100
#define MAXLINE 1000

#define LISTENQ 10

void initialize();
void sig_end(int);
void tcp_ser(int, struct sockaddr*, socklen_t);
int msg_check(char *, int, int);
void clear_data(int);

struct client_list{
    int fd, datafd;
    socklen_t len;
    char name[NAMELEN];
    char fname[NAMELEN];
    struct sockaddr_in info;
    int file_sent[NUM_FILE];
    
    int total_size;
    int total_read;
    int file_size;
    bool f_end;
    bool ok_to_read;
    
    char to[MAXLINE], fr[MAXLINE];
    char route[MAXLINE];
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

    printf("[Status] Server on! Port: %s\n", argv[1]);
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
        client_ID[i].datafd = -1;
        client_ID[i].len = 0;
        client_ID[i].total_read = 0;
        client_ID[i].total_size = 0;
        client_ID[i].file_size = 0;

        memset(client_ID[i].name, 0, NAMELEN);
        memset(client_ID[i].fname, 0, NAMELEN);
        memset(client_ID[i].route, 0, MAXLINE);
        memset(client_ID[i].to, 0, MAXLINE);
        memset(client_ID[i].fr, 0, MAXLINE);
        bzero(&client_ID[i].info, sizeof(client_ID[i].info));
        
        client_ID[i].f_end = false;
        client_ID[i].ok_to_read = false;
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
    int i, j;
    char server_msg[MSGLINE], client_msg[MSGLINE];
    struct sockaddr_in clientInfo;
    socklen_t clientlen;
    fd_set allset, rset, dataset, wset;

    FD_ZERO(&allset);
    FD_ZERO(&dataset);
    FD_SET(listenfd, &allset);
    maxi = -1;
    maxfd = listenfd+1;
    
    
    while(1){
        FD_ZERO(&wset);
        rset = allset;

        //add possible socket to FD_SET
        for(i = 0; i < MAX_CLI; i++){
            if(client_ID[i].fd != -1){
                if(client_ID[i].ok_to_read){
                    if(client_ID[i].friptr < (&client_ID[i].fr[MAXLINE])){
                        FD_SET(client_ID[i].datafd, &rset);
                    }
                }
                if(client_ID[i].friptr != client_ID[i].froptr){
                    FD_SET(fileno(client_ID[i].ofile), &wset);
                }         
            }
        }

        nready = select(maxfd, &rset, &wset, NULL, NULL);
             
        while(nready > 0){
            int n, nwritten;

            //when new client tries to connect
            if(FD_ISSET(listenfd, &rset)){
                clientlen = sizeof(clientInfo);

                if((connfd = accept(listenfd, (struct sockaddr*)&clientInfo, &clientlen)) < 0){
                    printf("[Error] Fail in accepting a client\n");
                    exit(-5);
                }
                printf("Someone %d join the server\n", connfd); 
                //find a free client spot
                for(i = 0; i < MAX_CLI; i++){
                    if(client_ID[i].fd == -1){
                        client_ID[i].fd = connfd;
                        //set socket to nonblocking
                        
                        if(connfd > (maxfd-1))
                            maxfd = connfd+1;
                        if(i > maxi)
                            maxi = i;
                        break;
                    }
                }
                //if server full, i == MAX_CLI
                if(i == MAX_CLI){
                    printf("[Warning] Someone tries to join the server, but it's full\n");
                    memset(server_msg, 0, MAXLINE);
                    strcpy(server_msg, "[Server] Server is currently full, please try again later\n");
                    write(connfd, server_msg, strlen(server_msg));
                    close(connfd);
                }
                else{
                    //wait for second datalink connection
                    char code[MSGLINE];
                    memset(code, 0, MSGLINE);
                    sprintf(code, "%d", i);
                    write(connfd, code, strlen(code));

                    client_ID[i].len = clientlen;
                    client_ID[i].info = clientInfo;
                    FD_SET(connfd, &allset);
                }       
                if((--nready) <= 0)
                    continue;
            }//end of FD_ISSET(listenfd...)
            
            for(i = 0; i <= maxi; i++){
                if(client_ID[i].fd < 0)
                    continue;
                else{
                    if(FD_ISSET(client_ID[i].fd, &rset)){
                        if((n=read(client_ID[i].fd, client_msg, MSGLINE)) == 0){
                            printf("[Status] %s has left the server\n", client_ID[i].name);
                            FD_CLR(client_ID[i].fd, &allset);
                            
                            close(client_ID[i].datafd);
                            close(client_ID[i].fd);
                            
                            clear_data(i);
                        }
                        else{
                            int c_msg = msg_check(client_msg, i, n);
                            char temp[MSGLINE];
                            memset(temp, 0, MSGLINE);
                            memset(server_msg, 0, MSGLINE);

                            //Client sent name, assign directory
                            if(c_msg == 0){
                                printf("[Status] %s has joined the server\n", client_ID[i].name);
                                strcpy(temp, "server_temp/");
                                strcat(temp, client_ID[i].name);
                                strcpy(client_ID[i].route, temp);

                                mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                            }
                            else if(c_msg == 1){
                                strcpy(temp, client_ID[i].route);
                                strcat(temp, "/");
                                strcat(temp, client_ID[i].fname);
                                printf("file = %s\n", temp);
                                if((client_ID[i].ofile=fopen(temp, "wb")) == NULL){
                                    printf("[Error] Fail in opening a file\n");
                                    exit(-8);
                                }  
                                client_ID[i].ok_to_read = true;
                            }
                            else if(c_msg == 3){
                                FD_CLR(client_ID[i].fd, &allset);
                                clear_data(i);
                            }
                        }
                        if((--nready) <= 0)
                            break;
                    }//end of if(fd is set)

                    if(FD_ISSET(client_ID[i].datafd, &rset)){
                        //printf("datafd\n");
                        if((n=read(client_ID[i].datafd, client_ID[i].friptr, (&client_ID[i].fr[MAXLINE])-client_ID[i].friptr)) < 0){
                            if(errno != EWOULDBLOCK){
                                printf("[Error] Failed in reading file data from client\n");
                                exit(-8);
                            }
                        }
                        else if(n == 0){
                            printf("[Error] Closing datafd\n");
                            close(client_ID[i].datafd);
                            exit(-8);
                        }
                        else{
                            client_ID[i].friptr = client_ID[i].friptr + n;
                            printf("[Receive] Receive %d bytes from user %s\n", n, client_ID[i].name);
                            int j;
                            client_ID[i].total_read += n;

                            if(client_ID[i].total_read == client_ID[i].total_size){
                                printf("Read complete\n");
                                client_ID[i].total_read = 0;
                                client_ID[i].ok_to_read = false;
                            }
                            /*for(j = 0; j < n; j++)
                                printf("%d", *(client_ID[i].froptr+j));
                            puts("");*/
                            FD_SET(fileno(client_ID[i].ofile), &wset);
                        }
                        //if((--nready) <= 0)
                            //break;
                    }//end of if(datafd is set)

                    if(client_ID[i].ofile != NULL){
                        if(FD_ISSET(fileno(client_ID[i].ofile), &wset) && ((n=(client_ID[i].friptr-client_ID[i].froptr)) > 0)){
                        
                            if((nwritten=write(fileno(client_ID[i].ofile), client_ID[i].froptr, n)) < 0){
                                if(errno != EWOULDBLOCK){
                                    printf("[Error] Fail during write\n");
                                    exit(-9);
                                }
                            }
                            else{
                                printf("[Write] Wrote %d bytes to user %s's file %s\n", nwritten, client_ID[i].name, client_ID[i].fname);  
                                /*int j;
                                for(j = 0; j < nwritten; j++)
                                    printf("%d", *(client_ID[i].froptr+j));
                                puts("");*/
                                client_ID[i].file_size += nwritten;

                                client_ID[i].froptr += nwritten;
                                if(client_ID[i].froptr == client_ID[i].friptr){
                                    client_ID[i].froptr = client_ID[i].friptr = client_ID[i].fr;
                                }   
                                
                                
                                if(client_ID[i].file_size == client_ID[i].total_size){
                                    printf("[Status] Closing file %s\n", client_ID[i].fname);
                                    client_ID[i].file_size = 0;
                                    fclose(client_ID[i].ofile);
                                }
                                        
                            }
                            if((--nready) <= 0)
                                break;
                        }//end of ofile is set
                    }
                }//end of else (fd != -1)
            }//end of for maxi loop

            
            
            
            /*for(i = 0; i <= maxi; i++){
                if(client_ID[i].fd < 0)
                    continue;
                else{
                    if(FD_ISSET(client_ID[i].fd, &rset)){
                        int data_len = (&client_ID[i].fr[MAXLINE])-client_ID[i].friptr;
                        memset(client_msg, 0, MAXLINE);
                        
                        if((n=read(client_ID[i].fd, client_msg, data_len)) < 0){
                            if(errno != EWOULDBLOCK){
                                printf("[Error] Fail in read socket\n");
                                exit(-6);
                            }
                        }
                        else if(n == 0){
                            printf("[Status] %s has left the server\n", client_ID[i].name);
                            close(client_ID[i].fd);
                            close(client_ID[i].datafd);
                            clear_data(i);
                        }
                        else{
                            printf("nread = %d\n", n);
                            client_ID[i].check = msg_check(client_msg, i, n);                                      
                            memset(server_msg, 0, MAXLINE);
                            char temp[MAXLINE];
                            memset(temp, 0, MAXLINE);
                            //Client sent name, assign directory
                            if(client_ID[i].check == 0){
                                printf("[Status] %s has joined the server\n", client_ID[i].name);
                                strcpy(temp, "server_temp/");
                                strcat(temp, client_ID[i].name);
                                strcpy(client_ID[i].route, temp);

                                mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
                            }
                            else if(client_ID[i].check == 1){

                                strcpy(temp, client_ID[i].route);
                                strcat(temp, "/");
                                strcat(temp, client_ID[i].fname);
                                printf("file = %s\n", temp);
                                if((client_ID[i].ofile=fopen(temp, "wb")) == NULL){
                                    printf("[Error] Fail in opening a file\n");
                                    exit(-8);
                                }  
                            }
                            else if(client_ID[i].check == 2){
                                client_ID[i].friptr = client_ID[i].friptr + (n-HEADER);
                                printf("[Receive] Receive %d bytes from user %s\n", (n-HEADER), client_ID[i].name);
                                FD_SET(fileno(client_ID[i].ofile), &wset);
                            }
                        }    
                        --nready;
                    }//end of clientfd is set

                    printf("%d\n", (client_ID[i].friptr-client_ID[i].froptr));
                    
                    if(client_ID[i].ofile != NULL){
                        if(FD_ISSET(fileno(client_ID[i].ofile), &wset) && ((n=(client_ID[i].friptr-client_ID[i].froptr)) > 0)){
                        
                            if((nwritten=write(fileno(client_ID[i].ofile), client_ID[i].froptr, n)) < 0){
                                if(errno != EWOULDBLOCK){
                                    printf("[Error] Fail during write\n");
                                    exit(-9);
                                }
                            }
                            else{
                                printf("[Write] Wrote %d bytes to user %s's file %s\n", nwritten, client_ID[i].name, client_ID[i].fname);  
                                client_ID[i].froptr += nwritten;
                                if(client_ID[i].froptr == client_ID[i].friptr)
                                client_ID[i].friptr = client_ID[i].froptr = client_ID[i].fr;
                            }

                        }//end of ofile is set
                    }
                
                }//end of else
            }//end of for(maxi...)
            */
            

        }//end of while(nready)
        


    }//end of while(1)
    
    return;
}

int msg_check(char *msg, int current, int num){
    //                              01234
    const char codeword0[HEADER] = "/name";
    const char codeword1[HEADER] = "/file";
    const char codeword2[HEADER] = "/end";
    const char codeword3[HEADER] = "/link";

    int i, check_count = 0;
    bool check_space = false;
    char input[HEADER], input2[MSGLINE], input3[MSGLINE];
    memset(input, 0, HEADER);
    memset(input2, 0, MSGLINE);
    memset(input3, 0, MSGLINE);


    for(i = 0; i < HEADER; i++)
        input[i] = msg[i];
    for(i = HEADER; i < num; i++){
        if(msg[i] == ' '){
            printf("found\n");
            check_space = true;
            check_count = i+1;
            continue;
        }
        if(check_space)
            input3[i-check_count] = msg[i];
        else
            input2[i-HEADER] = msg[i];
    }
        
    printf("(1):%s, ", input); 
    printf("(2):%s, ", input2);
    printf("(3):%s\n", input3);

    if(!strcmp(input, codeword0)){
        strcpy(client_ID[current].name, input2);
        return 0;
    }
    else if(!strcmp(input, codeword1)){
        strcpy(client_ID[current].fname, input2);
        write(client_ID[current].fd, "file name received\n", 19);
        client_ID[current].total_size = atoi(input3);
        return 1;
    }
    else if(!strcmp(input, codeword2)){
        printf("End of file\n");
        if(!strcmp(input2, client_ID[current].fname))
            client_ID[current].f_end = true;
        else{
            printf("[Error] File name not the same\n");
            exit(-8);
        }
        return 2;
    }
    else if(!strcmp(input, codeword3)){
        int val;
        int num = atoi(input2);
        client_ID[num].datafd = client_ID[current].fd;

        val = fcntl(client_ID[num].datafd, F_GETFL, 0);
        fcntl(client_ID[num].datafd, F_SETFL, val | O_NONBLOCK);

        return 3;
    }
    else{
        printf("[Error] Message unknown\n");
        exit(-7);
    }
}
void clear_data(int current){
    int i;

    client_ID[current].total_size = 0;
    client_ID[current].total_read = 0;
    client_ID[current].file_size = 0;
    client_ID[current].len = 0;
    memset(client_ID[current].name, 0, NAMELEN);
    memset(client_ID[current].fname, 0, NAMELEN);
    memset(client_ID[current].route, 0, MAXLINE);
    memset(client_ID[current].to, 0, MAXLINE);
    memset(client_ID[current].fr, 0, MAXLINE);
    bzero(&client_ID[current].info, sizeof(client_ID[current].info));
        
    client_ID[current].f_end = false;
    client_ID[current].ok_to_read = false;
    client_ID[current].toiptr = client_ID[current].tooptr = client_ID[current].to;
    client_ID[current].friptr = client_ID[current].froptr = client_ID[current].fr;

    client_box_status[current].file_amount = 0;

    for(i = 0; i < NUM_FILE; i++){
        client_ID[current].file_sent[i] = 0;

        client_box_status[current].file_status[i] = 0;
        memset(client_box_status[current].file_name[i], 0, NAMELEN);
    }

    client_ID[current].fd = -1;
    client_ID[current].datafd = -1;
    return;
}
