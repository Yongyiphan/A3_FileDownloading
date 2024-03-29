#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include "../taskqueue.h"
#include "../Utils.h"
#include <fstream>


#define DEV

//#include <Windows.h>
//#include <WS2tcpip.h>
//#pragma comment(lib, "ws2_32.lib")

Connection serverTCP, serverUDP;
std::string ServerIP;
std::string ServerTCP_Port;
std::string ServerUDP_Port;
std::string DL_Path;
int Slide_WindowSize, Ack_Timer;
float Loss_Rate;


int errCode;

void Start();
void TCP_Server_Run();
int ListenSocket(Connection*);
int AwaitSocket(Connection*);
bool ExecuteSocket(SOCKET);
void DisconnectSocket(SOCKET*);
int main(int argc, char** argv) {

  StartWSA();
  Ask("Server TCP Port Number: ", ServerTCP_Port, "9000");
  Ask("Server UDP Port Number: ", ServerUDP_Port, "9001");
  Ask("Download path: ", DL_Path, "C:\\Test\\");
  Ask("Sliding window size [1,100]: ", Slide_WindowSize, 4);
  Ask("Packet loss rate [0.0-1.0]: ", Loss_Rate, 0.2f);
  Ask("Ack timer [10ms-500ms]: ", Ack_Timer, 20);
  Start();
  TCP_Server_Run();

  EndWSA();
  return 0;
}

void Start() {
  addrinfo hints;
  SocketInfo* TCPsoc = &serverTCP.sinfo;
  SecureZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;        // IPv4
  // For UDP use SOCK_DGRAM instead of SOCK_STREAM.
  hints.ai_socktype = SOCK_STREAM;  // Reliable delivery
  // Could be 0 for autodetect, but reliable delivery over IPv4 is always TCP.
  hints.ai_protocol = IPPROTO_TCP;  // TCP
  // Create a passive socket that is suitable for bind() and listen().
  hints.ai_flags = AI_PASSIVE;
  char hostname[512];
  if (gethostname(hostname, 512) == SOCKET_ERROR)
  {
    ErrMsg("gethostname()");
    errCode = 1;
  }
  TCPsoc->CreateSocket(hostname, ServerTCP_Port, hints);
  TCPsoc->Bind();

  auto sinfo = reinterpret_cast<sockaddr_in*>(TCPsoc->a_info->ai_addr);
  sinfo->sin_port = u_short(std::stol(TCPsoc->PortStr, NULL, 0));
  serverTCP.Data(GetIPAddress(sinfo));

  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  SocketInfo* UDPsoc = &serverUDP.sinfo;
  UDPsoc->CreateSocket(hostname, ServerUDP_Port, hints);
  UDPsoc->Bind();
  sinfo = reinterpret_cast<sockaddr_in*>(UDPsoc->a_info->ai_addr);
  sinfo->sin_port = u_short(std::stol(UDPsoc->PortStr, NULL, 0));
  serverUDP.Data(GetIPAddress(sinfo));
  std::cout << "Server IP Address: " << GetIPAddress(sinfo, false) << "\n";
  std::cout << "Server TCP Port Number: " << TCPsoc->PortStr << "\n";
  std::cout << "Server UDP Port Number: " << UDPsoc->PortStr << "\n";
}


int ListenSocket(Connection* conn) {
  errCode = listen(conn->sinfo.soc, SOMAXCONN);
  if (errCode != NO_ERROR) {
    ErrMsg("listen()");
    conn->Cleanup();
    return 3;
  }
  return NO_ERROR;
}

void TCP_Server_Run() {
  if (ListenSocket(&serverTCP) == 3) {
    return;
  }
  const auto onDisconnect = [&]()
    {
      DisconnectSocket(&serverTCP.sinfo.soc);
    };
  auto tq = TaskQueue<SOCKET, decltype(ExecuteSocket), decltype(onDisconnect)>{ 10, 20, ExecuteSocket, onDisconnect };
  while (serverTCP.sinfo.soc != INVALID_SOCKET)
  {
    sockaddr clientAddress{};
    SecureZeroMemory(&clientAddress, sizeof(clientAddress));
    int clientAddressSize = sizeof(clientAddress);
    SOCKET clientSocket = accept(serverTCP.sinfo.soc, &clientAddress, &clientAddressSize);
    if (clientSocket == INVALID_SOCKET)
    {
      break;
    }
    sockaddr_in* sinfo = reinterpret_cast<sockaddr_in*>(&clientAddress);
    StoreTCPConnection(sinfo, clientSocket);
    tq.produce(clientSocket);
  }

  while (serverTCP.sinfo.soc != INVALID_SOCKET)
  {
    sockaddr clientAddress{};
    SecureZeroMemory(&clientAddress, sizeof(clientAddress));
    int clientAddressSize = sizeof(clientAddress);
    SOCKET clientSocket = accept(serverUDP.sinfo.soc, &clientAddress, &clientAddressSize);
    if (clientSocket == INVALID_SOCKET)
    {
      break;
    }
    sockaddr_in* sinfo = reinterpret_cast<sockaddr_in*>(&clientAddress);
    StoreUDPConnection(sinfo, clientSocket);
    tq.produce(clientSocket);
  }
#if defined(DEV)
  std::cout << "Exiting TCP Run\n";
#endif
}

