#pragma once

#include "SocketUtility.h"
#include "Main.h"
#include "FileSystem.h"

class FtpServer {
  public:
    typedef enum _ReceiveStatus {
        ReceiveStatus_OK = 1,
        ReceiveStatus_Network_Error,
        ReceiveStatus_Timeout,
        ReceiveStatus_Invalid_Data,
        ReceiveStatus_Insufficient_Buffer
    } ReceiveStatus;

    static bool WINAPI ConnectionThread(uint64_t pParam);

    static bool WINAPI ListenThread(LPVOID lParam);

    static bool Init();
    static void Close();
    static bool SocketSendString(uint64_t s, const char* format, ...);
    static ReceiveStatus SocketReceiveString(uint64_t s, char* psz, uint32_t dwMaxChars, uint32_t* pdwCharsReceived);
    static ReceiveStatus SocketReceiveLetter(uint64_t s, char* pch, uint32_t dwMaxChars, uint32_t* pdwCharsReceived);
    static ReceiveStatus SocketReceiveData(uint64_t s, char* psz, uint32_t dwBytesToRead, uint32_t* pdwBytesRead);
    static uint64_t EstablishDataConnection(sockaddr_in* psaiData, uint64_t* psPasv);
    static bool ReceiveSocketFile(uint64_t sCmd, uint64_t sData, uint32_t fileHandle);
    static bool SendSocketFile(uint64_t sCmd, uint64_t sData, uint32_t fileHandle, uint32_t* pdwAbortFlag);
};
