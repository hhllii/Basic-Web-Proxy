#include"simpleSocket.h"

struct SimpleAddress getAddressbyLine(char* line){
    struct SimpleAddress simpleaddress;
    //read address
    char delim[]=" ";
    char* res = NULL;
    if((res=strtok(line,delim)) != NULL){
        simpleaddress.address = res;
        printf("*Address: %s\n",simpleaddress.address);
    }
    if((res=strtok(NULL,delim)) != NULL){
        res[strlen(res) - 1] = '\0';
        if(portVarify(res)){ //valid port
            simpleaddress.port = atoi(res);
            printf("*Port: %i\n",simpleaddress.port);
        }else{//error port set to default port num
            simpleaddress.port = DEFAULT_PORT;
        }
    }else{ //error port set to default port num
        simpleaddress.port = DEFAULT_PORT;
    }
    return simpleaddress;
}

vector<int> getActiveSockList(vector<SimpleAddress> list){
    vector<int> socklist;
    for(auto add:list){
        struct sockaddr_in serv_addr;
        int sockfd = 0;
        // Create socket address
        memset(&serv_addr, '0', sizeof(serv_addr)); 
        int port_num = add.port;
        serv_addr.sin_family = AF_INET; 
        serv_addr.sin_port = htons(port_num); 

        if( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
            printf("Create socket error: %s(errno: %d)\n",strerror(errno),errno);
            continue;
        }
        // Convert IPv4 and IPv6 addresses from text to binary form 
        if(inet_pton(AF_INET, add.address, &serv_addr.sin_addr)<=0)  
        { 
            printf("\n Invalid address: %s\n", add.address);
            continue;
        } 
        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        { 
            printf("\n Connection Failed %s(errno: %d)\n",strerror(errno),errno);
            continue;
        } 
        // Set timeout
        setTimeout(sockfd, 3, 3);
        //valid socket save it
        socklist.push_back(sockfd);
    }
    return socklist;
}


bool checkdigit(const char* line){
    for(int i = 0; i < strlen(line); ++i){
        if(!isdigit(line[i])){
            //inclued non-digit
            return false;
        }
    }
    return true;
}

bool portVarify(const char* port){
    //-1 for fgets() \n
    if(!checkdigit(port)){
        return false;
    }
    if(atoi(port) < 0 || atoi(port) > 65535){
        //invalid number
        return false;
    }
    return true;
}

int getFileSize(FILE* fp){
    int file_size;
    fseek(fp, 0, SEEK_END ); // to file end
    file_size=ftell(fp);
    fseek(fp, 0, SEEK_SET ); // return to begin
    return file_size;
}

void filecat(FILE* fp1, FILE* fp2){
	char* buffer[1];
	while(fread(buffer, sizeof(char), 1, fp2)){
		fwrite(buffer, sizeof(char), 1, fp1);
	}
}

int getHTTPHEAD(int sockfd, string &head){
    char buffer[1] = {0};
    int cnt = 0;
    int recv_size;
    // Find the end of head with \r\n\r\n
    while(true){
        if((recv_size = recv(sockfd, buffer, 1, 0)) > 0){
            head.push_back(buffer[0]);
            if(buffer[0] == '\r' || buffer[0] == '\n' ){
                ++cnt;
            }else{
                cnt = 0;
            }
            if(cnt == 4){
                // End of head
                // std::cout << "----------------------Start of HEAD----------------- \n" << head << std::endl;
                // std::cout << "----------------------End of HEAD------------------- \n" << std::endl;
                break;
            }
        }else{
            if((recv_size<0) &&(recv_size == EAGAIN||recv_size == EWOULDBLOCK||recv_size == EINTR)){ //error code, connection doesn't fail continue
                printf("Recv header error: %s(errno: %d)\n",strerror(errno),errno);
                return -1;
            }
            // End of connection no header
            return 0;
        }
    }
    // Got head
    return 1;
}

string getHTTPCommand(string header){
    int command_size = header.find("\r\n", 0);
    if(command_size > 0){
        return header.substr(0, command_size);
    }
    return "";
}

int getHostaddress(const char* url, char* raddress){
    struct hostent *hostptr;
    const char *address;
    char host_address[INET_ADDRSTRLEN];
    if((hostptr = gethostbyname(url))==NULL){
        printf("gethostbyname error for host: %s: %s\n",url,hstrerror(h_errno));
        return -1;
    }

    if((address = inet_ntop(hostptr->h_addrtype, hostptr->h_addr, host_address,sizeof(host_address)))<0){
        printf("\n Invalid address: %s\n",url);
        return -1;
    }
    printf("official hostname: %s\n", hostptr->h_name);
    raddress = strcpy(raddress, address);
    return 0;
}

int createSocketAddr(struct sockaddr_in *serv_addr, const char *address, int port){
    memset(serv_addr, '0', sizeof(serv_addr)); 
    serv_addr->sin_family = AF_INET; 
    serv_addr->sin_port = htons(port); 
    if(inet_pton(AF_INET, address, &(serv_addr->sin_addr))<=0)  
    { 
        return -1;
    } 
    return 0;
}

void simpleSocketSend(int sockfd, SimpleChunk* chunk, int chunk_size){
    // Send data
    if (send(sockfd, chunk, chunk_size, 0) < 0)
    {
        printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
    }
    //printf("*Sending: \n%s\n", chunk->buffer);
}

int simpleSocketRecv(int sockfd, char* buffer, int buffer_size){
    while(true){
        int recv_size = (int)recv(sockfd, buffer, buffer_size, 0);
        if( recv_size >0 )
        {
            printf("%s", buffer);
        }
        else{
            // Handle socket recv error
            if((recv_size<0) &&(recv_size == EAGAIN||recv_size == EWOULDBLOCK||recv_size == EINTR)) //error code, connection doesn't fail continue
            {
                printf("\n Socket error %s(errno: %d)\n", strerror(errno),errno);
                return -1;
            }
            //printf("*End of receive \n");
            return 0;
        }
    }
}

void setTimeout(int sockfd, int send_time, int recv_time){
    struct timeval timeout_send={send_time,0};//3s
    struct timeval timeout_recv={recv_time,0};//3s

    int ret_send=setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout_send,sizeof(timeout_send));
    int ret_recv=setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout_recv,sizeof(timeout_recv));

}

void simpleUDPSend(int sockfd, SimpleChunk* chunk, int chunk_size){
    if (send(sockfd, chunk, chunk_size, 0) < 0)
    {
        printf("Send data error: %s(errno: %d)\n", strerror(errno), errno);
    }
}

int simpleUDPRecv(int sockfd, SimpleChunk* chunk, int chunk_size){
        if( recv(sockfd, chunk, chunk_size, 0) < 0 )
        {
            printf("\n Socket error %s(errno: %d)\n", strerror(errno),errno);
                return -1;
        }
}