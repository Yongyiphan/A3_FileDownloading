/*****************************************************************//**
 * \file            client.cpp
 * \author(s)       Edgar Yong (y.yiphanedgar\@digipen.edu) (2202206)
 * \author(s)       Geoffrey Cho (g.cho\@digipen.edu) ()
 * \date            31 March 2024
 * \brief           Multi threaded client.
 *                  Client inputs request to server.
 *                  Commands Available
 *                      /d for downloading a file
 *                      /l for listing available files at specified directory
 *
 * \copyright       All content © 2024 DigiPen Institute of Technology Singapore. All Rights Reserved
 *********************************************************************/
#include <iostream>
#include <string>
#include "../taskqueue.h"
#include "../Utils.h"
#include <thread>
#include <fstream>
#include <sstream>


#define DEV
Connection clientUDPCon, clientTCPCon;
std::string ClientIP{}, ClientUDP{};
std::string ServerTCP_Port{};
std::string ServerUDP_Port{};
std::string StorePath{};
int Slide_WindowSize{};
float Loss_Rate{};

sockaddr_in UDPSocketAddr;


bool ConnectToServer();
void Communicate();
void ClientInput();
bool ValidateCommand(std::string const& msg, CMDID&);
void PackageEchoMsg(std::string const& msg);
void ServerReply();

int main(int argc, char** argv) {
  StartWSA();
  Ask("Server IP Address: ", ClientIP, "192.168.5.1");
  Ask("Server TCP Port Number: ", ServerTCP_Port, "9000");
  Ask("Server UDP Port Number: ", ServerUDP_Port, "9001");
  Ask("Client UDP Port Number: ", ClientUDP, "9010");
  Ask("Path to store files: ", StorePath, "C:\\Test\\");
  Ask("Sliding window size: ", Slide_WindowSize, 4);
  Ask("Packet loss rate: ", Loss_Rate, 0.2);
  bool Connected;
  int Tries{};
  while (!(Connected = ConnectToServer()) && Tries < 4) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
#ifdef DEV
    std::cout << "Tying to connect to Server\n";
#endif
    Tries++;
  }
  if (Connected) {
#ifdef DEV
    std::cout << "Connected To Server\n";
#endif
    Communicate();

  }

  EndWSA();
  return 0;
}


bool ConnectToServer() {
  std::cout << "IP Address: " << ClientIP << "\n";
  std::cout << "Port Number: " << ServerTCP_Port << "\n";
  addrinfo TCPhints, UDPhints;
  SecureZeroMemory(&TCPhints, sizeof(TCPhints));
  TCPhints.ai_family = AF_INET;
  TCPhints.ai_socktype = SOCK_STREAM;
  TCPhints.ai_protocol = IPPROTO_TCP;
  TCPhints.ai_flags = AI_PASSIVE;
  clientTCPCon.sinfo.CreateSocket(ClientIP, ServerTCP_Port, TCPhints);

  if (clientTCPCon.sinfo.Connect() == SOCKET_ERROR)
  {
    ErrMsg("connect()");
    return false;
  }
  clientTCPCon.Data(ClientIP, ServerTCP_Port);

  SecureZeroMemory(&UDPhints, sizeof(UDPhints));
  UDPhints.ai_family = AF_INET;
  UDPhints.ai_socktype = SOCK_DGRAM;
  UDPhints.ai_protocol = IPPROTO_UDP;
  UDPhints.ai_flags = AI_PASSIVE;
  clientUDPCon.sinfo.CreateSocket(ClientIP, ServerUDP_Port, UDPhints);
  SecureZeroMemory(&UDPSocketAddr, sizeof(UDPSocketAddr));
  UDPSocketAddr.sin_family = AF_INET;
  UDPSocketAddr.sin_port = htons(static_cast<uint16_t>(std::stol(ClientUDP)));
  clientUDPCon.sinfo.Bind(&UDPSocketAddr, sizeof(UDPSocketAddr));
  if (clientUDPCon.sinfo.Connect() == SOCKET_ERROR)
  {
    ErrMsg("connect()");
    return false;
  }
  clientUDPCon.Data(ClientIP, ClientUDP);


  freeaddrinfo(clientTCPCon.sinfo.a_info);
  freeaddrinfo(clientUDPCon.sinfo.a_info);
  clientTCPCon.sinfo.a_info = NULL;
  clientUDPCon.sinfo.a_info = NULL;

  return true;
}

void Communicate() {
  //u_long mode = 1;
  //ioctlsocket(con->sinfo.soc, FIONBIO, &mode);
  std::cout << std::endl;
  std::thread InputThread(ClientInput);
  std::thread ReplyThread(ServerReply);
  InputThread.join();
  ReplyThread.join();
}

