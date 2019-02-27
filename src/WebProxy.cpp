#include "WebProxy.h"

char banfile[FILENAME_MAX];
FILE *logfile;
pthread_mutex_t logLock;



int banCheck(const char* url){
    FILE *ban;
    ban = fopen(banfile, "r");
    if(ban == NULL){
        printf("Open forbidden-sites-file failed!\n");
        return -1;
    }
    char buffer[1024];
    while(fgets(buffer, 1024, ban)){
        if(buffer[strlen(buffer) - 1] == '\n'){
            buffer[strlen(buffer) - 1] = '\0';
        }
        if(!strcmp(buffer, url)){
            return 1;
        }
        memset(buffer, 0, 1024);
    }
    return 0;
}

void writeLog(char* clientIP,const char* request, int code, int size){
    // Get time
    struct tm *timeptr;
	time_t lt;
	char timestr[30];
	lt=time(NULL);
	timeptr=localtime(&lt);
	strftime(timestr,100,"%Y-%m-%dT%X",timeptr);
    string stime = timestr;
    string sclien = clientIP;
    string srequest = request;
    srequest = "\"" + srequest + "\"";
    string scode = to_string(code);
    string ssize = to_string(size);

    string slogline = stime + " " + sclien + " " + srequest + " " + scode + " " + ssize + "\n";
    //write to log
    pthread_mutex_lock(&logLock);
    // Open log file
    logfile = fopen(LOG_FILE, "a");
    if(logfile == NULL){
        printf("Failed to open log file\n"); 
    }
    fwrite(slogline.c_str(), strlen(slogline.c_str()), sizeof(char), logfile);
    printf("*Wrote log:%s\n", slogline.c_str());
    fclose(logfile);
    pthread_mutex_unlock(&logLock);

}

void divCommand(string commandLine, vector<string> &command){
    int command_pos = 0;
    for(int i = 0; i < 3; ++i){
        int command_size = commandLine.find(" ", command_pos);
        if(command_size == -1){
            command_size = commandLine.size();
        }
        command_size = command_size - command_pos;
        command.push_back(commandLine.substr(command_pos, command_size));
        command_pos += command_size + 1;
        // printf("*Each Command: %s\n", command[i].c_str());
    }
}

void sendError(int sockclient, int code){
    char error[200];
    sprintf(error, "HTTP/1.1 %d\r\n\r\n<html><head>\n<title>%d</title>\n</head></html>\n", code, code);
    if(send(sockclient, error, strlen(error), 0) < 0){
        printf("send message error: %s(errno: %d)\n", strerror(errno), errno);
    }
}

