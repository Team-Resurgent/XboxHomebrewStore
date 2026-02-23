#include "FtpServer.h"
#include "SocketUtility.h"
#include "FileSystem.h"
#include "DriveMount.h"
#include "Debug.h"
#include "String.h"

#include <cstdlib>

#define SERVERID "Xbox Homebrew Store FTP, welcome..."

namespace {

LONG activeConnections = 0;

int mMaxConnections;
int mCommandTimeout;
int mConnectTimeout;

bool mStopRequested;

uint64_t sListen;
sockaddr_in saiListen;

HANDLE mListenThreadHandle;

int incrementConnections() {
    return (int)_InterlockedIncrement(&activeConnections);
}

void decrementConnections() {
    _InterlockedDecrement(&activeConnections);
}

std::string cleanVirtualPath(const std::string virtualPath) {
    const char* in = virtualPath.c_str();
    char* buf = new char[virtualPath.size() + 4];
    buf[0] = '\0';
    buf[1] = '\0';
    buf[2] = '\0';
    char* out = buf + 3;
    do {
        *out = *in;
        if (*out == '\\')
            *out = '/';
        if ((*out == '\0') || (*out == '/')) {
            if (out[-1] == '.') {
                if (out[-2] == '\0')
                    --out;
                else if (out[-2] == '/') {
                    if (out[-3] == '\0')
                        --out;
                    else
                        out -= 2;
                } else if (out[-2] == '.') {
                    if (out[-3] == '\0')
                        out -= 2;
                    else if (out[-3] == '/') {
                        if (out[-4] == '\0')
                            out -= 2;
                        else {
                            out -= 3;
                            while ((out[-1] != '\0') && (out[-1] != '/'))
                                --out;
                        }
                    }
                } else
                    ++in;
            } else {
                ++in;
                if (out[-1] != '/')
                    ++out;
            }
        } else
            ++in, ++out;
    } while (in[-1] != '\0');

    std::string result(buf + 3);
    delete[] buf;
    return result;
}

std::string resolveRelative(const std::string currentVirtual, const std::string relativeVirtual) {
    if (relativeVirtual.empty() || relativeVirtual[0] != '/') {
        return cleanVirtualPath(String::Format("%s\\%s", currentVirtual, relativeVirtual.c_str()));
    }
    return cleanVirtualPath(relativeVirtual);
}

std::vector<FileInfoDetail> getDirectoryListing(const std::string virtualPath) {
    if (String::EqualsIgnoreCase(virtualPath, "/")) {
        std::vector<FileInfoDetail> fileInfoDetails;
        std::vector<std::string> drives = DriveMount::GetMountedDrives();

        for (size_t i = 0; i < drives.size(); i++) {
            FileInfoDetail fileInfoDetail;
            fileInfoDetail.isDirectory = true;
            fileInfoDetail.isFile = false;
            fileInfoDetail.isVirtual = true;
            fileInfoDetail.path = drives[i];
            fileInfoDetail.accessTime.day = 1;
            fileInfoDetail.accessTime.month = 1;
            fileInfoDetail.accessTime.year = 2000;
            fileInfoDetail.accessTime.minute = 0;
            fileInfoDetail.accessTime.hour = 0;
            fileInfoDetail.accessTime.second = 0;
            fileInfoDetail.writeTime.day = 1;
            fileInfoDetail.writeTime.month = 1;
            fileInfoDetail.writeTime.year = 2000;
            fileInfoDetail.writeTime.minute = 0;
            fileInfoDetail.writeTime.hour = 0;
            fileInfoDetail.writeTime.second = 0;
            fileInfoDetails.push_back(fileInfoDetail);
        }

        return fileInfoDetails;
    }

    if (DriveMount::FtpPathMounted(virtualPath) == false) {
        return std::vector<FileInfoDetail>();
    }

    std::string ftpPath = DriveMount::MapFtpPath(virtualPath);
    std::vector<FileInfoDetail> fileInfoDetails = FileSystem::FileGetFileInfoDetails(ftpPath);

    for (size_t i = 0; i < fileInfoDetails.size(); i++) {
        fileInfoDetails[i].path = FileSystem::GetFileName(fileInfoDetails[i].path);
    }

    return fileInfoDetails;
}
} // namespace

