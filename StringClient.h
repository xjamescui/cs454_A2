#include <netinet/in.h>

class StringClient {
  private:
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int port;

  public:
    StringClient(hostent *server, int port);
    void connectOrDie(int socketd);
    int sendMessage(int socketd, std::string message);
};