bool ExecuteSocket(SOCKET soc) {
  char buffer[BUF_LEN]{};
  bool stay = true;
  std::string clientMsg{}, replyMsg{}, serverMsg;

  Connection* conn{ nullptr };
  auto FindCon = [](SOCKET soc) -> Connection* {
    for (auto& con : TCP_List) {
      if (con.sinfo.soc == soc) {
        return &con;
      }
    }
    return nullptr;
    };
  conn = FindCon(soc);
  if (!conn)return false;
  int bytesReceieved{ SOCKET_ERROR };
  while (stay) {
    bytesReceieved = recv(soc, buffer, BUF_LEN - 1, 0);
    if (bytesReceieved == SOCKET_ERROR) {
      ErrMsg("recv()"); break;
    };
    if (bytesReceieved == 0) {
      std::cerr << "Graceful shutdown.\n";
      break;
    }

    buffer[bytesReceieved] = '\0'; // Cut off data
    CMDID commandID = static_cast<CMDID>(buffer[0]);
    namespace fs = std::filesystem;
    if (commandID == REQ_QUIT) {
      buffer[0] = commandID;
      send(soc, buffer, 1, 0);
      break;
    }
    else if (commandID == REQ_LISTFILES) {
      commandID = CMDID::RSP_LISTFILES;
    }
    else if (commandID == REQ_DOWNLOAD) {
      commandID = CMDID::RSP_DOWNLOAD;
    }

    if (commandID == RSP_LISTFILES) {
      if (fs::is_directory(DL_Path)) {
        // Iterate through the directory entries

        // BUF_LEN - 1 - (1 + 2 + 4)
        size_t limit = BUF_LEN - 8, current{}, offset{ 7 };
        u_long fileListLen_Network{ 0 }, nofile_Network{ 0 };
        u_short no_files{};
        buffer[0] = commandID;
        auto sendListFile = [](SOCKET soc, u_short no_files, size_t length_of_fileList, char* buffer)->int {
          u_long fileListLen_Network{ 0 }, nofile_Network{ 0 };
          nofile_Network = htons(no_files);
          memcpy(buffer + 1, &nofile_Network, sizeof(uint16_t));

          fileListLen_Network = htonl(length_of_fileList);
          memcpy(buffer + 3, &fileListLen_Network, sizeof(uint32_t));
          return send(soc, buffer, BUF_LEN, 0);
          };

        std::vector<char> File_ListV{};
        for (const auto& entry : fs::directory_iterator(DL_Path)) {
            // Check if the directory entry is a file
          if (entry.is_regular_file()) {
              // Output the filename
            std::string filename = entry.path().filename().string();
            uint32_t filelen_network = htonl(filename.length());
            char* flen_Network_Ptr = reinterpret_cast<char*>(&filelen_network);
            File_ListV.insert(File_ListV.end(), flen_Network_Ptr, flen_Network_Ptr + sizeof(uint32_t));
            File_ListV.insert(File_ListV.end(), filename.begin(), filename.end());
            no_files++;
          }
        }

        std::vector<char> ChunkBuffer{};
        ChunkBuffer.reserve(BUF_LEN);
        ChunkBuffer.push_back(static_cast<char>(commandID));
        uint16_t NW_FileCount = htons(no_files);
        uint32_t NW_FileLenght = htonl();
        

        sendListFile(soc, no_files, current, buffer);
      }
    }
    else if (commandID == RSP_DOWNLOAD) {
      uint32_t fn_len = ntohl(*reinterpret_cast<uint32_t*>(&buffer[7]));
      std::string fileName(buffer + 11, fn_len);
      fs::path DL_File = fs::path(DL_Path) / fileName;
      std::ifstream file(DL_File.string(), std::ios::binary);
      if (!file.is_open()) {
        std::cerr << "Failed to open file: " << fileName << std::endl;
        continue;
      }
#ifdef DEV
      std::cout << "Opening: " << DL_File.string() << std::endl;
#endif
// target port no
      uint16_t target_port = ntohl(*reinterpret_cast<uint16_t*>(&buffer[5]));

      SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      sockaddr_in clientAddr;
      memset(&clientAddr, 0, sizeof(clientAddr));
      clientAddr.sin_family = AF_INET;
      clientAddr.sin_port = htons(target_port);
      clientAddr.sin_addr.s_addr = *reinterpret_cast<uint32_t*>(&buffer[1]);
      buffer[0] = CMDID::RSP_DOWNLOAD;
      send(soc, buffer, 1, 0);

      char fileBuffer[1024]; // Adjust buffer size as needed
      while (file.read(fileBuffer, sizeof(fileBuffer))) {
        sendto(udpSocket, fileBuffer, file.gcount(), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
      }

      closesocket(udpSocket);
      file.close();

    }
  }
  shutdown(soc, SD_BOTH);
#if defined(DEV)
  std::cout << "client " << conn->IPFull << " is closing\n";
#endif
  closesocket(soc);
  return stay;
}
void DisconnectSocket(SOCKET* soc) {
  if (soc)
  {
    if (*soc != INVALID_SOCKET)
    {
      shutdown(*soc, SD_BOTH);
      closesocket(*soc);
      *soc = INVALID_SOCKET;
    }
  }
}
