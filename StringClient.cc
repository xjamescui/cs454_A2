#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "StringClient.h"

using namespace std;

int main(int argc, char *argv[]) {

    int socketd, port, amount;
    struct hostent *server;
    StringClient *client;

    // create socket
    socketd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketd < 0){
        cerr << "ERROR creating a socket on client side, socket is " << socketd << endl;
        exit(1);
    }

    if (getenv("SERVER_ADDRESS") == NULL || getenv("SERVER_PORT") == NULL) {
      cerr << "Please ensure SERVER_ADDRESS and SERVER_PORT environment variables are set" << endl;
      exit(1);
    }

    string host_name = getenv("SERVER_ADDRESS");
    port = atoi(getenv("SERVER_PORT")); 

    cout << "SERVER_ADDRESS=" << host_name << endl;
    cout << "SERVER_PORT=" << port << endl;

    server = gethostbyname(host_name.c_str());

    if (server == NULL) {
        cerr << "ERROR getting the server with name " << host_name << endl;
        exit(0);
    }

    client = new StringClient(server, port);
    client->connectOrDie(socketd);

    // connected!

    string line;
    while(getline(cin, line)){
    
    } // while
} // main

StringClient::StringClient(hostent *server, int port) : server(server), port(port) {

    memset(&(this->serv_addr), 0, sizeof(this->serv_addr));
    this->serv_addr.sin_family = AF_INET;
    memcpy(&this->serv_addr.sin_addr.s_addr, this->server->h_addr, this->server->h_length);
    this->serv_addr.sin_port = htons(this->port);
} // constructor


void StringClient::connectOrDie(int socketd){
    if(connect(socketd, (struct sockaddr*)&(this->serv_addr), sizeof(this->serv_addr)) < 0 ) {
        cerr << "ERROR connecting to socket=" << socketd << " : " << strerror(errno) << endl;
        exit(1);
    }
} // connectOrDie

int StringClient::sendMessage(int socketd, string message){
    return write(socketd, &message, sizeof(message));
}
