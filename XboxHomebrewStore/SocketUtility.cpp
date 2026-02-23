#include "SocketUtility.h"
#include "Debug.h"

bool SocketUtility::CreateSocket(int af, int type, int protocol, uint64_t& result) {
    result = socket(af, type, protocol);
    if (result < 0) {
        Debug::Print("Error: Create socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

uint64_t SocketUtility::CreateSocket(sockaddr_in sockaddr_in, bool allow_reuse) {
    const uint64_t listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == INVALID_SOCKET) {
        Debug::Print("Error: Create socket failed: %i", WSAGetLastError());
        return NULL;
    }

    int result;

    unsigned long allowLocalAddressReuse = 1;
    result = setsockopt((SOCKET)listen_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&allowLocalAddressReuse,
        sizeof(allowLocalAddressReuse));
    if (result != 0) {
        Debug::Print("Error: Set socket option SO_REUSEADDR failed: %i", WSAGetLastError());
        return NULL;
    }

    unsigned long nonBlocking = 1;
    if (ioctlsocket((SOCKET)listen_socket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        Debug::Print("Error: IO control socket non-blocking failed: %i", WSAGetLastError());
        return NULL;
    }

    result = bind((SOCKET)listen_socket, (sockaddr*)&sockaddr_in, sizeof(sockaddr_in));
    if (result == SOCKET_ERROR) {
        Debug::Print("Error: Socket bind failed: %i", WSAGetLastError());
        return NULL;
    }

    result = listen((SOCKET)listen_socket, 10);
    if (result == SOCKET_ERROR) {
        Debug::Print("Error: Socket listen failed: %i", WSAGetLastError());
        return NULL;
    }

    return listen_socket;
}

bool SocketUtility::ConnectSocket(uint64_t socket, sockaddr_in* socket_addr_in) {
    const int result = connect((SOCKET)socket, (sockaddr*)socket_addr_in, sizeof(SOCKADDR_IN));
    if (result < 0) {
        Debug::Print("Error: Connect socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::ConnectSocket(uint64_t socket, sockaddr* socket_addr) {
    const int result = connect((SOCKET)socket, socket_addr, sizeof(sockaddr));
    if (result < 0) {
        Debug::Print("Error: Connect socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::AcceptSocket(uint64_t socket, uint64_t& result) {
    result = accept((SOCKET)socket, NULL, 0);
    if (result < 0) {
        Debug::Print("Error: Accept socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::AcceptSocket(uint64_t socket, sockaddr_in* socket_addr_in, uint64_t& result) {
    int dw = sizeof(sockaddr_in);
    result = accept((SOCKET)socket, (sockaddr*)socket_addr_in, &dw);
    if (result < 0) {
        Debug::Print("Error: Accept socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::AcceptSocket(uint64_t socket, sockaddr* socket_addr, uint64_t& result) {
    int dw = sizeof(sockaddr);
    result = accept((SOCKET)socket, socket_addr, &dw);
    if (result < 0) {
        Debug::Print("Error: Accept socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::SetSocketRecvSize(uint64_t socket, uint32_t& recv_size) {
    uint32_t recvBufferSize = RECV_SOCKET_BUFFER_SIZE;
    int result = setsockopt((SOCKET)socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufferSize, sizeof(uint32_t));
    if (result < 0) {
        Debug::Print("Error: Set socket option SO_RCVBUF failed: %i", WSAGetLastError());
    }

    int isize = sizeof(recvBufferSize);
    result = getsockopt((SOCKET)socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufferSize, (int*)&isize);
    if (result < 0) {
        recvBufferSize = 8192;
    }

    recv_size = recvBufferSize;
    return true;
}

bool SocketUtility::SetSocketSendSize(uint64_t socket, uint32_t& send_size) {
    uint32_t sendBufferSize = SEND_SOCKET_BUFFER_SIZE;
    int result = setsockopt((SOCKET)socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferSize, sizeof(uint32_t));
    if (result < 0) {
        Debug::Print("Error: Set socket option SO_SNDBUF failed: %i", WSAGetLastError());
    }

    int isize = sizeof(sendBufferSize);
    result = getsockopt((SOCKET)socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferSize, (int*)&isize);
    if (result < 0) {
        sendBufferSize = 8192;
    }

    send_size = sendBufferSize;
    return true;
}

bool SocketUtility::GetReadQueueLength(uint64_t socket, int& queue_length) {
    DWORD temp;
    int result = ioctlsocket((SOCKET)socket, FIONREAD, &temp);
    if (result < 0) {
        Debug::Print("Error: Get read queue length failed: %i", WSAGetLastError());
        queue_length = 0;
        return false;
    }
    queue_length = temp;
    return true;
}

bool SocketUtility::BindSocket(uint64_t socket, sockaddr_in* socket_addr_in) {
    int result = bind((SOCKET)socket, (sockaddr*)socket_addr_in, sizeof(sockaddr_in));
    if (result < 0) {
        Debug::Print("Error: Bind socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::BindSocket(uint64_t socket, sockaddr* socket_addr) {
    int result = bind((SOCKET)socket, socket_addr, sizeof(sockaddr));
    if (result < 0) {
        Debug::Print("Error: Bind socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::ListenSocket(uint64_t socket, int count) {
    int result = listen((SOCKET)socket, count);
    if (result < 0) {
        Debug::Print("Error: Listen socket failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::CloseSocket(uint64_t& socket) {
    if (!socket) {
        return true;
    }
    int result = closesocket((SOCKET)socket);
    if (result < 0) {
        Debug::Print("Error: Close socket failed: %i", WSAGetLastError());
        socket = 0;
        return false;
    }
    socket = 0;
    return true;
}

bool SocketUtility::GetSocketName(uint64_t socket, sockaddr_in* socket_addr_in) {
    int size = sizeof(sockaddr_in);
    int result = getsockname((SOCKET)socket, (sockaddr*)socket_addr_in, &size);
    if (result < 0) {
        Debug::Print("Error: Get socket name failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

bool SocketUtility::GetSocketName(uint64_t socket, sockaddr* socket_addr) {
    int size = sizeof(sockaddr);
    const int result = getsockname((SOCKET)socket, socket_addr, &size);
    if (result < 0) {
        Debug::Print("Error: Get socket name failed: %i", WSAGetLastError());
        return false;
    }
    return true;
}

int SocketUtility::GetAvailableDataSize(const uint64_t socket) {
    unsigned long available_data = 0;
    ioctlsocket((SOCKET)socket, FIONREAD, &available_data);
    return (int)available_data;
}

int SocketUtility::ReceiveSocketData(const uint64_t socket, char* buffer, const int size) {
    const int bytes_received = recv((SOCKET)socket, buffer, size, 0);
    if (bytes_received == SOCKET_ERROR) {
        Debug::Print("Error: Socket receive failed: %i\n", WSAGetLastError());
    }
    return bytes_received;
}

int SocketUtility::SendSocketData(const uint64_t socket, const char* buffer, const int size) {
    const int bytes_sent = send((SOCKET)socket, buffer, size, 0);
    if (bytes_sent == SOCKET_ERROR) {
        Debug::Print("Error: Socket send failed: %i\n", WSAGetLastError());
    }
    return bytes_sent;
}

int SocketUtility::GetReadStatus(const uint64_t socket) {
    static const timeval instantSpeedPlease = {0, 0};
    fd_set a = {1, {(SOCKET)socket}};

    int result = select(0, &a, 0, 0, &instantSpeedPlease);
    if (result == SOCKET_ERROR) {
        result = WSAGetLastError();
    }

    if (result != 0 && result != 1) {
        Debug::Print("Error: getReadStatus failed: %i\n", result);
        return SOCKET_ERROR;
    }
    return result;
}

int SocketUtility::EndBrokerSocket(uint64_t socket) {
    int result = shutdown((SOCKET)socket, SD_BOTH);
    if (result != 0) {
        Debug::Print("Error: Socket shutdown failed: %i", WSAGetLastError());
    }
    result = closesocket((SOCKET)socket);
    if (result != 0) {
        Debug::Print("Error: Close socket failed: %i", WSAGetLastError());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