void ClientInput() {
  char buffer[BUF_LEN]{};
  std::string clientMsg{};
  CMDID commandID{ CMDID::UNKNOWN };
  int bytesSent;
  std::cout << "Command Prompt> ";
  while (true) {
    std::getline(std::cin, clientMsg);
    if (clientMsg.empty())continue;
    std::istringstream iss(clientMsg);
    std::string prefix, IPSFull, PS, FileName;
    iss >> prefix;
    iss >> IPSFull;
    iss >> FileName;
    if (ValidateCommand(prefix, commandID)) {
      if (commandID == CMDID::REQ_QUIT) {
#if defined(DEV)
        std::cout << "REQ_QUIT\n";
#endif
        buffer[0] = commandID;
        bytesSent = send(clientTCPCon.sinfo.soc, buffer, 1, 0);
        break;
      }
      else if (commandID == CMDID::REQ_DOWNLOAD) {
#if defined(DEV)
        std::cout << "REQ_DOWNLOAD\n";
#endif
        // Validate IP Address + port
        if (IPSFull.empty())continue;
        if (IPSFull.find(':') == std::string::npos) {
          continue;
        }
        std::string targetIP = IPSFull.substr(0, IPSFull.find(':'));
        std::string targetPN = IPSFull.substr(IPSFull.find(':') + 1);
        uint32_t targetIP_Host;
        if (inet_pton(AF_INET, targetIP.c_str(), &targetIP_Host) != 1)continue;
        uint16_t targetPN_Network{ htons(static_cast<uint16_t>(std::stol(targetPN))) };
        uint32_t targetIP_Network{ htonl(targetIP_Host) };

        /*
          4 -> IP
          2 -> Port
          4 -> filename length
          n -> filename
          Note: no need for buffer overflow handle, not realistic for a filename to be that long
          Buffer is already like 4k
        */
        buffer[0] = commandID;
        memcpy(buffer + 1, &targetIP_Network, sizeof(uint32_t));
        memcpy(buffer + 5, &targetPN_Network, sizeof(uint16_t));
        uint32_t fn_len_Network = htonl(static_cast<uint32_t>(FileName.length()));
        memcpy(buffer + 7, &fn_len_Network, sizeof(uint32_t));
        memcpy(buffer + 11, FileName.data(), FileName.length());
        send(clientTCPCon.sinfo.soc, buffer, 11 + FileName.length(), 0);

      }
      else if (commandID == CMDID::REQ_LISTFILES) {
#if defined(DEV)
        std::cout << "REQ_LISTFILES\n";
#endif
        buffer[0] = commandID;
        bytesSent = send(clientTCPCon.sinfo.soc, buffer, 1, 0);

      }
    }
  }
  std::lock_guard<std::mutex> clientLock{ _stdoutMutex };
}

bool ValidateCommand(std::string const& msg, CMDID& cid) {
  cid = CMDID::UNKNOWN;
  // Empty msg or lack of command = reject
  if (msg.empty() || msg.length() < 2)return false;
  bool cmdHead = (msg[0] == '/');
  if (!cmdHead) return false;

  char cmd = msg[1];
  if (cmd == 'q') {
    cid = CMDID::REQ_QUIT;
  }
  else if (cmd == 'd') {
    cid = CMDID::REQ_DOWNLOAD;
  }
  else if (cmd == 'l') {
    cid = CMDID::REQ_LISTFILES;
  }
  return true;

}

void PackageEchoMsg(std::string const& msg) {}

