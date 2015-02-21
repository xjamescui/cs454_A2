class StringServer {
    private:
        int server_socketd;
        struct sockaddr_in server_addr;

    public:
        StringServer(int server_socketd, struct sockaddr_in server_addr) : server_socketd(server_socketd), server_addr(server_addr){};

        void connectOrDie();
        void sendMessage(int client_socketd, char* response_msg);
        std::string readMessage(int client_socketd);

};
