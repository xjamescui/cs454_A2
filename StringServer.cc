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
using namespace std;

StringServer* server; // the server

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
 *  get the port number the server is running on (called after server is up)
 */
int getPort(int socketd, struct sockaddr_in server_addr) {

    unsigned int len = sizeof(server_addr);
    if (getsockname(socketd, (struct sockaddr*)&server_addr, &len) == -1) {
        cerr << "ERROR getsockname for socket=" << socketd << endl;
        exit(1);
    }
    return ntohs(server_addr.sin_port);
} // getPort


/**
 *  get the hostname of the server (called after server is up)
 */
string getHostname() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    string hostname_string(hostname);
    return hostname_string;
} // getHostname

void StringServer::connectOrDie() {
    // bind to host
    if (bind(this->server_socketd, (struct sockaddr *) & (this->server_addr), sizeof(this->server_addr)) < 0) {
        // binding failed
        cerr << "ERROR while binding: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    } // if
}
int main(int argc, char *argv[] ) {

    int server_socketd, client_socketd; // socket descriptors
    struct sockaddr_in server_addr; // address of the server
    struct sockaddr_in client_addr; // address of the connected client
    unsigned int client_addr_len;
    fd_set readfds, activefds;
    int select_rv; // return value from select call
    int highest_fds; // highest socket descriptor in the set


    // setup server end socket
    server_socketd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socketd < 0) {
        cerr << "ERROR opening socket" << endl;
        exit(1);
    } // if

    // init socket
    memset(&server_addr, 0, sizeof(server_addr)); // zero out
    server_addr.sin_family = AF_INET; // the address family
    server_addr.sin_addr.s_addr = INADDR_ANY; // set host IP address
    server_addr.sin_port = 0; // random available port

    server = new StringServer(server_socketd, server_addr);
    server->connectOrDie();

    // listen for clients
    listen(server_socketd, 5);   // allow up to 5 connections waiting in queue

    cout << "SERVER_ADDRESS " << getHostname() << endl;
    cout << "SERVER_PORT " << getPort(server_socketd, server_addr) << endl;

    client_addr_len = sizeof(client_addr);

    FD_ZERO(&activefds);
    FD_SET(server_socketd, &activefds);
    highest_fds = server_socketd;

    // keep server as an ongoing running service
    while (1) {

        readfds = activefds;
        select_rv = select(highest_fds + 1, &readfds, NULL, NULL, NULL);

        if (select_rv == -1) {
            cerr << "ERROR on select(): " << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }

        // iterate through all sockets in the set to see which ones are ready
        for (int connection = 0; connection <= highest_fds; connection++) {

            if (!FD_ISSET(connection, &readfds)) continue;

            // connection is in readfds

            // connection is the server socket
            if (connection == server_socketd) {

                client_socketd = accept(server_socketd, (struct sockaddr*) &client_addr, &client_addr_len);
                cout << client_socketd << " has connected" << endl; // TODO remove after
                if (client_socketd < 0) {
                    cerr << "ERROR on accepting client: " << strerror(errno) << endl;
                } else {
                    FD_SET(client_socketd, &activefds);  // add new connection to set
                    if (client_socketd > highest_fds) highest_fds = client_socketd;
                }
            } else {
                // connection is a client socket
                string msg = server->readMessage(connection);
                titleCase(msg);
                server->sendMessage(connection, (char *)msg.c_str());
            }

        } // for
    } // while
} // main


/**
 * Read a message from a client
 */
string StringServer::readMessage(int client_socketd) {
    char text_size_str[4]; // first 4 bytes
    unsigned int text_size;
    stringstream ss; // to produe final result as a string

    // read text size (first 4 bytes)
    if (read(client_socketd, &text_size_str, 4) < 0) {
        cerr << "ERROR reading from client " << client_socketd << ": " << strerror(errno) << endl;
        return ""; // TODO handle
    }

    text_size = strtol(text_size_str, NULL, 10);
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




/**
 * Send a message to a client
 */
void StringServer::sendMessage(int client_socketd, char* response_msg) {
    // calculate various length values
    int string_length = strlen(response_msg) + 1;
    char string_length_char[4];
    sprintf(string_length_char, "%d", string_length);

    int msg_size = 4 + string_length; // integer(4 bytes)+ text length, note that no space between

    // prepare messsage (text length followed by text)
    char *msg = new char[msg_size]();
    strncpy(msg, string_length_char, 4);
    strcpy(msg + 4, response_msg);

    // sending the message
    if (write(client_socketd, msg, msg_size) < 0) {
        cerr << "ERROR writing to client socket " << client_socketd << ": " << strerror(errno) << endl;
    }

    delete msg;
} // sendMessage