int forwardData(int sockclient, char* address, const char* recvData){
    // Connect to host
    int dataSize = 0;
    int sockserver;
    if( (sockserver = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    setTimeout(sockserver, 2, 2);
    struct sockaddr_in serv_addr; 
    memset(&serv_addr, '0', sizeof(serv_addr)); 
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(80); 
    printf("Address: %s\n", address);
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, address, &serv_addr.sin_addr)<=0)  
    { 
        printf("\n Invalid address: %s\n",address);
        return -1; 
    } 
    if (connect(sockserver, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\n Connection Failed %s(errno: %d)\n",strerror(errno),errno);
        return -1; 
    } 

    printf("Sent request to host\n");
    if(send(sockserver, recvData, strlen(recvData), 0) < 0){
        printf("send message error: %s(errno: %d)\n", strerror(errno), errno);
        return -1;
    }
    char recv_buffer[BUFFER_SIZE];
    while(true){
        memset(recv_buffer, 0, BUFFER_SIZE);
        int recv_size = (int)recv(sockserver, recv_buffer, BUFFER_SIZE, 0);
        if( recv_size >0 )
        {
            //send back to client
            dataSize += recv_size;
            if(send(sockclient, recv_buffer, BUFFER_SIZE, 0) < 0){
                printf("send message error: %s(errno: %d)\n", strerror(errno), errno);
                return -1;
            }
            //printf("%s", recv_buffer);
        }
        else{
            // Handle socket recv error or end
            if((recv_size<0) &&(recv_size == EAGAIN||recv_size == EWOULDBLOCK||recv_size == EINTR)) //error code, connection doesn't fail continue
            {
                printf("\n Recv socket error %s(errno: %d)\n", strerror(errno),errno);
                break;
            }
            return dataSize;
        }
    }
}

void *serverThread(void *arg){
    struct ThreadAttri *temp;
    temp = (struct ThreadAttri *)arg;
    int sockclient = temp->sockclient;
    setTimeout(sockclient, 2, 2);
    string recvData;

    struct sockaddr_in Addclient;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if(!getpeername(sockclient, (struct sockaddr *)&Addclient, &addrlen))
    {
        // printf( "client IP：%s ", inet_ntoa(Addclient.sin_addr));
        // printf( "client PORT：%d ", ntohs(Addclient.sin_port));
    }

    //handleRequest();
    recvData = getHTTPHEAD(sockclient);
    if(recvData.empty()){
        printf("HEAD invalid\n");
    }

    printf("*Received request:\n%s\n", recvData.c_str());
    // Get command line
    string commandLine, method, url, httpv;
    commandLine = getHTTPCommand(recvData);
    vector<string> commands; 
    divCommand(commandLine, commands);
    for(auto c:commands){
        printf("*Each Command: %s\n", c.c_str());
    }
    method = commands[0];
    if(method != "GET" && method != "HEAD"){
        printf("Invalid request error\n");
        //Invalid request error return
        // 405
        sendError(sockclient, 405);
        close(sockclient);
        writeLog(inet_ntoa(Addclient.sin_addr), commandLine.c_str(), 405, 0);
        pthread_exit((void*)-1);
    }
    int url_start = commands[1].find("//", 0);
    int url_end = commands[1].find("/", url_start + 2);
    url = commands[1].substr(url_start + 2, url_end - url_start - 2); // *bug '/' fixed!
    while(url.back() == '/'){
        url.pop_back();
    }
    httpv = commands[2];
    // Host address
    char address[INET_ADDRSTRLEN];
    if(banCheck(url.c_str()) == 1){
        printf("Forbidden site\n");
        //403
        sendError(sockclient, 403);
        close(sockclient);
        writeLog(inet_ntoa(Addclient.sin_addr), commandLine.c_str(), 403, 0);
        pthread_exit((void*)1);
    }

    if(httpv != "HTTP/1.1" && httpv != "HTTP/1.0" &&httpv != "HTTP/0.9"){
        printf("Invalid HTTP version error\n");
        //TODO 400
        sendError(sockclient, 400);
        close(sockclient);
        writeLog(inet_ntoa(Addclient.sin_addr), commandLine.c_str(), 400, 0);
        pthread_exit((void*)-1);
    }
    if(getHostaddress(url.c_str(), address) < 0){
        printf("Invalid address error\n");
        //Invalid address error return
        //TODO 400
        sendError(sockclient, 400);
        close(sockclient);
        writeLog(inet_ntoa(Addclient.sin_addr), commandLine.c_str(), 400, 0);
        pthread_exit((void*)-1);
    }
    printf("Address: %s\n", address);

    // Forward data from host to client
    int dataSize;
    if((dataSize = forwardData(sockclient, address, recvData.c_str())) < 0){
        printf("Failed forward data\n");
        pthread_exit((void*)-1);
    }
    writeLog(inet_ntoa(Addclient.sin_addr), commandLine.c_str(), 200, dataSize);
    pthread_exit((void*)1);
}

int main(int argc, char const *argv[]) {
    int connfd, listenfd;
    struct sockaddr_in  servaddr;
    if(argc != 3){
        printf("usage: ./proxy <listen-port> <forbidden-sites-file>\n");
        return 0;
    }

    strcpy(banfile, argv[2]);

    // Varify port
    if(!portVarify(argv[1])){
        printf("\n Port invalid\n"); 
        return -1;
    }

    int port_num = atoi(argv[1]);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_num);
    // Listen socket create
    if( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    if( bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
        printf("Bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }
    //listenning
    if( listen(listenfd, 10) == -1){
        printf("Listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        return -1;
    }

    printf("======Waiting for client's request======\n");
    while(1){
        if( (connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) == -1){
            printf("Accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }else{
            // Create new thread to handle HTTP request
            pthread_t thid;
            struct ThreadAttri Attri;
            struct ThreadAttri *ptAttri = &Attri;
            
            ptAttri->sockclient = connfd;

            if (pthread_create(&thid,NULL,serverThread,(void*)ptAttri) == -1){
                printf("Thread create error!\n");
                return -1;
            }
            
            printf("*End of the request.\n");
        }
    }
    return 0;
}