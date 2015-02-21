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


/**
 * function performed by thread handling user input
 */
void *UserInput(void *args) {

    string line, reply; // input

    while(getline(cin, line)) {

        // add to the list of data we need to send to server
        pthread_mutex_lock(&mutex);
        client->enqueueMessage(line);
        pthread_mutex_unlock(&mutex);

    } // while
} // UserInput




/**
 * function performed by thread handling backend (req/reply from/to server)
 */
void *ServerInteraction(void *socket) {

    string msg, reply;
    int socketd = (int)*((int*)socket);

    while (1) {
        while (client->queueCount() > 0) {

            // send client message in queue
            msg = client->nextMessage();

            pthread_mutex_lock(&mutex);
            client->dequeueMessage();
            pthread_mutex_unlock(&mutex);

            client->sendMessage(socketd, (char*)msg.c_str());
            reply = client->readMessage(socketd);

            cout << "Server: " << reply << endl;

            sleep(2); // 2 second delay between successive requests
        } // while
    } // while
} // ServerInteraction



int main(int argc, char *argv[]) {

    int socketd, port;
    struct hostent *server;

    // create socket
    socketd = socket(AF_INET, SOCK_STREAM, 0);

    if (socketd < 0) {
        cerr << "ERROR creating a socket on client side, socket is " << socketd << endl;
        exit(1);
    }

    if (getenv("SERVER_ADDRESS") == NULL || getenv("SERVER_PORT") == NULL) {
        cerr << "Please ensure SERVER_ADDRESS and SERVER_PORT environment variables are set" << endl;
        exit(1);
    }

    string host_name = getenv("SERVER_ADDRESS");
    port = atoi(getenv("SERVER_PORT"));

    server = gethostbyname(host_name.c_str());

    if (server == NULL) {
        cerr << "ERROR getting the server with name " << host_name << endl;
        exit(0);
    }

    // establish client and connection
    client = new StringClient(server, port);
    client->connectOrDie(socketd);

    // connected!

    pthread_t stdin_thread, backend_thread;

    // create stdin_thread to handle user input
    if (pthread_create(&stdin_thread, NULL, UserInput, NULL)) {
        cerr << "ERROR creating thread to handle user input" << endl;
        exit(EXIT_FAILURE);
    }

    // create backend_thread to handle request sending to server
    if (pthread_create(&backend_thread, NULL, ServerInteraction, (void *)&socketd)) {
        cerr << "ERROR creating thread to handle backend" << endl;
        exit(EXIT_FAILURE);
    }

    pthread_exit(NULL);
    pthread_mutex_destroy(&mutex);
} // main




StringClient::StringClient(hostent *server, int port) : port(port) {

    memset(&(this->serv_addr), 0, sizeof(this->serv_addr));
    this->serv_addr.sin_family = AF_INET;
    memcpy(&this->serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    this->serv_addr.sin_port = htons(this->port);
} // constructor




void StringClient::connectOrDie(int socketd) {
    if(connect(socketd, (struct sockaddr*)&(this->serv_addr), sizeof(this->serv_addr)) < 0 ) {
        cerr << "ERROR connecting to socket=" << socketd << " : " << strerror(errno) << endl;
        exit(1);
    }
} // connectOrDie



/**
 * Send a message to the server through socket identified by socketd
 */
void StringClient::sendMessage(int socketd, char* client_msg) {

    // calculate various length values
    int string_length = strlen(client_msg) + 1;
    char string_length_char[4];
    sprintf(string_length_char, "%d", string_length);

    int msg_size = 4 + string_length; // integer(4 bytes)+ text length, note that no space between


    // prepare messsage (text length followed by text)
    char *msg = new char[msg_size]();
    strncpy(msg, string_length_char, 4);
    strcpy(msg + 4, client_msg);

    // sending the message
    if (write(socketd, msg, msg_size) < 0) {
        cerr << "ERROR writing to socket " << socketd << ": " << strerror(errno) << endl;
    }

    delete msg;
} // sendMessage



/**
 * Get message/reply from server through socket identified by socketd
 */
string StringClient::readMessage(int socketd) {
    char text_size_str[4]; // first 4 bytes
    unsigned int text_size;
    stringstream ss; // to produe final result as a string

    // read text size (first 4 bytes)
    if (read(socketd, &text_size_str, 4) < 0) {
        cerr << "ERROR reading from server socket" << socketd << ": " << strerror(errno) << endl;
        return ""; // TODO handle
    }

    text_size = strtol(text_size_str, NULL, 10);
    char text[text_size];
    memset(text, 0, sizeof(text));

    if (read(socketd, &text, sizeof(text)) < 0 ) {
        cerr << "ERROR reading from socket " << socketd << ": " << strerror(errno) << endl;
        return ""; // TODO handle
    } // if

    for (unsigned int i = 0; i < text_size; i++) ss << text[i];

    return ss.str();
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
