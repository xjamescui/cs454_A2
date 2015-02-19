#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <sstream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "StringServer.h"
#include "Message.h"
using namespace std;

/**
 * Read a message from a client
 */
string readMessage(int client_socketd) {
    char text_size_str[4]; // first 4 bytes
    unsigned int text_size;

    // read text size (first 4 bytes)
    if (read(client_socketd, &text_size_str, 4) < 0) {
        cerr << "ERROR reading from client " << client_socketd << ": " << strerror(errno) << endl;
        return ""; // TODO handle
    }

    text_size = strtol(text_size_str, NULL, 10);

    stringstream ss; // to produe final result as a string
    char text[text_size];
    memset(text, 0, sizeof(text));

    // read the text
    if (read(client_socketd, &text, sizeof(text)) < 0) {
        cerr << "ERROR reading from client " << client_socketd << ": " << strerror(errno) << endl;
        return ""; // TODO handle
    }

    for (unsigned int i = 0; i < text_size; i++) ss << text[i];

    cout << "Received from client " << client_socketd << ": " << ss.str() << endl;
    return ss.str();

} // readMessage

void sendMessage(int client_socketd, const char* msg) {
    if (write(client_socketd, msg, strlen(msg)) < 0) {
        cerr << "ERROR writing to client socket " << client_socketd << ": " << strerror(errno) << endl;
    }
} // sendMessage

void titleCase(string &str) {
    bool titleCasedWord = false; // whether or not a word in the string has been title cased

    // Convert everything to lower case to begin
    for (unsigned int i = 0; i < str.length(); i++) str[i] = tolower(str[i]);

    for (unsigned int i = 0; i < str.length(); i++) {

        if (!isalpha(str[i])) {
            titleCasedWord = false;
            continue;
        }

        if (titleCasedWord == false && isalpha(str[i])) {
            str[i] = toupper(str[i]);
            titleCasedWord = true;
        }
    }
} // titleCase

/**
 * String server and client interaction
 */
void process_client (int client_socketd) {

    string msg = readMessage(client_socketd);
    titleCase(msg);
    sendMessage(client_socketd, msg.c_str());

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

    int socketd, client_socketd; // socket descriptors
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
        /* pid = fork(); */
        /* if(pid < 0) { */
        /*     exit(1); */
        /* } */

        /* if (pid == 0) { */
        process_client(client_socketd);
        /* } */

    } // while
} // main