bool WINAPI FtpServer::ConnectionThread(uint64_t sCmd) {
    uint64_t sData = 0;
    uint64_t sPasv = 0;
    char cmdBuffer[512];
    std::string szPeerName;
    std::string szCmd;
    std::string pszParam;
    std::string user;
    std::string currentVirtual;
    std::string rnfr;
    uint32_t dw = 0;
    uint32_t dwRestOffset = 0;
    ReceiveStatus status = ReceiveStatus_OK;
    bool isLoggedIn = false;
    FileTime fileTime;
    UINT_PTR i;

    SOCKADDR_IN saiCmd;
    memset(&saiCmd, 0, sizeof(SOCKADDR_IN));
    SOCKADDR_IN saiCmdPeer;
    memset(&saiCmdPeer, 0, sizeof(SOCKADDR_IN));
    SOCKADDR_IN saiPasv;
    memset(&saiPasv, 0, sizeof(SOCKADDR_IN));
    SOCKADDR_IN saiData;
    memset(&saiData, 0, sizeof(SOCKADDR_IN));

    // Get peer address
    dw = sizeof(SOCKADDR_IN);
    getpeername((SOCKET)sCmd, (SOCKADDR*)&saiCmdPeer, (int*)&dw);
    szPeerName = String::Format("%u.%u.%u.%u", saiCmdPeer.sin_addr.S_un.S_un_b.s_b1, saiCmdPeer.sin_addr.S_un.S_un_b.s_b2,
        saiCmdPeer.sin_addr.S_un.S_un_b.s_b3, saiCmdPeer.sin_addr.S_un.S_un_b.s_b4);

    // Send greeting
    SocketSendString(sCmd, "220-%s\r\n220-You are connecting from %s:%u.\r\n220 Proceed with login.\r\n", SERVERID,
        szPeerName.c_str(), ntohs(saiCmdPeer.sin_port));

    SocketUtility::GetSocketName(sCmd, &saiCmd);

    // Command processing loop
    while (mStopRequested == false) {
        const int result = SocketUtility::GetReadStatus(sCmd);
        if (result != 1) {
            Sleep(50);
            continue;
        }

        status = SocketReceiveString(sCmd, cmdBuffer, 512, &dw);

        if (status == ReceiveStatus_Network_Error) {
            SocketSendString(sCmd, "421 Network error.\r\n");
            break;
        } else if (status == ReceiveStatus_Timeout) {
            SocketSendString(sCmd, "421 Connection timed out.\r\n");
            break;
        } else if (status == ReceiveStatus_Invalid_Data) {
            SocketSendString(sCmd, "500 Malformed request.\r\n");
            continue;
        } else if (status == ReceiveStatus_Insufficient_Buffer) {
            SocketSendString(sCmd, "500 Command line too long.\r\n");
            continue;
        }

        {
            std::string line(cmdBuffer, dw);
            size_t sp = line.find(' ');
            szCmd = (sp != std::string::npos) ? line.substr(0, sp) : line;
            pszParam = (sp != std::string::npos) ? line.substr(sp + 1) : "";
        }

        if (String::EqualsIgnoreCase(szCmd, "USER")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
                continue;
            } else if (isLoggedIn) {
                SocketSendString(sCmd, "503 Already logged in. Use REIN to change users.\r\n");
                continue;
            } else {
                user = pszParam;
                if (String::EqualsIgnoreCase(user, "xbox")) {
                    SocketSendString(sCmd, "331 Need password for user \"%s\".\r\n", user.c_str());
                    continue;
                }
            }
        }

        if (String::EqualsIgnoreCase(szCmd, "PASS")) {
            if (user.empty()) {
                SocketSendString(sCmd, "503 Bad sequence of commands. Send USER first.\r\n");
            } else if (isLoggedIn) {
                SocketSendString(sCmd, "503 Already logged in. Use REIN to change users.\r\n");
            } else {
                if (String::EqualsIgnoreCase(user, "xbox") && String::EqualsIgnoreCase(pszParam, "xbox")) {
                    if (incrementConnections() <= mMaxConnections) {
                        isLoggedIn = true;
                        currentVirtual = "/";
                        SocketSendString(sCmd, "230 User \"%s\" logged in.\r\n", user.c_str());
                    } else {
                        decrementConnections();
                        SocketSendString(sCmd, "421 Your login was refused due to a server connection limit.\r\n");
                        break;
                    }
                } else {
                    SocketSendString(sCmd, "530 Incorrect password.\r\n");
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "REIN")) {
            if (isLoggedIn) {
                isLoggedIn = false;
                decrementConnections();
                SocketSendString(sCmd, "220-User \"%s\" logged out.\r\n", user.c_str());
                user.clear();
            }
            SocketSendString(sCmd, "220 REIN command successful.\r\n");
        }

        else if (String::EqualsIgnoreCase(szCmd, "HELP")) {
            SocketSendString(sCmd, "214 For help, please cry to mom.\r\n");
        }

        else if (String::EqualsIgnoreCase(szCmd, "FEAT")) {
            SocketSendString(
                sCmd, "211-Extensions supported:\r\n SIZE\r\n REST STREAM\r\n MDTM\r\n TVFS\r\n XCRC\r\n211 END\r\n");
        }

        // else if (!stricmp(szCmd, "SYST")) {
        //	sprintf(szOutput, "215 Windows_NT\r\n");
        //	SocketSendString(sCmd, szOutput);
        // }

        else if (String::EqualsIgnoreCase(szCmd, "QUIT")) {
            if (isLoggedIn) {
                isLoggedIn = false;
                decrementConnections();
                SocketSendString(sCmd, "221-User \"%s\" logged out.\r\n", user.c_str());
            }
            SocketSendString(sCmd, "221 Goodbye!\r\n");
            break;
        }

        else if (String::EqualsIgnoreCase(szCmd, "NOOP")) {
            SocketSendString(sCmd, "200 NOOP command successful.\r\n");
        }

        else if (String::EqualsIgnoreCase(szCmd, "PWD") || String::EqualsIgnoreCase(szCmd, "XPWD")) {
            if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                SocketSendString(sCmd, "257 \"%s\" is current directory.\r\n", currentVirtual.c_str());
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "CWD") || String::EqualsIgnoreCase(szCmd, "XCWD")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                bool isFolder = false;
                if (newVirtual == "/") {
                    isFolder = true;
                } else {
                    std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                    if (ftpPath.size() >= 1 && ftpPath[ftpPath.size() - 1] == ':') {
                        isFolder = true;
                    } else {
                        FileSystem::DirectoryExists(ftpPath, isFolder);
                    }
                }

                if (isFolder == true) {
                    currentVirtual = newVirtual;
                    SocketSendString(sCmd, "250 \"%s\" is now current directory.\r\n", currentVirtual.c_str());
                } else {
                    SocketSendString(sCmd, "550 %s failed. \"%s\": directory not found.\r\n", szCmd.c_str(), newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "CDUP") || String::EqualsIgnoreCase(szCmd, "XCUP")) {
            if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                currentVirtual = resolveRelative(currentVirtual, "..");
                SocketSendString(sCmd, "250 \"%s\" is now current directory.\r\n", currentVirtual.c_str());
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "TYPE")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                SocketSendString(sCmd, "200 TYPE command successful.\r\n");
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "REST")) {
            if (pszParam.empty() || (!(dw = atoi(pszParam.c_str())) && (pszParam[0] != '0'))) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                dwRestOffset = dw;
                SocketSendString(sCmd, "350 Ready to resume transfer at %u bytes.\r\n", dwRestOffset);
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "PORT")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                memset(&saiData, 0, sizeof(SOCKADDR_IN));
                saiData.sin_family = AF_INET;

                unsigned short h1 = 0;
                unsigned short h2 = 0;
                unsigned short h3 = 0;
                unsigned short h4 = 0;
                unsigned short p1 = 0;
                unsigned short p2 = 0;

                dw = sscanf(pszParam.c_str(), "%hu,%hu,%hu,%hu,%hu,%hu", &h1, &h2, &h3, &h4, &p1, &p2);

                saiData.sin_addr.S_un.S_un_b.s_b1 = (u_char)h1;
                saiData.sin_addr.S_un.S_un_b.s_b2 = (u_char)h2;
                saiData.sin_addr.S_un.S_un_b.s_b3 = (u_char)h3;
                saiData.sin_addr.S_un.S_un_b.s_b4 = (u_char)h4;
                saiData.sin_port = (p2 << 8) + p1;

                if (dw == 6) {
                    SocketUtility::CloseSocket(sPasv);
                    SocketSendString(sCmd, "200 PORT command successful.\r\n");
                } else {
                    SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
                    memset(&saiData, 0, sizeof(SOCKADDR_IN));
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "PASV")) {
            if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                if (sPasv) {
                    SocketUtility::CloseSocket(sPasv);
                }

                static u_short portTest = 6000;

                XNADDR addr;
                XNetGetTitleXnAddr(&addr);

                bool found = false;
                while (!found) {
                    memset(&saiPasv, 0, sizeof(SOCKADDR_IN));
                    saiPasv.sin_family = AF_INET;
                    saiPasv.sin_addr.s_addr = INADDR_ANY;
                    saiPasv.sin_port = htons(portTest);
                    if (SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sPasv)) {
                        found = true;
                    } else {
                        Debug::Print(
                            "Error: Could not create socket on port %u, trying to find next available.\n", portTest);
                    }
                    portTest++;
                    if (portTest > 6999) {
                        portTest = 6000;
                    }
                }

                SocketUtility::BindSocket(sPasv, &saiPasv);
                SocketUtility::ListenSocket(sPasv, 1);
                SocketUtility::GetSocketName(sPasv, &saiPasv);

                SocketSendString(sCmd, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n", addr.ina.S_un.S_un_b.s_b1,
                    addr.ina.S_un.S_un_b.s_b2, addr.ina.S_un.S_un_b.s_b3, addr.ina.S_un.S_un_b.s_b4,
                    ((unsigned char*)&saiPasv.sin_port)[0], ((unsigned char*)&saiPasv.sin_port)[1]);
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "LIST") || String::EqualsIgnoreCase(szCmd, "NLST")) {
            if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string pathArg = pszParam;
                if (!pathArg.empty() && pathArg[0] == '-') {
                    size_t sp2 = pathArg.find(' ');
                    if (sp2 != std::string::npos)
                        pathArg = pathArg.substr(sp2 + 1);
                }
                std::string newVirtual =
                    pathArg.empty() ? currentVirtual : resolveRelative(currentVirtual, pathArg);

                std::vector<FileInfoDetail> fileInfoDetails = getDirectoryListing(newVirtual);
                if (!fileInfoDetails.empty()) {
                    sData = EstablishDataConnection(&saiData, &sPasv);
                    if (sData) {
                        SocketSendString(sCmd, "150 Opened %s mode data connection for \"%s\".\r\n",
                            sPasv ? "passive" : "active", newVirtual.c_str());
                        const char* months[12] = {
                            "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                        for (size_t i = 0; i < fileInfoDetails.size(); i++) {
                            char szLine[512];
                            const FileInfoDetail& fileInfoDetail = fileInfoDetails[i];
                            if (String::EqualsIgnoreCase(szCmd, "NLST")) {
                                strcpy(szLine, fileInfoDetail.path.c_str());
                            } else {
                                sprintf(szLine, "%c--------- 1 ftp ftp %llu ", fileInfoDetail.isDirectory ? 'd' : '-',
                                    fileInfoDetail.size);
                                strcat(szLine, months[fileInfoDetail.writeTime.month - 1]);
                                sprintf(szLine + strlen(szLine), " %2i %.2i:%.2i ", fileInfoDetail.writeTime.day,
                                    fileInfoDetail.writeTime.hour, fileInfoDetail.writeTime.minute);
                                strcat(szLine, fileInfoDetail.path.c_str());
                            }
                            strcat(szLine, "\r\n");
                            SocketSendString(sData, szLine);
                        }
                        SocketUtility::CloseSocket(sData);
                        SocketSendString(sCmd, "226 %s command successful.\r\n", String::EqualsIgnoreCase(szCmd, "NLST") ? "LIST" : "NLST");
                    } else {
                        SocketSendString(sCmd, "425 Can't open data connection.\r\n");
                    }
                } else {
                    SocketSendString(sCmd, "550 \"%s\": Path not found.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "STAT")) {
            if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string pathArg = pszParam;
                if (!pathArg.empty() && pathArg[0] == '-') {
                    size_t sp2 = pathArg.find(' ');
                    if (sp2 != std::string::npos)
                        pathArg = pathArg.substr(sp2 + 1);
                }
                std::string newVirtual =
                    pathArg.empty() ? currentVirtual : resolveRelative(currentVirtual, pathArg);

                std::vector<FileInfoDetail> fileInfoDetails = getDirectoryListing(newVirtual);
                if (!fileInfoDetails.empty()) {
                    SocketSendString(sCmd, "212-Sending directory listing of \"%s\".\r\n", newVirtual.c_str());
                    const char* months[12] = {
                        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                    for (size_t i = 0; i < fileInfoDetails.size(); i++) {
                        char szLine[512];
                        const FileInfoDetail& fileInfoDetail = fileInfoDetails[i];
                        sprintf(szLine, "%c--------- 1 ftp ftp %llu ", fileInfoDetail.isDirectory ? 'd' : '-',
                            fileInfoDetail.size);
                        strcat(szLine, months[fileInfoDetail.writeTime.month - 1]);
                        sprintf(szLine + strlen(szLine), " %2i %.2i:%.2i ", fileInfoDetail.writeTime.day,
                            fileInfoDetail.writeTime.hour, fileInfoDetail.writeTime.minute);
                        strcat(szLine, fileInfoDetail.path.c_str());
                        if (fileInfoDetail.isDirectory == true) {
                            strcat(szLine, "/");
                        }
                        strcat(szLine, "\r\n");
                        SocketSendString(sCmd, szLine);
                    }
                } else {
                    SocketSendString(sCmd, "550 \"%s\": Path not found.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "RETR")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);

                uint32_t fileHandle;
                if (FileSystem::FileOpen(ftpPath, FileModeRead, fileHandle) == true) {
                    if (dwRestOffset) {
                        FileSystem::FileSeek(fileHandle, FileSeekModeStart, dwRestOffset);
                        dwRestOffset = 0;
                    }
                    sData = EstablishDataConnection(&saiData, &sPasv);
                    if (sData) {
                        SocketSendString(sCmd, "150 Opened %s mode data connection for \"%s\".\r\n",
                            sPasv ? "passive" : "active", newVirtual.c_str());
                        if (SendSocketFile(sCmd, sData, fileHandle, &dw)) {
                            SocketSendString(sCmd, "226 \"%s\" transferred successfully.\r\n", newVirtual.c_str());
                        } else {
                            SocketSendString(sCmd, "426 Connection closed; transfer aborted.\r\n");
                            if (dw) {
                                SocketSendString(sCmd, "226 ABOR command successful.\r\n");
                            }
                        }
                        SocketUtility::CloseSocket(sData);
                    }

                    else {
                        SocketSendString(sCmd, "425 Can't open data connection.\r\n");
                    }
                    FileSystem::FileClose(fileHandle);
                } else {
                    SocketSendString(sCmd, "550 \"%s\": Unable to open file.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "STOR") || String::EqualsIgnoreCase(szCmd, "APPE")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);

                uint32_t fileHandle;
                if (FileSystem::FileOpen(ftpPath, FileModeWriteUpdate, fileHandle) == true) {
                    if (String::EqualsIgnoreCase(szCmd, "APPE")) {
                        FileSystem::FileSeek(fileHandle, FileSeekModeEnd, 0);
                    } else {
                        FileSystem::FileSeek(fileHandle, FileSeekModeStart, dwRestOffset);
                        FileSystem::FileTruncate(fileHandle, dwRestOffset);
                    }
                    dwRestOffset = 0;
                    sData = EstablishDataConnection(&saiData, &sPasv);
                    if (sData) {
                        SocketSendString(sCmd, "150 Opened %s mode data connection for \"%s\".\r\n",
                            sPasv ? "passive" : "active", newVirtual.c_str());
                        if (ReceiveSocketFile(sCmd, sData, fileHandle)) {
                            SocketSendString(sCmd, "226 \"%s\" transferred successfully.\r\n", newVirtual.c_str());
                        } else {
                            SocketSendString(sCmd, "426 Connection closed; transfer aborted.\r\n");
                        }
                        SocketUtility::CloseSocket(sData);
                    } else {
                        SocketSendString(sCmd, "425 Can't open data connection.\r\n");
                    }
                    FileSystem::FileClose(fileHandle);
                } else {
                    SocketSendString(sCmd, "550 \"%s\": Unable to open file.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "ABOR")) {
            SocketUtility::CloseSocket(sPasv);
            dwRestOffset = 0;
            SocketSendString(sCmd, "200 ABOR command successful.\r\n");
        }

        else if (String::EqualsIgnoreCase(szCmd, "SIZE")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);

                uint32_t fileHandle;
                if (FileSystem::FileOpen(ftpPath, FileModeRead, fileHandle) == true) {
                    uint32_t fileSize = 0;
                    FileSystem::FileSize(fileHandle, fileSize);
                    SocketSendString(sCmd, "213 %llu\r\n", (unsigned long long)fileSize);
                    FileSystem::FileClose(fileHandle);
                } else {
                    SocketSendString(sCmd, "550 \"%s\": File not found.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "MDTM")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                for (i = 0; i < 14; i++) {
                    if ((pszParam[i] < '0') || (pszParam[i] > '9')) {
                        break;
                    }
                }
                std::string paramPath = pszParam;
                if ((i == 14) && (pszParam.size() > 14 && pszParam[14] == ' ')) {
                    fileTime.year = (WORD)atoi(pszParam.substr(0, 4).c_str());
                    fileTime.month = (WORD)atoi(pszParam.substr(4, 2).c_str());
                    fileTime.day = (WORD)atoi(pszParam.substr(6, 2).c_str());
                    fileTime.hour = (WORD)atoi(pszParam.substr(8, 2).c_str());
                    fileTime.minute = (WORD)atoi(pszParam.substr(10, 2).c_str());
                    fileTime.second = (WORD)atoi(pszParam.substr(12, 2).c_str());
                    paramPath = pszParam.substr(15);
                    dw = 1;
                } else {
                    dw = 0;
                }

                std::string newVirtual = resolveRelative(currentVirtual, paramPath);
                if (dw == 1) {
                    std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                    if (FileSystem::SetFileTime(ftpPath, fileTime) == true) {
                        SocketSendString(sCmd, "250 MDTM command successful.\r\n");
                    } else {
                        SocketSendString(sCmd, "550 \"%s\": File not found.\r\n", newVirtual.c_str());
                    }
                } else {
                    std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                    FileInfoDetail fileInfoDetail;
                    if (FileSystem::FileGetFileInfoDetail(ftpPath, fileInfoDetail)) {
                        SocketSendString(sCmd, "213 %04u%02u%02u%02u%02u%02u\r\n", fileInfoDetail.writeTime.year,
                            fileInfoDetail.writeTime.month, fileInfoDetail.writeTime.day,
                            fileInfoDetail.writeTime.hour, fileInfoDetail.writeTime.minute,
                            fileInfoDetail.writeTime.second);
                    } else {
                        SocketSendString(sCmd, "550 \"%s\": File not found.\r\n", newVirtual.c_str());
                    }
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "DELE")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                if (FileSystem::FileDelete(ftpPath) == true) {
                    SocketSendString(sCmd, "250 \"%s\" deleted successfully.\r\n", newVirtual.c_str());
                } else {
                    SocketSendString(sCmd, "550 \"%s\": File not found.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "RNFR")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                bool exists;
                if ((FileSystem::FileExists(ftpPath, exists) == true && exists == true) ||
                    (FileSystem::DirectoryExists(ftpPath, exists) == true && exists == true)) {
                    rnfr = ftpPath;
                    SocketSendString(sCmd, "350 \"%s\": File/Directory exists; proceed with RNTO.\r\n", newVirtual.c_str());
                } else {
                    SocketSendString(sCmd, "550 \"%s\": File/Directory not found.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "RNTO")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else if (rnfr.empty()) {
                SocketSendString(sCmd, "503 Bad sequence of commands. Send RNFR first.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);

                if (FileSystem::FileMove(rnfr, ftpPath) == true) {
                    SocketSendString(sCmd, "250 RNTO command successful.\r\n");
                    rnfr.clear();
                } else {
                    SocketSendString(sCmd, "553 \"%s\": Unable to rename file.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "MKD") || String::EqualsIgnoreCase(szCmd, "XMKD")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                if (FileSystem::DirectoryCreate(ftpPath) == true) {
                    SocketSendString(sCmd, "250 \"%s\" created successfully.\r\n", newVirtual.c_str());
                } else {
                    SocketSendString(sCmd, "550 \"%s\": Unable to create directory.\r\n", newVirtual.c_str());
                }
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "RMD") || String::EqualsIgnoreCase(szCmd, "XRMD")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (!isLoggedIn) {
                SocketSendString(sCmd, "530 Not logged in.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);
                if (FileSystem::DirectoryDelete(ftpPath, true) == true) {
                    SocketSendString(sCmd, "250 \"%s\" removed successfully.\r\n", newVirtual.c_str());
                } else {
                    SocketSendString(sCmd, "550 \"%s\": Unable to remove directory.\r\n", newVirtual.c_str());
                }
            }
        }
        else if (String::EqualsIgnoreCase(szCmd, "OPTS")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "501 Syntax error in parameters or arguments.\r\n");
            } else if (String::EqualsIgnoreCase(pszParam, "UTF8 On")) {
                SocketSendString(sCmd, "200 Always in UTF8 mode.\r\n");
            } else {
                SocketSendString(sCmd, "501 Option not understood.\r\n");
            }
        }

        else if (String::EqualsIgnoreCase(szCmd, "AVBL")) {
            if (pszParam.empty()) {
                SocketSendString(sCmd, "550 Syntax error in parameters or arguments.\r\n");
            } else {
                std::string newVirtual = resolveRelative(currentVirtual, pszParam);
                std::string ftpPath = DriveMount::MapFtpPath(newVirtual);

                uint64_t totalFree;
                if (DriveMount::GetTotalFreeNumberOfBytes(ftpPath, totalFree)) {
                    SocketSendString(sCmd, "213 %llu\r\n", (unsigned long long)totalFree);
                } else {
                    SocketSendString(sCmd, "550 \"%s\": not found.\r\n", pszParam.c_str());
                }
            }
        }

        else {
            SocketSendString(sCmd, "500 Syntax error, command \"%s\" unrecognized.\r\n", szCmd.c_str());
        }
    }

    SocketUtility::CloseSocket(sPasv);
    SocketUtility::CloseSocket(sCmd);

    if (isLoggedIn) {
        decrementConnections();
    }

    return false;
}

bool WINAPI FtpServer::ListenThread(LPVOID lParam) {
    uint64_t sIncoming = 0;

    while (mStopRequested == false) {
        const int result = SocketUtility::GetReadStatus(sListen);
        if (result == SOCKET_ERROR) {
            Debug::Print("Error: Socket status failed: %i\n", WSAGetLastError());
            break;
        }

        if (result == 1 && SocketUtility::AcceptSocket(sListen, sIncoming)) {
            HANDLE connectionThreadHandle =
                CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ConnectionThread, (void*)sIncoming, 0, NULL);
            if (connectionThreadHandle != NULL) {
                SetThreadPriority(connectionThreadHandle, 2);
                CloseHandle(connectionThreadHandle);
            }
        } else {
            Sleep(50);
        }
    }

    SocketUtility::CloseSocket(sListen);
    SocketUtility::CloseSocket(sIncoming);
    return false;
}

bool FtpServer::Init() {
    mStopRequested = false;

    mListenThreadHandle = NULL;

    XSetFileCacheSize(4 * 1024 * 1024);

    memset(&saiListen, 0, sizeof(SOCKADDR_IN));
    saiListen.sin_family = AF_INET;
    saiListen.sin_addr.S_un.S_addr = INADDR_ANY;
    saiListen.sin_port = htons(21);

    mMaxConnections = 20;
    mCommandTimeout = 300;
    mConnectTimeout = 60;

    SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sListen);

    if (!SocketUtility::BindSocket(sListen, &saiListen)) {
        SocketUtility::CloseSocket(sListen);
        return false;
    }

    SocketUtility::ListenSocket(sListen, SOMAXCONN);

    mListenThreadHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ListenThread, 0, 0, NULL);
    if (mListenThreadHandle != NULL) {
        SetThreadPriority(mListenThreadHandle, 2);
    }

    return true;
}

void FtpServer::Close() {
    mStopRequested = true;
    WaitForSingleObject(mListenThreadHandle, INFINITE);
    CloseHandle(mListenThreadHandle);
}

bool FtpServer::SocketSendString(uint64_t s, const char* format, ...) {

    va_list args;
    va_start(args, format);

    uint32_t length = _vsnprintf(NULL, 0, format, args);

    char* result = (char*)malloc(length + 1);
    _vsnprintf(result, length, format, args);
    result[length] = 0;

    va_end(args);

    bool bSuccess = send((SOCKET)s, result, strlen(result), 0) != SOCKET_ERROR;
    free(result);
    return bSuccess;
}

FtpServer::ReceiveStatus FtpServer::SocketReceiveString(
    uint64_t s, char* psz, uint32_t dwMaxChars, uint32_t* pdwCharsReceived) {
    uint32_t dwChars = 0;
    ReceiveStatus status, statusError;
    char buf[2];
    uint32_t dw;

    for (;;) {
        if (dwChars == dwMaxChars) {
            statusError = ReceiveStatus_Insufficient_Buffer;
            break;
        }

        status = SocketReceiveLetter(s, psz, dwMaxChars - dwChars, &dw);
        if (status == ReceiveStatus_OK) {
            dwChars += dw;
            if (*psz == '\r')
                *psz = 0;
            else if (*psz == '\n') {
                *psz = 0;
                *pdwCharsReceived = dwChars;
                return ReceiveStatus_OK;
            }
            psz += dw;
        } else if (status == ReceiveStatus_Invalid_Data || status == ReceiveStatus_Insufficient_Buffer) {
            statusError = status;
            break;
        } else {
            return status;
        }
    }

    // A non-critical error occurred, read until end of line
    for (;;) {
        status = SocketReceiveLetter(s, buf, sizeof(buf) / sizeof(char), &dw);
        if (status == ReceiveStatus_OK) {
            if (*buf == '\n') {
                return statusError;
            }
        } else if (status == ReceiveStatus_Invalid_Data || status == ReceiveStatus_Insufficient_Buffer) {
            // Go on...
        } else {
            return status;
        }
    }
}

FtpServer::ReceiveStatus FtpServer::SocketReceiveLetter(
    uint64_t s, char* pch, uint32_t dwMaxChars, uint32_t* pdwCharsReceived) {
    char buf[4];
    uint32_t dw;
    TIMEVAL tv;
    fd_set fds;

    tv.tv_sec = mCommandTimeout;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET((SOCKET)s, &fds);
    dw = select(0, &fds, 0, 0, &tv);
    if (dw == SOCKET_ERROR || dw == 0)
        return ReceiveStatus_Timeout;
    dw = recv((SOCKET)s, &buf[0], 1, 0);
    if (dw == SOCKET_ERROR || dw == 0)
        return ReceiveStatus_Network_Error;

    if (dwMaxChars == 0) {
        return ReceiveStatus_Insufficient_Buffer;
    }

    pch[0] = buf[0];

    *pdwCharsReceived = 1;
    return ReceiveStatus_OK;
}

FtpServer::ReceiveStatus FtpServer::SocketReceiveData(
    uint64_t s, char* psz, uint32_t dwBytesToRead, uint32_t* pdwBytesRead) {
    uint32_t dw;
    TIMEVAL tv;
    fd_set fds;

    tv.tv_sec = mConnectTimeout;
    tv.tv_usec = 0;
    memset(&fds, 0, sizeof(fd_set));
    FD_SET((SOCKET)s, &fds);
    dw = select(0, &fds, 0, 0, &tv);
    if (dw == SOCKET_ERROR || dw == 0) {
        Debug::Print("Error: SocketReceiveData failed with Timeout.\n");
        return ReceiveStatus_Timeout;
    }
    dw = recv((SOCKET)s, psz, dwBytesToRead, 0);
    if (dw == SOCKET_ERROR) {
        Debug::Print("Error: SocketReceiveData failed with Network Error.\n");
        return ReceiveStatus_Network_Error;
    }
    *pdwBytesRead = dw;
    return ReceiveStatus_OK;
}

uint64_t FtpServer::EstablishDataConnection(sockaddr_in* psaiData, uint64_t* psPasv) {
    uint64_t sData;
    uint32_t dw;
    TIMEVAL tv;
    fd_set fds;

    if (psPasv && *psPasv) {
        tv.tv_sec = mConnectTimeout;
        tv.tv_usec = 0;
        memset(&fds, 0, sizeof(fd_set));
        FD_SET((SOCKET)*psPasv, &fds);
        dw = select(0, &fds, 0, 0, &tv);
        if (dw && dw != SOCKET_ERROR) {
            SocketUtility::AcceptSocket(*psPasv, psaiData, sData);
        } else {
            sData = 0;
        }
        SocketUtility::CloseSocket(*psPasv);
        return sData;
    } else {
        SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sData);
        if (!SocketUtility::ConnectSocket(sData, psaiData)) {
            SocketUtility::CloseSocket(sData);
            return INVALID_SOCKET;
        } else {
            return sData;
        }
    }
}

bool FtpServer::ReceiveSocketFile(uint64_t sCmd, uint64_t sData, uint32_t fileHandle) {
    uint32_t socketBufferSize = RECV_SOCKET_BUFFER_SIZE;
    SocketUtility::SetSocketRecvSize(sData, socketBufferSize);

    int driveSectorBufferSize = 128 * 1024;
    int combinedBufferSize = socketBufferSize + driveSectorBufferSize;
    char* combinedBuffer = (char*)malloc(combinedBufferSize);

    bool fileComplete = false;

    uint32_t totalBytesToWrite = 0;
    uint32_t totalWritten = 0;

    while (true) {
        while (!fileComplete && totalBytesToWrite < (uint32_t)driveSectorBufferSize) {
            uint32_t bytesRead = 0;
            if (SocketReceiveData(sData, combinedBuffer + totalBytesToWrite, socketBufferSize, &bytesRead) !=
                ReceiveStatus_OK) {
                free(combinedBuffer);
                return false;
            }
            totalBytesToWrite += bytesRead;
            if (bytesRead == 0) {
                fileComplete = true;
                break;
            }
        }

        if (fileComplete && totalBytesToWrite == 0) {
            free(combinedBuffer);
            return true;
        }

        uint32_t bytesWritten = 0;
        uint32_t bytesToWrite = min((uint32_t)driveSectorBufferSize, totalBytesToWrite);
        if (!FileSystem::FileWrite(fileHandle, combinedBuffer, bytesToWrite, bytesWritten)) {
            Debug::Print("Error: WriteFile failed: %i", GetLastError());
            free(combinedBuffer);
            return false;
        }

        totalBytesToWrite -= bytesWritten;
        totalWritten += bytesWritten;
        memmove(combinedBuffer, combinedBuffer + bytesWritten, totalBytesToWrite);
    }
}

bool FtpServer::SendSocketFile(uint64_t sCmd, uint64_t sData, uint32_t fileHandle, uint32_t* pdwAbortFlag) {
    uint32_t bufferSize = SEND_SOCKET_BUFFER_SIZE;
    SocketUtility::SetSocketSendSize(sData, bufferSize);
    char* szBuffer = (char*)malloc(bufferSize);

    while (true) {
        uint32_t bytesRead;
        if (FileSystem::FileRead(fileHandle, szBuffer, bufferSize, bytesRead) == false) {
            Debug::Print("Error: ReadFile failed: %i", GetLastError());
            free(szBuffer);
            return false;
        }

        if (bytesRead == 0) {
            free(szBuffer);
            return true;
        }

        int bytesToSend = bytesRead;
        int bufferOffset = 0;

        while (bytesToSend > 0) {
            int sent = send((SOCKET)sData, szBuffer + bufferOffset, bytesToSend, 0);
            int queueLength = 0;
            SocketUtility::GetReadQueueLength(sCmd, queueLength);
            if (queueLength > 0) {
                char cmdBuf[512];
                uint32_t bytes_received = 0;
                if (SocketReceiveString(sCmd, cmdBuf, 512, &bytes_received) == ReceiveStatus_OK) {
                    std::string cmdLine(cmdBuf, bytes_received);
                    if (String::EqualsIgnoreCase(cmdLine, "ABOR")) {
                        *pdwAbortFlag = 1;
                        free(szBuffer);
                        return false;
                    } else {
                        SocketSendString(sCmd, "500 Only command allowed at this time is ABOR.\r\n");
                    }
                }
            }
            bytesToSend -= sent;
            bufferOffset += (int)sent;
            if (sent < 1) {
                free(szBuffer);
                return false;
            }
        }
    }
    return true;
}
