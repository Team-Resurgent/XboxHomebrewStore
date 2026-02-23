#pragma once

#include "Main.h"

#define RECV_SOCKET_BUFFER_SIZE_IN_K 64
#define RECV_SOCKET_BUFFER_SIZE RECV_SOCKET_BUFFER_SIZE_IN_K * 1024
#define SEND_SOCKET_BUFFER_SIZE_IN_K 64
#define SEND_SOCKET_BUFFER_SIZE SEND_SOCKET_BUFFER_SIZE_IN_K * 1024

class SocketUtility {
  public:
    static bool CreateSocket(int af, int type, int protocol, uint64_t& result);
    static uint64_t CreateSocket(sockaddr_in sockaddr_in, bool allow_reuse);
    static bool ConnectSocket(uint64_t socket, sockaddr_in* socket_addr_in);
    static bool ConnectSocket(uint64_t socket, sockaddr* socket_addr);
    static bool AcceptSocket(uint64_t socket, uint64_t& result);
    static bool AcceptSocket(uint64_t socket, sockaddr_in* socket_addr_in, uint64_t& result);
    static bool AcceptSocket(uint64_t socket, sockaddr* socket_addr, uint64_t& result);
    static bool SetSocketRecvSize(uint64_t socket, uint32_t& recv_size);
    static bool SetSocketSendSize(uint64_t socket, uint32_t& send_size);
    static bool GetReadQueueLength(uint64_t socket, int& queue_length);
    static bool BindSocket(uint64_t socket, sockaddr_in* socket_addr_in);
    static bool BindSocket(uint64_t socket, sockaddr* socket_addr);
    static bool ListenSocket(uint64_t socket, int count);
    static bool CloseSocket(uint64_t& socket);
    static bool GetSocketName(uint64_t socket, sockaddr_in* socket_addr_in);
    static bool GetSocketName(uint64_t socket, sockaddr* socket_addr);

    static int GetAvailableDataSize(const uint64_t socket);
    static int ReceiveSocketData(uint64_t socket, char* buffer, int size);
    static int SendSocketData(uint64_t socket, const char* buffer, int size);
    static int GetReadStatus(const uint64_t socket);
    static int EndBrokerSocket(uint64_t socket);
};
