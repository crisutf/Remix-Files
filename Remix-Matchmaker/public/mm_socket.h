struct MMHandler : seasocks::WebSocket::Handler
{
private:
    seasocks::Server* _server;
    std::unordered_set<seasocks::WebSocket*> connections;

public:
    explicit MMHandler(seasocks::Server* server) : _server(server) {}
private:

    void dropSocket(seasocks::WebSocket* socket);
    void closeSocket(seasocks::WebSocket* socket);
    void onConnect(seasocks::WebSocket* socket) override;
    void onData(seasocks::WebSocket* socket, const char* data) override;
    void onDisconnect(seasocks::WebSocket* socket) override;
};