void ServerReply() {
  int bytesRecv;
  bool stay = true;
  char buffer[BUF_LEN];
  CMDID commandID = CMDID::UNKNOWN;
  while (stay) {
    bytesRecv = recv(clientTCPCon.sinfo.soc, buffer, BUF_LEN - 1, 0);
    if (bytesRecv <= 0)continue;
    commandID = static_cast<CMDID>(buffer[0]);
    if (commandID == REQ_QUIT) {
#ifdef DEV
      std::cout << "RSP_QUIT\n";
#endif
      // Client Input on Different thread. Using Server to prompt this exe to close
      shutdown(clientTCPCon.sinfo.soc, SD_SEND);
      closesocket(clientTCPCon.sinfo.soc);
      std::cout << "disconnection..." << std::endl;
      break;
    }
    else if (commandID == RSP_LISTFILES) {
#ifdef DEV
      std::cout << "RSP_LISTFILES\n";
#endif
      uint16_t no_files = ntohs(*reinterpret_cast<uint16_t*>(&buffer[1]));
      uint32_t fileList_len = ntohl(*reinterpret_cast<uint32_t*>(&buffer[3]));
      std::stringstream ss;
      size_t headerSize = 7;
      std::vector<char> RecvBuffer;
      RecvBuffer.reserve(fileList_len);
      size_t offset = headerSize;
      RecvBuffer.insert(RecvBuffer.end(), buffer + offset, buffer + offset + bytesRecv - headerSize);
      size_t totalReceived = bytesRecv - headerSize;
      while (totalReceived < fileList_len) {
        memset(buffer, 0, BUF_LEN);
        bytesRecv = recv(clientTCPCon.sinfo.soc, buffer, BUF_LEN - 1, 0);
        RecvBuffer.insert(RecvBuffer.end(), buffer, buffer + bytesRecv);
        totalReceived += bytesRecv;
      }

      ss << "\n# of Files: " << std::to_string(no_files) << "\n";
      for (int i{}, offset = 0; i < no_files; ++i) {
        uint32_t filename_len = ntohl(*reinterpret_cast<uint32_t*>(&RecvBuffer[offset]));
        std::string filename(RecvBuffer.data() + offset + sizeof(uint32_t), filename_len);
        ss << std::to_string(i + 1) << "-th file: " << filename;
        ss << "\n";
        offset += sizeof(uint32_t) + filename_len;
      }
      std::string message;
      if (no_files) {
        message = ss.str();
        message.pop_back();
      }

      ReplyFormat(message);
    }
    else if (commandID == RSP_DOWNLOAD) {
#ifdef DEV
      std::cout << "RSP_DOWNLOAD\n";
#endif
      uint32_t IP_ = ntohl(*reinterpret_cast<uint32_t*>(&buffer[1]));
      uint16_t PN_ = ntohs(*reinterpret_cast<uint16_t*>(&buffer[5]));
      uint32_t SID = *reinterpret_cast<uint32_t*>(&buffer[7]);
      uint32_t LEN = ntohl(*reinterpret_cast<uint32_t*>(&buffer[11]));
      uint32_t FN_LEN = ntohl(*reinterpret_cast<uint32_t*>(&buffer[15]));
      std::string FileName(buffer + 19, FN_LEN);
      Connection UDPCon(IP_, PN_);

      std::cout << "\nNow listening for messages on " << UDPCon.IPFull << " ...\n";
      std::cout << "Start UDP Session\n";
      fs::path outputfile = fs::path(StorePath) / FileName;

      std::ofstream oFile(outputfile, std::ios::binary | std::ios::out);
      if (!oFile) {
        std::cout << "Unable to open File\n";
      }
      UDPMessage UDPChunk{};
      int UDPServerAddrSize = sizeof(UDPSocketAddr);
      int DGRAM_Recv{};
      std::vector<char> FileData;
      uint32_t currentReceived{};
      while ((DGRAM_Recv = recvfrom(clientUDPCon.sinfo.soc, buffer, BUF_LEN - 1, 0, (struct sockaddr*)&UDPSocketAddr, &UDPServerAddrSize))) {
        UDPChunk.SessionID = ntohl(*reinterpret_cast<uint32_t*>(&buffer));
        UDPChunk.Flags = *reinterpret_cast<uint32_t*>(&buffer[4]);
        UDPChunk.SeqNo = *reinterpret_cast<uint32_t*>(&buffer[5]);
        UDPChunk.FileLen = *reinterpret_cast<uint32_t*>(&buffer[9]);
        UDPChunk.FileOffset = *reinterpret_cast<uint32_t*>(&buffer[13]);
        UDPChunk.FileDataLen = *reinterpret_cast<uint32_t*>(&buffer[17]);
        FileData.insert(FileData.end(), buffer + 21, buffer + 21 + UDPChunk.FileDataLen);
        currentReceived += UDPChunk.FileDataLen;
        if (currentReceived == UDPChunk.FileLen) {
          break;
        }
        memset(buffer, '\0', BUF_LEN);
      }

      oFile.seekp(UDPChunk.FileOffset, std::ios::beg);
      oFile.write(FileData.data(), UDPChunk.FileLen);
      if (!oFile) {
        ErrMsg("write()");
      }
      oFile.close();


      // Reply to server
      //memset(buffer, '\0', BUF_LEN);
      //uint32_t SID_Network = htonl(UDPChunk.SessionID);
      //memcpy(buffer, &SID_Network, sizeof(uint32_t));
      //uint8_t Flag = 1;
      //memcpy(buffer + 4, &Flag, sizeof(uint8_t));
      



    }
    std::cout << "Command Prompt> ";
  }
}
