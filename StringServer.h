class StringServer {
private:
    int server_socketd;
    struct sockaddr_in server_addr;
    fd_set active_fds;
    int highest_fds;

public:
    StringServer(int server_socketd, struct sockaddr_in server_addr) : server_socketd(server_socketd), server_addr(server_addr) {
        FD_ZERO(&this->active_fds);
        FD_SET(server_socketd, &this->active_fds);
        this->highest_fds = server_socketd;
    };

    void connectOrDie();
    void sendMessage(int client_socketd, char* response_msg);
    int readMessage(int client_socketd, std::string &msg);
    void closeClient(int client_socketd);

    fd_set getActiveFds() {
        return this->active_fds;
    }

    void addToActiveFds(int client_socketd) {
        FD_SET(client_socketd, &this->active_fds);
    }

    int getHighestFds() {
        return this->highest_fds;
    }

    void setHighestFds(int client_socketd) {
        this->highest_fds = client_socketd;
    }

};
