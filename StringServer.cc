#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
using namespace std;

/**
 * String server and client interaction
 */
void process_client (int client_socketd) {

    int amount; // amount of work doen during read/write

    // read client message


    // reply to client
    amount = write(client_socketd, "Hello client", 18);

    if (amount < 0){
        // write is not successful
        cerr << "ERROR when writing to client socket" << endl;
        exit(1);
    }
} // process_client

int getPort(int socketd, struct sockaddr_in server_addr) {

    unsigned int len = sizeof(server_addr);
    if(getsockname(socketd, (struct sockaddr*)&server_addr, &len) == -1){
        cerr << "ERROR getsockname for socket=" << socketd << endl;
        exit(1);
    }
    return ntohs(server_addr.sin_port);
}

string getHostname(){
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    string hostname_string(hostname);
    return hostname_string;
}

int main(int argc, char *argv[] ){

    int socketd, client_socketd; // socket descriptor
    int pid;  // used for handing multiple clients using multi-threading later
    struct sockaddr_in server_addr; // address of the server
    struct sockaddr_in client_addr; // address of the connected client
    unsigned int client_addr_len;

    socketd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketd < 0) {
        cerr << "ERROR opening socket" << endl;
        exit(1);
    } // if

    // init socket
    memset(&server_addr, 0, sizeof(server_addr)); // zero out 
    server_addr.sin_family = AF_INET; // the address family
    server_addr.sin_addr.s_addr = INADDR_ANY; // set host IP address
    server_addr.sin_port = 0; // random available port

    // bind to host
    if (bind(socketd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        // binding failed
        cerr << "ERROR while binding: " << strerror(errno) << endl;
        exit(1);
    } // if


    // listen for clients
    listen(socketd, 5);   // allow up to 5 connections waiting in queue

    cout << "SERVER_ADDRESS " << getHostname() << endl;
    cout << "SERVER_PORT " << getPort(socketd, server_addr) << endl; 

    client_addr_len = sizeof(client_addr);
    // keep server as an ongoing running service
    while (1) {

        client_socketd = accept(socketd, (struct sockaddr*) &client_addr, &client_addr_len);

        if (client_socketd < 0){
            cerr << "ERROR on accepting client: " << strerror(errno) << endl;
            exit(1);
        }

        cout << client_socketd << " has connected" << endl;
        pid = fork();
        if(pid < 0) {
            exit(1);
        }

        if (pid == 0) {
            process_client(client_socketd);
        }

    } // while
} // main

