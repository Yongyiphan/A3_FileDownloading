
#include <iostream>
#include <string>
#include "../Utils.h"
#include <thread>
#include <fstream>
#include <sstream>

#define DEV
Connection clientUDPCon, clientTCPCon, serverCon;
std::string ClientIP{}, ClientUDP{};
std::string ServerTCP_Port{};
std::string ServerUDP_Port{};
std::string StorePath{};
int Slide_WindowSize{};
float Loss_Rate{};


bool ConnectToServer();
void Communicate();
void ClientInput();
bool ValidateCommand(std::string const& msg, CMDID&);
void PackageEchoMsg(std::string const& msg);
void ServerReply();

int main(int argc, char** argv) {
  StartWSA();
  Ask("Sever IP Address: ", ClientIP, "192.168.5.1");
  Ask("Server TCP Port Number: ", ServerTCP_Port, "9000");
  Ask("Server UDP Port Number: ", ServerUDP_Port, "9001");
  Ask("Client UDP Port Number: ", ClientUDP, "9010");
  Ask("Path to store files: ", StorePath, "C:\\Test\\");
  Ask("Sliding window size: ", Slide_WindowSize, 4);
  Ask("Packet loss rate: ", Loss_Rate, 0.2);
  bool Connected;
  int Tries{};
  while (!ConnectToServer() && Tries < 4) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
#ifdef DEV
    std::cout << "Tying to connect to Server\n";
#endif
    Tries++;
  }
#ifdef DEV
  std::cout << "Connected To Server\n";
#endif
  Communicate();

  EndWSA();
  return 0;
}


bool ConnectToServer() {
  std::cout << "IP Address: " << ClientIP << "\n";
  std::cout << "Port Number: " << ServerTCP_Port << "\n";
  addrinfo hints;
  SecureZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;
  clientTCPCon.sinfo.CreateSocket(ClientIP, ServerTCP_Port, hints);

  if (clientTCPCon.sinfo.Connect() == SOCKET_ERROR)
  {
    ErrMsg("connect()");
    return false;
  }
  clientTCPCon.Data(ClientIP, ServerTCP_Port);
  clientUDPCon.Data(ClientIP, ClientUDP);

  return true;
}

void Communicate() {
  //u_long mode = 1;
  //ioctlsocket(con->sinfo.soc, FIONBIO, &mode);
  std::cout << std::endl;
  std::thread InputThread(ClientInput);
  ServerReply();
  InputThread.join();
}

void ClientInput() {
  char buffer[BUF_LEN]{};
  std::string clientMsg{};
  CMDID commandID{ CMDID::UNKNOWN };
  int bytesSent;
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

        /*
          4 -> IP
          2 -> Port
          4 -> filename length
          n -> filename
        */
        buffer[0] = commandID;
        memcpy(buffer + 1, &clientTCPCon.NIP32, sizeof(uint32_t));
        memcpy(buffer + 5, &clientUDPCon.NP16, sizeof(uint16_t));
        uint32_t filenamelen = static_cast<uint32_t>(FileName.length());
        uint32_t nfilenamelen = htonl(filenamelen);
        memcpy(buffer + 7, &nfilenamelen, sizeof(uint32_t));
        memcpy(buffer + 11, FileName.data(), filenamelen);
        send(clientTCPCon.sinfo.soc, buffer, 11 + filenamelen, 0);

        int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = htons(static_cast<u_short>(std::stol(ClientUDP))); // Convert port number to network byte order
        clientAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on any IP address

        if (bind(udpSocket, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
          closesocket(udpSocket);
          return;
        }

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
      ss << "# of Files: " << std::to_string(no_files) << "\n";
      for (int i{}, offset = 7; i < no_files; ++i) {
        uint32_t filename_len;
        memcpy(&filename_len, buffer + offset, sizeof(uint32_t));
        filename_len = ntohl(filename_len);
        char* filename = new char[filename_len + 1] {};
        memcpy(filename, buffer + offset + 4, filename_len);
        offset += 4 + filename_len;
        ss << std::to_string(i + 1) << "-th file: " << filename;
        ss << "\n";
        delete[] filename;
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
      uint32_t IP_ = *reinterpret_cast<uint32_t*>(&buffer[1]);
      uint16_t PN_ = *reinterpret_cast<uint16_t*>(&buffer[5]);
      uint32_t SID = *reinterpret_cast<uint32_t*>(&buffer[7]);
      uint32_t LEN = *reinterpret_cast<uint32_t*>(&buffer[11]);
      PN_ = ntohs(PN_);
      LEN = ntohl(LEN);

      std::ofstream outputFile(StorePath, std::ios::binary);

    }
  }
}
