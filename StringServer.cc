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
    if (str.empty()) return;

    bool titleCasedWord = false; // whether or not a word in the string has been title cased

    // Convert everything to lower case to begin
    for (unsigned int i = 0; i < str.length(); i++) str[i] = tolower(str[i]);

    for (unsigned int i = 0; i < str.length(); i++) {

        if (isspace(str[i])) {
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
        cout << "ERROR getsockname for socket=" << socketd << endl;
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


int main(int argc, char *argv[] ) {

    int server_socketd, client_socketd; // socket descriptors
    struct sockaddr_in server_addr; // address of the server
    struct sockaddr_in client_addr; // address of the connected client
    unsigned int client_addr_len;
    fd_set readfds;
    int select_rv; // return value from select call

    // setup server end socket
    server_socketd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socketd < 0) {
        cout << "ERROR opening socket" << endl;
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

    // keep server as an ongoing running service
    while (1) {

        readfds = server->getActiveFds();
        select_rv = select(server->getHighestFds() + 1, &readfds, NULL, NULL, NULL);

        if (select_rv == -1) {
            cout << "ERROR on select(): " << strerror(errno) << endl;
            exit(EXIT_FAILURE);
        }

        // iterate through all sockets in the set to see which ones are ready
        for (int connection = 0; connection <= server->getHighestFds(); connection++) {

            if (!FD_ISSET(connection, &readfds)) continue;

            // connection is in readfds

            // connection is the server socket
            if (connection == server_socketd) {

                client_socketd = accept(server_socketd, (struct sockaddr*) &client_addr, &client_addr_len);
                if (client_socketd < 0) {
                    cout << "ERROR on accepting client: " << strerror(errno) << endl;
                } else {
                    server->addToActiveFds(client_socketd); // add new connection to set
                    if (client_socketd > server->getHighestFds()) server->setHighestFds(client_socketd);
                }
            } else {
                // connection is a client socket
                string msg;
                if(server->readMessage(connection, msg) == 0) continue; // client disconnected
                titleCase(msg);
                server->sendMessage(connection, (char *)msg.c_str());
            }

        } // for
    } // while
    delete server;
} // main


void StringServer::closeClient(int client_socketd) {
    FD_CLR(client_socketd, &this->active_fds);
    close(client_socketd);
} // closeClient


/**
 * Get the server online
 */
void StringServer::connectOrDie() {
    // bind to host
    if (bind(this->server_socketd, (struct sockaddr *) & (this->server_addr), sizeof(this->server_addr)) < 0) {
        // binding failed
        cout << "ERROR while binding: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    } // if
} // connectOrDie


/**
 * Pack a 4 byte integer in to a message char buffer
 * extracted and modified from: http://beej.us/guide/bgnet/examples/pack2.c
 */
void packInt(unsigned char *msg, unsigned long int i) {
    *msg++ = i>>24;
    *msg++ = i>>16;
    *msg++ = i>>8;
    *msg++ = i;
}

/**
 * Unpack a 4 byte integer from the beginning of a message char buffer
 * extracted and modified from: http://beej.us/guide/bgnet/examples/pack2.c
 */
long int unpackInt(unsigned char *msg) {
    // recreate the integer by bitwise or the first 4 bytes after applying corresponding bit shifts
    unsigned long int unsigned_int = ((unsigned long int) msg[0]<<24)
                                     | ((unsigned long int)msg[1]<<16)
                                     | ((unsigned long int)msg[2]<<8)
                                     | msg[3];

    long int i;

    // change from unsigned to signed
    if (unsigned_int <= 0x7fffffffu) i = unsigned_int;
    else {
        i = -1 - (long int)(0xffffffffu - unsigned_int);
    }

    return i;
}


/**
 * Read a message from a client
 * return:
 *   -1 if error
 *   0 if client disconnected
 *   else good
 */
int StringServer::readMessage(int client_socketd, string &msg) {
    char text_size_str[4]; // first 4 bytes
    unsigned long int text_size;
    unsigned int total_text_recv = 0; // total bytes received
    int text_recv = 0; // bytes received in one recv()
    int text_left = 0; // bytes that have not been received yet
    stringstream ss; // to produe final result as a string

    // read text size (first 4 bytes)
    int result = recv(client_socketd, &text_size_str, 4, 0);
    if (result == 0) {
        this->closeClient(client_socketd);
        return result;
    }
    if (result < 0) {
        cout << "ERROR reading from client " << client_socketd << ": " << strerror(errno) << endl;
        return result;
    }

    text_size = unpackInt((unsigned char*)text_size_str);
    text_left = text_size;
    char text[text_size];
    memset(text, 0, sizeof(text));

    // read the text
    while (total_text_recv < text_size) {

        text_recv = recv(client_socketd, &text[total_text_recv], text_left, 0);
        if (text_recv == 0) {
            this->closeClient(client_socketd);
            return text_recv;
        }

        if (text_recv < 0) {
            cout << "ERROR reading from client " << client_socketd << ": " << strerror(errno) << endl;
            return text_recv;
        }

        total_text_recv += text_recv;
        text_left -= text_recv;

    } // while

    for (unsigned int i = 0; i < text_size; i++) ss << text[i];
    msg = ss.str();

    cout << msg << endl; // print out the string received from client
    return 1;
} // readMessage



/**
 * Send a message to a client
 */
void StringServer::sendMessage(int client_socketd, char* response_msg) {
    // calculate various length values
    unsigned long int text_size = strlen(response_msg) + 1;
    unsigned long int msg_size = 4 + text_size; // integer(4 bytes)+ text length, note that no space between

    int bytes_out = 0; // bytes sent out in one send() call
    unsigned long int total_bytes_out= 0; // total bytes sent out from send() so far
    int bytes_left = msg_size; // bytes remaining that are not sent()

    // prepare messsage (text length followed by text)
    char *msg = new char[msg_size]();
    packInt((unsigned char *)msg, text_size); // pack first 4 bytes
    strcpy(msg + 4, response_msg); // message to follow after first 4 bytes

    // sending the message
    while (total_bytes_out < msg_size) {

        bytes_out = send(client_socketd, msg + total_bytes_out, bytes_left, 0);
        if (bytes_out < 0) {
            cout << "ERROR writing to client socket " << client_socketd << ": " << strerror(errno) << endl;
            break;
        }
        total_bytes_out += bytes_out;
        bytes_left -= bytes_out;
    }

    delete msg;
} // sendMessage
