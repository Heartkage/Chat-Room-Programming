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
#include<time.h>

#define HEADER 8
#define MSGLINE 100
#define MAXLINE 1000

int file_len = 0;
char value[MAXLINE];
char route[MSGLINE];
int msg_check(char*);
int msg_check2(char*, int);
void str_cli(int, int);
void upload_file(int, int);
void download_file(int, int);
void process_sleep(int);

int main(int argc, char *argv[]){
    //input check
    if(argc != 4){
        printf("[Usage] ./client <ip> <port> <username>");
        exit(-1);
    }

    //socket create
    int sockfd, datafd;
    if((sockfd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("[Error] Failed in socket creation\n");
        exit(-2);
    }
    if((datafd=socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("[Error] Failed in socket creation\n");
        exit(-2);
    }

    //set serverinfo
    struct sockaddr_in server, datalink;
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(atoi(argv[2]));

    bzero(&datalink, sizeof(datalink));
    datalink.sin_family = AF_INET;
    datalink.sin_addr.s_addr = inet_addr(argv[1]);
    datalink.sin_port = htons(atoi(argv[2]));

    memset(route, 0, MSGLINE);
    int i;
    char to[MSGLINE], temp2[MSGLINE];
    //connect and read first msg
    if((connect(sockfd, (struct sockaddr *)&server, sizeof(server))) < 0){
        printf("[Error] Failed in server connection\n");
        exit(-3);
    }
    read(sockfd, temp2, MSGLINE);
    //printf("%s\n", temp2);
    strcpy(route, "temp");
    strcat(route, temp2);

    //make datalink connection and read first msg
    if((connect(datafd, (struct sockaddr *)&datalink, sizeof(datalink))) < 0){
        printf("[Error] Failed in datalink connection\n");
        exit(-3);
    }
    read(datafd, to, MSGLINE);
    
    //send datalink to server
    memset(to, 0, MSGLINE);
    strcpy(to, "/link");
    for(i = 0; i < strlen(temp2); i++)
        to[i+HEADER] = temp2[i];
    write(datafd, to, strlen(temp2)+HEADER);
    
    //send client name to server
    memset(to, 0, MSGLINE);
    strcpy(to, "/name");
    for(i = 0; i < strlen(argv[3]); i++)
        to[i+HEADER] = argv[3][i];
    write(sockfd, to, (HEADER+strlen(argv[3])));
    
    mkdir(route, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    
    printf("Welcome to the dropbox-like server!: %s\n", argv[3]);

    str_cli(sockfd, datafd);
        
    return 0;
}

void str_cli(int sockfd, int datafd){
    bool end = false;
    int maxfd, nready;
    fd_set rset;
    char input[MSGLINE];
    char output[MSGLINE];    

    

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
            memset(input, 0, MSGLINE);

            if(FD_ISSET(sockfd, &rset)){
                if((n=read(sockfd, input, MSGLINE)) == 0){
                    FD_ZERO(&rset);
                    close(sockfd);
                    exit(1);
                }
                else if(n < 0){
                    printf("[Error] Fail in reading server msg\n");
                    exit(-4);
                }
                else{
                    int check = msg_check2(input, n);
        
                    if(check == 0)
                        download_file(sockfd, datafd);
                    else{
                        printf("[Error] Server msg undefined\n");
                        exit(-4);
                    }
                    nready--;
                }
            }
            else if(FD_ISSET(fileno(stdin), &rset)){
                if((n=read(fileno(stdin), input, MSGLINE)) == 0){
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
                        upload_file(sockfd, datafd);
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

void upload_file(int sockfd, int datafd){
    char msg[MSGLINE];
    char msg2[MSGLINE];
    printf("%s\n", value);
    FILE *ifile = fopen(value, "rb");
    
    if(ifile == NULL){
        printf("[Error] Fail in opening file %s\n", value);
        exit(-5);
    }

    int filelen, i;
    char filedata[MAXLINE];
    fseek(ifile, 0, SEEK_END);
    filelen = ftell(ifile);
    fseek(ifile, 0, SEEK_SET);
    
    
    /*printf("filelen = %d\n", filelen);
    for(i = 0; i < filelen; i++)
        printf("%d", filedata[i]);*/
    //send file name
    memset(msg, 0, MSGLINE);
    
    strcpy(msg, "/file");
    for(i = 0; i < strlen(value); i++)
        msg[i+HEADER] = value[i];

    msg[i+HEADER] = ' ';
    i++;

    memset(msg2, 0, MSGLINE);
    sprintf(msg2, "%d", filelen);
    for(i = 0; i < strlen(msg2); i++)
        msg[HEADER+strlen(value)+1+i] = msg2[i];

    for(i = 0; i < HEADER+strlen(value)+1+strlen(msg2); i++)
        printf("%c", msg[i]);
    puts("");
    write(sockfd, msg, HEADER+strlen(value)+1+strlen(msg2));
    printf("Uploading file: %s\n", value);
    //warning
    read(sockfd, msg2, MSGLINE);  

    //send file data
    bool end = false;
    int total_sent = 0;
    int temp_count = 1;
    int current_read;
    i = 0;
    printf("Progress:[");

    while(!end){
        memset(filedata, 0, MAXLINE);
        
        
        size_t num = fread(filedata, sizeof(char), MAXLINE, ifile);

        //printf("%d\n", num);
        
        total_sent += num;
        write(datafd, filedata, num);
        
        if(total_sent == filelen)
            end = true;
        
        while((filelen*temp_count) <= (22*total_sent)){
            temp_count++;
            printf("#");
        } 
    }
    printf("]\n");
    
    //send end of file
    memset(msg, 0, MSGLINE);
    strcpy(msg, "/end");
    
    for(i = 0; i < strlen(value); i++)
        msg[i+HEADER] = value[i];
    
    write(sockfd, msg, strlen(value)+HEADER);
    printf("Upload %s comeplete!\n", value);

    fclose(ifile);
    return;
}

void download_file(int sockfd, int datafd){
    
    FILE *ofile;
    int current_len = 0, n, nwritten, temp_count;
    char server_msg[MAXLINE];
    char temp_route[MSGLINE];
    memset(temp_route, 0, MSGLINE);
    strcpy(temp_route, route);
    strcat(temp_route, "/");
    strcat(temp_route, value);

    //write(sockfd, "ok", 2);
    
    if((ofile=fopen(temp_route, "wb")) == NULL){
        printf("[Error] Fail in opening output file\n");
        exit(-1);
    }

    printf("Downloading file: %s\n", value);
    
    while(current_len != file_len){
        memset(server_msg, 0, MAXLINE);
        
        int amount;
        if((file_len-current_len) < MAXLINE)
            amount = file_len-current_len;
        else
            amount = MAXLINE;

        if((n=read(datafd, server_msg, amount)) <= 0){
            printf("[Error] Something wrong with server\n");
            exit(-2);
        }
        else{
            nwritten = fwrite(server_msg, sizeof(char), n, ofile);
            current_len += nwritten;

            printf("Progress:[");
            for(temp_count = 1; temp_count <= 22 ; temp_count++){
                if(file_len*temp_count <= (22*current_len))
                    printf("#");
                else
                    printf(" ");
            }
            
            if(current_len != file_len)
                printf("]\r");
            else
                printf("]\n");
        }
    }

    memset(server_msg, 0, MSGLINE);
    strcpy(server_msg, "/recv");
    int i;
    for(i = 0; i < strlen(value); i++)
        server_msg[HEADER+i] = value[i];

    write(sockfd, server_msg, HEADER+strlen(value));

    printf("Download %s complete!\n", value);
    fclose(ofile);

   /* memset(server_msg, 0, MAXLINE);
   
    //printf("%s", server_msg);
    if(!strcmp(server_msg, "/end")){
        printf("Download %s complete!\n", value);
        fclose(ofile);
    }
    else{
        printf("[Error] /end didn't received\n");
        exit(-3);
    }*/
    
    return;
}

void process_sleep(int sec){
    int i = 0, counter = 1;
    clock_t start_tick;
    clock_t current_tick;
    
    printf("Client starts to sleep\n");

    start_tick = clock() / CLOCKS_PER_SEC;
    while(1){
        current_tick = clock() / CLOCKS_PER_SEC;

        if(current_tick-start_tick == sec+1){
            break;
        }
        else if(current_tick-start_tick == counter){
            printf("Sleep %d\n", counter);
            counter++;
        }
    }
    printf("Client wakes up\n");

    return;
}

int msg_check(char *words){
    const char codeword1[HEADER] = "/put";
    const char codeword2[HEADER] = "/sleep";
    const char codeword3[HEADER] = "/exit";
    
    memset(value, 0, MAXLINE);
    
    char input[HEADER];
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
    else{
        printf("[Warning] Unknown input\n");
        return -1;
    }
}

int msg_check2(char *msg, int num){
    
    const char codeword0[HEADER] = "/file";

    //printf("hello\n");
    
    int i, check_count = 0;
    bool check_space = false;
    char input[HEADER], input2[MSGLINE], input3[MSGLINE];
    char temp[5];
    memset(input, 0, HEADER);
    memset(input2, 0, MSGLINE);
    memset(input3, 0, MSGLINE);
    memset(temp, 0, 5);

    for(i = 0; i < HEADER; i++)
        input[i] = msg[i];

    //len of the current packet
    temp[0] = input[6];
    temp[1] = input[7];
    

    printf("amount = %d\n", atoi(temp));
    for(i = HEADER; i < atoi(temp); i++){
        if(msg[i] == ' '){
            check_space = true;
            check_count = i+1;
            continue;
        }
        if(check_space){
            input3[i-check_count] = msg[i];
        }
        else
            input2[i-HEADER] = msg[i];
    }

    printf("(1):%s, ", input);
    printf("(2):%s, ", input2);
    printf("(3):%s\n", input3);
    
    if(!strcmp(input, codeword0)){
        memset(value, 0, MAXLINE);
        strcpy(value, input2);
        file_len = atoi(input3);
        return 0;
    }

    return -1;
}
