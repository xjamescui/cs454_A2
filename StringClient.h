#include <list>

class StringClient {
    private:
        struct sockaddr_in serv_addr;
        int socketd;
        int port;
        std::list<std::string> outgoingMessages;

    public:
        StringClient(hostent *server, int port);
        void connectOrDie(int socketd);
        void sendMessage(int socketd, char* msg);
        std::string readMessage(int socketd);
        int queueCount();
        void enqueueMessage(std::string msg);
        std::string nextMessage(); // read next message
        void dequeueMessage(); // dequeues next message
};
