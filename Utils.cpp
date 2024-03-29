#include "Utils.h"
#include <iostream>
#include <sstream>


SocketInfo::SocketInfo() {
  SecureZeroMemory(&a_hints, sizeof(a_hints));
  a_info = nullptr;
  soc = INVALID_SOCKET;
}

int SocketInfo::CreateSocket(std::string const& IP_Short, std::string const& Port, addrinfo const& hints) {
  int errCode = getaddrinfo(IP_Short.data(), Port.c_str(), &hints, &a_info);
  if ((errCode) || (!a_info))
  {
    ErrMsg("getaddrinfo()");
    return errCode;
  }
  return CreateSocket(Port, hints);
}

int SocketInfo::CreateSocket(std::string const& Port, addrinfo const& hints) {
  PortStr = Port;
  a_hints = hints;

  soc = socket(a_hints.ai_family, a_hints.ai_socktype, a_hints.ai_protocol);
  if (soc == INVALID_SOCKET)
  {
    ErrMsg("socket()");
    if (a_info) { freeaddrinfo(a_info); }
    return 1;
  }
  return NO_ERROR;
}

int SocketInfo::Bind() {
  int errCode = bind(soc, a_info->ai_addr, static_cast<int>(a_info->ai_addrlen));
  if (errCode != NO_ERROR)
  {
    ErrMsg("bind()");
    closesocket(soc);
  }
  freeaddrinfo(a_info);
  return errCode;
}

int SocketInfo::Connect() {
  int errCode = connect(soc, a_info->ai_addr, static_cast<int>(a_info->ai_addrlen));
  return errCode;
}

Connection::~Connection() {
  //if (sinfo.soc != INVALID_SOCKET) {
  //  shutdown(sinfo.soc, SD_BOTH);
  //  closesocket(sinfo.soc);
  //}
}


void Connection::Cleanup() {
  if (sinfo.soc != INVALID_SOCKET) {
    closesocket(sinfo.soc);
  }
}


void Connection::Data(std::string const& p_IPFull) {
  IPFull = p_IPFull;
  auto colon = p_IPFull.find(':');
  IPS = p_IPFull.substr(0, colon);
  PS = p_IPFull.substr(colon + 1);
  StrToByte();

}

void Connection::Data(std::string const& p_IP, std::string const& p_Port) {
  IPS = p_IP;
  PS = p_Port;
  IPFull = IPS + ":" + PS;
  StrToByte();

}

void Connection::Data(uint32_t p_IP, uint32_t p_P, bool host) {
  if (host) {
    HIP32 = p_IP;
    HP16 = p_P;
    NIP32 = htonl(HIP32);
    NP16 = htons(HP16);
  }
  else {
    NIP32 = p_IP;
    NP16 = p_P;
    HIP32 = ntohl(NIP32);
    HP16 = ntohs(NP16);
  }
  ByteToStr();
}

void Connection::StrToByte() {
  inet_pton(AF_INET, IPS.c_str(), &HIP32);
  HP16 = static_cast<uint16_t>(std::stoi(PS));
  NIP32 = ntohl(HIP32);
  NP16 = ntohs(HP16);

}

void Connection::ByteToStr() {
  struct in_addr ipAddr;
  ipAddr.s_addr = NIP32;
  // Convert the IP address to a string
  char str[INET_ADDRSTRLEN];
  PS = std::to_string(NP16);
  if (inet_ntop(AF_INET, &NIP32, str, INET_ADDRSTRLEN) != NULL) {
    IPS = str;
    IPFull = IPS + ":" + PS;
  }
}

void StoreTCPConnection(sockaddr_in* info, SOCKET soc) {
  std::string clientIP = GetIPAddress(info);
  PrintIPAddress("Client", info);
  Connection conn_(clientIP);
  if (std::find_if(TCP_List.begin(), TCP_List.end(), [clientIP](Connection c) {
    return c.IPS == clientIP;
    }) == TCP_List.end()) {
    conn_.sinfo.soc = soc;
    TCP_List.push_back(conn_);
#if defined(DEV)
    std::cout << soc << std::endl;
#endif
  }
}

void StoreUDPConnection(sockaddr_in* info, SOCKET soc) {
  std::string clientIP = GetIPAddress(info);
  PrintIPAddress("Client", info);
  Connection conn_(clientIP);
  if (std::find_if(UDP_List.begin(), UDP_List.end(), [clientIP](Connection c) {
    return c.IPS == clientIP;
    }) == UDP_List.end()) {
    conn_.sinfo.soc = soc;
    UDP_List.push_back(conn_);
#if defined(DEV)
    std::cout << soc << std::endl;
#endif
  }
}

int StartWSA() {
  WSADATA wsa;
  SecureZeroMemory(&wsa, sizeof(wsa));
  int errCode = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (errCode != NOERROR) {
    ErrMsg("WSAStartUp()");
  }
  return errCode;
}

void EndWSA() {
  WSACleanup();
}

std::string GetIPAddress(sockaddr_in* sinfo, bool Full) {
  char IP[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(sinfo->sin_addr), IP, INET_ADDRSTRLEN);
  if(!Full){return {IP}; }
  std::stringstream ss(IP);
  ss << IP << ":" << std::to_string(sinfo->sin_port);
  return ss.str();
}
void PrintIPAddress(std::string const& prefix, sockaddr_in* sinfo) {
  std::string IP = GetIPAddress(sinfo, false);
  printf("\n%s IP Address: %s\n", prefix.c_str(), IP.c_str());
  printf("%s Port Number: %hu\n", prefix.c_str(), sinfo->sin_port);
}

void ErrMsg(std::string const& err) {
  std::cerr << err << " failed. WSA_EC(" << WSAGetLastError() << ")";
#if defined(DEV)
  std::cout << " S";
#endif 
  std::cout << std::endl;
}

void ReplyFormat(std::string const& msg) {
  std::cout << "==========RECV START==========\n\n";
  std::cout << msg << "\n";
  std::cout << "==========RECV END==========" << std::endl;
}
