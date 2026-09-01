struct GSHandler : seasocks::WebSocket::Handler
{
public:
    void onConnect(seasocks::WebSocket* socket) override;
    void onData(seasocks::WebSocket* socket, const char* data) override;
    void onDisconnect(seasocks::WebSocket* socket) override;
};
