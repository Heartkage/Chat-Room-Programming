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

#define CODELEN 10
#define MAXLINE 1000


char value[MAXLINE];
int msg_check(char*);
void str_cli(int);
void upload_file();
void download_file();
void process_sleep(int);

int main(int argc, char *argv[]){
    //input check
    if(argc != 4){
        printf("[Usage] ./client <ip> <port> <username>");
        exit(-1);
    }

    //socket create
    int sockfd;
    if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("[Error] Failed in socket creation\n");
        exit(-2);
    }

    //set serverinfo
    struct sockaddr_in server;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(atoi(argv[2]));

    //connect
    if((connect(sockfd, (struct sockaddr *)&server, sizeof(server))) < 0){
        printf("[Error] Failed in server connection\n");
        exit(-3);
    }

    str_cli(sockfd);
        
    return 0;
}

void str_cli(int sockfd){
    bool end = false;
    int maxfd, nready;
    fd_set rset;
    char input[MAXLINE];
    char output[MAXLINE];    

    if(sockfd > fileno(stdin))
        maxfd = sockfd+1;
    else
        maxfd = fileno(stdin)+1;

    while(!end){
        FD_ZERO(&rset);
        FD_SET(sockfd, &rset);
        FD_SET(fileno(stdin), &rset);

        nready = select(maxfd, &rset, NULL, NULL, NULL);
        while(nready){
            int n;
            memset(input, 0, MAXLINE);

            if(FD_ISSET(sockfd, &rset)){
                if((n=read(sockfd, input, MAXLINE)) == 0){
                    FD_ZERO(&rset);
                    close(sockfd);
                    exit(1);
                }
                else if(n < 0){
                    printf("[Error] Fail in reading server msg\n");
                    exit(-4);
                }
                else{
                    int check = msg_check(input);
                    
                    if(check == 0)
                        write(fileno(stdout), input, n);
                    else if(check == 4)
                        download_file();
                    else{
                        printf("[Error] Server msg undefined\n");
                        exit(-4);
                    }
                    nready--;
                }
            }
            else if(FD_ISSET(fileno(stdin), &rset)){
                if((n=read(fileno(stdin), input, MAXLINE)) == 0){
                    FD_ZERO(&rset);
                    printf("[Terminate] Closing the program\n");
                    exit(2);
                }
                else if(n < 0){
                    printf("[Error] Fail in reading keyboard msg\n");
                    exit(-5);
                }
                else{
                    int check = msg_check(input);

                    if(check == 1)
                        upload_file();
                    else if(check == 2)
                        process_sleep(atoi(value));
                    else if(check == 3)
                        end = true;
                       
                    nready--;
                }
            }
        }//end of while nready

    }//end of while end

    return;
}

void upload_file(){

}

void download_file(){


}

void process_sleep(int sec){

}

int msg_check(char *words){
    if(words[0] != '/')
        return 0;
    
    const char codeword1[CODELEN] = "/put";
    const char codeword2[CODELEN] = "/sleep";
    const char codeword3[CODELEN] = "/exit";
    const char codeword4[CODELEN] = "/file";
    
    memset(value, 0, MAXLINE);
    
    char input[CODELEN];
    char *temp = strtok(words, " \n");
    if(temp != NULL)
        strcpy(input, temp);
    else
        return -1;

    if(!strcmp(input, codeword1)){
        temp = strtok(NULL, "\n");
        if(temp != NULL)
            strcpy(value, temp);
        else{
            printf("[Usage] /put <filename>\n");
            return -1;
        }
        return 1;
    }
    else if(!strcmp(input, codeword2)){
        temp = strtok(NULL, "\n");
        if(temp != NULL)
            strcpy(value, temp);
        else{
            printf("[Usage] /sleep <time in second>\n");
            return -1;
        }
        return 2;
    }
    else if(!strcmp(input, codeword3))
        return 3;
    else if(!strcmp(input, codeword4)){
        temp = strtok(NULL, "\n");
        if(temp != NULL)
            strcpy(value, temp);
        else
            exit(-100);
        return 4;
    }
    else{
        printf("[Warning] Unknown input\n");
        return -1;
    }
}
