#pragma once
#ifndef UTILS_H
#define UTILS_H

// Winsock
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define DEV
#ifdef DEV
#define Ask(question, holder, defaultValue){\
  std::cout << question;\
  holder = defaultValue;\
  std::cout << std::endl;\
}
#else
#define Ask(question, holder, defaultValue) {\
  std::cout << question;\
  std::cin >> holder;\
  std::cout << std::endl;\
}
#endif
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <filesystem>
#include <atomic>

enum CMDID {
  UNKNOWN = (unsigned char)0x0,//not used
  REQ_QUIT = (unsigned char)0x1,
  REQ_DOWNLOAD = (unsigned char)0x2,
  RSP_DOWNLOAD = (unsigned char)0x3,
  REQ_LISTFILES = (unsigned char)0x4,
  RSP_LISTFILES = (unsigned char)0x5,
  CMD_TEST = (unsigned char)0x20, //not used
  DOWNLOAD_ERROR = (unsigned char)0x30
};

struct SocketInfo {
  std::string PortStr;
  addrinfo* a_info, a_hints;
  SOCKET soc;
  SocketInfo();
  int CreateSocket(std::string const& IPAddress_Short, std::string const& Port, addrinfo const& hints);
  int CreateSocket(std::string const& Port, addrinfo const& hints);
  int Bind();
  int Bind(sockaddr_in*, size_t Len);
  int Connect();
};



struct Connection {
  // ******** Host byte order ***********
  uint32_t HIP32{}, NIP32{};
  uint16_t HP16{}, NP16{};
  std::string IPS{}, PS{}, IPFull{};
  SocketInfo sinfo;
  Connection() = default;
  Connection(std::string const& FullIP) { Data(FullIP); }
  Connection(uint32_t I, uint16_t P) { Data(I, P); }
  // Setup connection using IP:Port
  void Data(std::string const&);
  // Setup connection using IP string, Port string
  void Data(std::string const&, std::string const&);
  // Setup connection using IP int, Port int
  void Data(uint32_t, uint16_t, bool host = false);
  void StrToByte();
  void ByteToStr();
  void Cleanup();
  ~Connection();
  bool operator==(Connection c) {
    return c.NIP32 == NIP32 && c.NP16 == NP16;
  }
};


constexpr size_t BUF_LEN{ SO_RCVBUF };
//constexpr size_t BUF_LEN{ 10 };
constexpr uint32_t FILE_LIMIT{ 100 * 1024 };
inline std::vector<Connection> TCP_List, UDP_List;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

struct UDPMessage {
  uint32_t SessionID;
  uint8_t Flags;
  uint32_t SeqNo, FileLen, FileOffset{}, FileDataLen;
  std::vector<char> FileData;
  size_t Size(){return sizeof(uint32_t) * 5 + sizeof(uint8_t) + FileData.size();}
  void CreateBuffer(char*);
};



int StartWSA();
void EndWSA();
void StoreTCPConnection(sockaddr_in*, SOCKET soc);
void StoreUDPConnection(sockaddr_in*, SOCKET soc);

void ErrMsg(std::string const&);
std::string GetIPAddress(sockaddr_in*, bool = true);
void PrintIPAddress(std::string const& prefix, sockaddr_in* sinfo);
void ReplyFormat(std::string const& msg);
#endif


