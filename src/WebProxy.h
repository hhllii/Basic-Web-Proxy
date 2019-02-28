#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <string> 
#include <arpa/inet.h>
#include <unistd.h>
#include<errno.h>
#include<iostream>
#include<fstream>
#include <netdb.h>
#include<time.h>
#include<sys/time.h>

#include "simpleSocket.h" 
#define LOG_FILE "access.log"
//#define FORBIDDEN_FILE_PATH "./forbidden_site.txt"

using namespace std;
struct ThreadAttri{
    int sockclient;

};
enum HTTPRequest{
    GET, HEAD
};