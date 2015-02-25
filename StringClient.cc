#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include "StringClient.h"

using namespace std;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // used for mutex purposes on threads
StringClient *client; // the client
bool eof = false; // whether or not EOF is reached


/**
 * function performed by thread handling user input
 */
void *UserInput(void *args) {

    string line, reply; // input

    while(getline(cin, line)) {
        // add to the list of data we need to send to server
        pthread_mutex_lock(&mutex);
        if (!line.empty()) client->enqueueMessage(line);
        pthread_mutex_unlock(&mutex);
    } // while

    eof = true;
} // UserInput



/**
 * function performed by thread handling backend (req/reply from/to server)
 */
void *ServerInteraction(void *socket) {

    string msg, reply;
    int socketd = (int)*((int*)socket);

    while (1) {
        while (client->queueCount() > 0) {

            if (eof) break;

            // send client message in queue
            msg = client->nextMessage();

            pthread_mutex_lock(&mutex);
            client->dequeueMessage();
            pthread_mutex_unlock(&mutex);

            client->sendMessage(socketd, (char*)msg.c_str());
            client->readMessage(socketd, reply);

            cout << "Server: " << reply << endl;

            sleep(2); // 2 second delay between successive requests
        } // while

        if (eof) break;
    } // while

    close(socketd);
} // ServerInteraction



int main(int argc, char *argv[]) {

    int socketd, port;
    struct hostent *server;
    pthread_t stdin_thread, backend_thread;

    // create socket
    socketd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketd < 0) {
        cout << "ERROR creating a socket on client side, socket is " << socketd << endl;
        exit(1);
    }

    if (getenv("SERVER_ADDRESS") == NULL || getenv("SERVER_PORT") == NULL) {
        cout << "Please ensure SERVER_ADDRESS and SERVER_PORT environment variables are set" << endl;
        exit(1);
    }

    string host_name = getenv("SERVER_ADDRESS");
    port = atoi(getenv("SERVER_PORT"));

    server = gethostbyname(host_name.c_str());

    if (server == NULL) {
        cout << "ERROR getting the server with name " << host_name << endl;
        exit(0);
    }

    // establish client and connection
    client = new StringClient(server, port);
    client->connectOrDie(socketd);

    // connected!

    // create stdin_thread to handle user input
    if (pthread_create(&stdin_thread, NULL, UserInput, NULL)) {
        cout << "ERROR creating thread to handle user input" << endl;
        exit(EXIT_FAILURE);
    }

    // create backend_thread to handle request sending to server
    if (pthread_create(&backend_thread, NULL, ServerInteraction, (void *)&socketd)) {
        cout << "ERROR creating thread to handle backend" << endl;
        exit(EXIT_FAILURE);
    }

    pthread_exit(NULL);
    pthread_mutex_destroy(&mutex);
    delete client;
} // main



StringClient::StringClient(hostent *server, int port) : port(port) {

    memset(&(this->serv_addr), 0, sizeof(this->serv_addr));
    this->serv_addr.sin_family = AF_INET;
    memcpy(&this->serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    this->serv_addr.sin_port = htons(this->port);
} // constructor



void StringClient::connectOrDie(int socketd) {
    if(connect(socketd, (struct sockaddr*)&(this->serv_addr), sizeof(this->serv_addr)) < 0 ) {
        cout << "ERROR connecting to socket " << socketd << " : " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
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
    unsigned long int unsigned_int = ((unsigned long int) msg[0]<<24)
                                     | ((unsigned long int)msg[1]<<16)
                                     | ((unsigned long int)msg[2]<<8)
                                     | msg[3];

    long int i;

    // change unsigned numbers to signed
    if (unsigned_int <= 0x7fffffffu) i = unsigned_int;
    else {
        i = -1 - (long int)(0xffffffffu - unsigned_int);
    }

    return i;
}

/**
 * Send a message to the server through socket identified by socketd
 */
void StringClient::sendMessage(int socketd, char* client_msg) {

    // calculate various length values
    unsigned long int text_size = strlen(client_msg) + 1;
    unsigned long int msg_size = 4 + text_size; // integer(4 bytes)+ text length, note that no space between

    int bytes_out = 0; // bytes sent out in one send() call
    unsigned long int total_bytes_out= 0; // total bytes sent out from send() so far
    int bytes_left = msg_size; // bytes remaining that are not sent()


    // prepare messsage (text length followed by text)
    char *msg = new char[msg_size]();
    packInt((unsigned char *)msg, text_size); // pack first 4 bytes
    strcpy(msg + 4, client_msg); // message to follow after first 4 bytes

    // sending the message
    while (total_bytes_out < msg_size) {

        bytes_out = send(socketd, msg + total_bytes_out, bytes_left, 0);
        if (bytes_out < 0) {
            cout << "ERROR writing to socket " << socketd << ": " << strerror(errno) << endl;
            break;
        }
        total_bytes_out += bytes_out;
        bytes_left -= bytes_out;
    }

    delete msg;
} // sendMessage



/**
 * Get message/reply from server through socket identified by socketd
 */
void StringClient::readMessage(int socketd, string &reply) {
    char text_size_str[4]; // first 4 bytes
    unsigned long int text_size; // integer value of the first 4 bytes
    unsigned int total_text_recv = 0; // total bytes received
    int text_recv = 0; // bytes received in one recv()
    int text_left = 0; // bytes that have not been received yet
    stringstream ss; // to produe final result as a string

    // read text size (first 4 bytes)
    if (recv(socketd, &text_size_str, 4, 0) < 0) {
        cout << "ERROR reading from server socket" << socketd << ": " << strerror(errno) << endl;
        return;
    }

    text_size = unpackInt((unsigned char*)text_size_str);
    text_left = text_size;
    char text[text_size];
    memset(text, 0, sizeof(text));

    // read the text
    while (total_text_recv < text_size) {

        text_recv = recv(socketd, &text[total_text_recv], text_left, 0);
        if (text_recv < 0) {
            cout << "ERROR reading from socket " << socketd << ": " << strerror(errno) << endl;
            return;
        } // if

        total_text_recv += text_recv;
        text_left -= text_recv;

    } // while

    for (unsigned int i = 0; i < text_size; i++) ss << text[i];

    reply = ss.str();
} // getMessage




void StringClient::enqueueMessage(string msg) {
    this->outgoingMessages.push_back(msg);
}


string StringClient::nextMessage() {
    return this->outgoingMessages.front();
}

void StringClient::dequeueMessage() {
    this->outgoingMessages.pop_front();
}

int StringClient::queueCount() {
    return this->outgoingMessages.size();
}
