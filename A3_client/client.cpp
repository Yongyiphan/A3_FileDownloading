
#include <iostream>
#include <string>
#include "../taskqueue.h"
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

  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  clientUDPCon.sinfo.CreateSocket(ClientIP, ServerUDP_Port, hints);
  if (clientUDPCon.sinfo.Connect() == SOCKET_ERROR)
  {
    ErrMsg("connect()");
    return false;
  }
  clientUDPCon.Data(ClientIP, ClientUDP);
  //clientUDPCon.sinfo.Bind();

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
    std::cout << "Command Prompt> ";
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
        if(IPSFull.empty())continue;
        if (IPSFull.find(':') == std::string::npos) {
          continue;
        }
        std::string targetIP = IPSFull.substr(0, IPSFull.find(':'));
        std::string targetPN = IPSFull.substr(IPSFull.find(':') + 1);
        uint32_t targetIP_Host;
        if (inet_pton(AF_INET, targetIP.c_str(), &targetIP_Host) != 1)continue;
        uint16_t targetPN_Network{htons(static_cast<uint16_t>(std::stol(targetPN)))};
        uint32_t targetIP_Network{htonl(targetIP_Host)};

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
  std::lock_guard<std::mutex> clientLock{_stdoutMutex};
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

      ss << "# of Files: " << std::to_string(no_files) << "\n";
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
      uint32_t LEN = *reinterpret_cast<uint32_t*>(&buffer[11]);
      Connection UDPCon(IP_, PN_);

      std::cout << "\nNow listening for messages on " << UDPCon.IPFull << "...\n";
      std::cout << "Start UDP Session\n";
      std::ofstream outputFile(StorePath, std::ios::binary);

    }
  }
}
