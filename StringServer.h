class StringServer {
    public:
        StringServer();
        const static int MSG_SIZE = 256;

        void sendMessage(int client_socketd, char* message);
        std::string readMessage(int client_socketd);

};
