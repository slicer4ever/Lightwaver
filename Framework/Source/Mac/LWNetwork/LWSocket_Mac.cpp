#include "LWNetwork/LWSocket.h"
#include "LWNetwork/LWProtocolManager.h"
#include "LWCore/LWText.h"
#include "LWCore/LWByteBuffer.h"
#include "LWPlatform/LWPlatform.h"
#include <arpa/inet.h>
#include <resolv.h>
#include <iostream>

uint32_t LWSocket::MakeIP(const LWText &Address) {
	in_addr Addr;
	if (inet_pton(AF_INET, (const char*)Address.GetCharacters(), &Addr) != 1) return 0;
	return LWByteBuffer::MakeHost((uint32_t)Addr.s_addr);
}

uint32_t LWSocket::LookUpAddress(const LWText &Address, uint32_t *IPBuffer, uint32_t IPBufferLen, char *Addresses, uint32_t AddressLen) {
	addrinfo hint = { AI_CANONNAME, AF_INET, 0, 0, 0, nullptr, nullptr, nullptr }, *servinfo = nullptr;
	if (getaddrinfo((const char*)Address.GetCharacters(), nullptr, &hint, &servinfo)) return 0xFFFFFFFF;
	unsigned int i = 0;
	char *AL = Addresses + AddressLen;
	for (addrinfo *p = servinfo; p; p = p->ai_next, i++) {
		if (i < IPBufferLen) IPBuffer[i] = LWByteBuffer::MakeHost((uint32_t)((sockaddr_in*)p->ai_addr)->sin_addr.s_addr);
		if (p->ai_canonname) {
			char *c = p->ai_canonname;
			for (; Addresses != AL && c; Addresses++, c++) *Addresses = *c;
		}
		if (Addresses != AL) {
			*Addresses = 0;
			Addresses++;
		}
	}
	if (Addresses != AL) {
		*Addresses = 0;
		Addresses++;
	}
	freeaddrinfo(servinfo);
	return i;
}

uint32_t LWSocket::LookUpDNSServers(uint32_t *IPBuf, uint32_t IPBufferLen) {
    __res_state s;
    res_ninit(&s);
	uint32_t Cnt = 0;
	for (; Cnt < s.nscount; Cnt++) {
		auto addr = s.nsaddr_list[Cnt];
		if (Cnt < IPBufferLen) {
			IPBuf[Cnt] = LWByteBuffer::MakeHost((uint32_t)((sockaddr_in*)&addr)->sin_addr.s_addr);
		}
	}
	return Cnt;
}

uint32_t LWSocket::CreateSocket(LWSocket &Socket, uint16_t Port, uint32_t Flag, uint32_t ProtocolID) {
    sockaddr_in Addr,lAddr;
    Addr.sin_family =  AF_INET;
    Addr.sin_port = LWByteBuffer::MakeNetwork(Port);
    Addr.sin_addr.s_addr = INADDR_ANY;
    Addr.sin_zero[0] = '\0';
	socklen_t AddrLen = sizeof(Addr);
	uint32_t UPDNoConnReset = (Flag&LWSocket::UdpConnReset) ? true : false;
	uint32_t SockBroadcast = (Flag&LWSocket::Broadcast) ? true : false;
	uint32_t TcpNoDelay = (Flag&LWSocket::TcpNoDelay) ? true : false;
	uint32_t ReuseAddr = (Flag&LWSocket::ReuseAddr) ? true : false;
	uint32_t SockID = (uint32_t)socket(AF_INET, Flag&LWSocket::Udp ? SOCK_DGRAM : SOCK_STREAM, Flag&LWSocket::Udp ? IPPROTO_UDP : IPPROTO_TCP);

	if (SockID == INVALID_SOCKET) return LWSocket::ErrSocket;
	if (Flag&LWSocket::Udp) {
		/*int32_t r = ioctl(SockID, UDP_CONNRESET, (u_long*)&UPDNoConnReset);
		if (r) {
		std::cout << "Error: " << r << std::endl;
		return LWSocket::ErrCtrlFlags;
		}*/
		if (setsockopt(SockID, SOL_SOCKET, SO_BROADCAST, (char*)&SockBroadcast, sizeof(SockBroadcast))) return LWSocket::ErrCtrlFlags;
	} else {
		if (setsockopt(SockID, IPPROTO_TCP, TCP_NODELAY, (char*)&TcpNoDelay, sizeof(TcpNoDelay))) return LWSocket::ErrCtrlFlags;
	}

	if (setsockopt(SockID, SOL_SOCKET, SO_REUSEADDR, (char*)&ReuseAddr, sizeof(ReuseAddr))) return LWSocket::ErrCtrlFlags;
	if (bind(SockID, (sockaddr*)&Addr, AddrLen)) return LWSocket::ErrBind;
	if (Flag&LWSocket::Listen) if (listen(SockID, LWSocket::MaxBacklog)) return LWSocket::ErrListen;
	if (getsockname(SockID, (sockaddr*)&lAddr, &AddrLen)) return LWSocket::ErrGetSock;

	Socket = LWSocket(SockID, ProtocolID, LWByteBuffer::MakeHost((uint32_t)lAddr.sin_addr.s_addr), LWByteBuffer::MakeHost(lAddr.sin_port), 0, 0, Flag);
	return 0;
}

uint32_t LWSocket::CreateSocket(LWSocket &Socket, uint32_t RemoteIP, uint16_t RemotePort, uint16_t LocalPort, uint32_t Flag, uint32_t ProtocolID) {
    sockaddr_in lAddr, rAddr;
    
    lAddr.sin_family =  AF_INET;
    lAddr.sin_port = LWByteBuffer::MakeNetwork(LocalPort);
    lAddr.sin_addr.s_addr = INADDR_ANY;
    lAddr.sin_zero[0] = '\0';
    
    rAddr.sin_family =  AF_INET;
    rAddr.sin_port = LWByteBuffer::MakeNetwork(RemotePort);
    rAddr.sin_addr.s_addr = LWByteBuffer::MakeNetwork(RemoteIP);
    rAddr.sin_zero[0] = '\0';
    
    socklen_t lAddrLen = sizeof(lAddr);
	socklen_t rAddrLen = sizeof(rAddr);
	uint32_t UPDNoConnReset = (Flag&LWSocket::UdpConnReset) ? true : false;
	uint32_t SockBroadcast = (Flag&LWSocket::Broadcast) ? true : false;
	uint32_t TcpNoDelay = (Flag&LWSocket::TcpNoDelay) ? true : false;
	uint32_t ReuseAddr = (Flag&LWSocket::ReuseAddr) ? true : false;

	uint32_t SockID = (uint32_t)socket(AF_INET, Flag&LWSocket::Udp ? SOCK_DGRAM : SOCK_STREAM, Flag&LWSocket::Udp ? IPPROTO_UDP : IPPROTO_TCP);

	if (SockID == INVALID_SOCKET) return LWSocket::ErrSocket;
	if (Flag&LWSocket::Udp) {
		//if (ioctl(SockID, UDP_CONNRESET, (u_long*)&UPDNoConnReset)) return LWSocket::ErrCtrlFlags;
		if (setsockopt(SockID, SOL_SOCKET, SO_BROADCAST, (char*)&SockBroadcast, sizeof(SockBroadcast))) return LWSocket::ErrCtrlFlags;
	} else {
		if (setsockopt(SockID, IPPROTO_TCP, TCP_NODELAY, (char*)&TcpNoDelay, sizeof(TcpNoDelay))) return LWSocket::ErrCtrlFlags;
	}

	if (setsockopt(SockID, SOL_SOCKET, SO_REUSEADDR, (char*)&ReuseAddr, sizeof(ReuseAddr))) return LWSocket::ErrCtrlFlags;
	if (bind(SockID, (sockaddr*)&lAddr, lAddrLen)) return LWSocket::ErrBind;
	if (connect(SockID, (sockaddr*)&rAddr, rAddrLen)) return LWSocket::ErrConnect;
	if (Flag&LWSocket::Listen) if (listen(SockID, LWSocket::MaxBacklog)) return LWSocket::ErrListen;
	if (getsockname(SockID, (sockaddr*)&lAddr, &lAddrLen)) return LWSocket::ErrGetSock;
	if (getpeername(SockID, (sockaddr*)&rAddr, &rAddrLen)) return LWSocket::ErrGetPeer;

	Socket = LWSocket(SockID, ProtocolID, LWByteBuffer::MakeHost((uint32_t)lAddr.sin_addr.s_addr), LWByteBuffer::MakeHost(lAddr.sin_port), LWByteBuffer::MakeHost((uint32_t)rAddr.sin_addr.s_addr), LWByteBuffer::MakeHost(rAddr.sin_port), Flag);
	return 0;
}

uint32_t LWSocket::CreateSocket(LWSocket &Socket, const LWText &Address, uint16_t RemotePort, uint16_t LocalPort, uint32_t Flag, uint32_t ProtocolID) {
	uint32_t AddrIP = 0;
	addrinfo Hint = { 0, AF_INET, 0, 0, 0, nullptr, nullptr, nullptr }, *Result;
	if (getaddrinfo((const char*)Address.GetCharacters(), nullptr, &Hint, &Result)) return LWSocket::ErrAddress;
	AddrIP = LWByteBuffer::MakeHost((uint32_t)((sockaddr_in*)Result->ai_addr)->sin_addr.s_addr);
	freeaddrinfo(Result);
	return CreateSocket(Socket, AddrIP, RemotePort, LocalPort, (Flag | LWSocket::Tcp)&~LWSocket::Udp, ProtocolID);
}

uint32_t LWSocket::Receive(char *Buffer, uint32_t BufferLen) const {
	return recv(m_SocketID, Buffer, BufferLen, 0);
}

uint32_t LWSocket::Receive(char *Buffer, uint32_t BufferLen, uint32_t *RemoteIP, uint16_t *RemotePort) const {
	sockaddr_in rAddr;
	socklen_t rAddrLen = sizeof(rAddr);
	uint32_t Len = recvfrom(m_SocketID, Buffer, BufferLen, 0, (sockaddr*)&rAddr, &rAddrLen);
	if (RemoteIP) *RemoteIP = LWByteBuffer::MakeHost((uint32_t)rAddr.sin_addr.s_addr);
	if (RemotePort) *RemotePort = LWByteBuffer::MakeHost(rAddr.sin_port);
	return Len;
}

uint32_t LWSocket::Send(const char *Buffer, uint32_t BufferLen) const {
	return send(m_SocketID, Buffer, BufferLen, 0);
}

uint32_t LWSocket::Send(char *Buffer, uint32_t BufferLen, uint32_t RemoteIP, uint16_t RemotePort) const {
    sockaddr_in rAddr;
    rAddr.sin_family =  AF_INET;
    rAddr.sin_port = LWByteBuffer::MakeNetwork(RemotePort);
    rAddr.sin_addr.s_addr = LWByteBuffer::MakeNetwork(RemoteIP);
    rAddr.sin_zero[0] = '\0';
	return sendto(m_SocketID, Buffer, BufferLen, 0, (sockaddr*)&rAddr, sizeof(rAddr));
}

bool LWSocket::Accept(LWSocket &Result, uint32_t ProtocolID) const {
	sockaddr_in rAddr, lAddr;
	socklen_t rAddrLen = sizeof(rAddr);
	socklen_t lAddrLen = sizeof(lAddr);
	uint32_t SockID = (uint32_t)accept(m_SocketID, nullptr, nullptr);

	uint32_t TcpNoDelay = (m_Flag&LWSocket::TcpNoDelay) ? true : false;
	if (setsockopt(SockID, IPPROTO_TCP, TCP_NODELAY, (char*)&TcpNoDelay, sizeof(TcpNoDelay))) return false;

	if (getsockname(SockID, (sockaddr*)&lAddr, &lAddrLen)) return false;
	if (getpeername(SockID, (sockaddr*)&rAddr, &rAddrLen)) return false;
	Result = LWSocket(SockID, ProtocolID, LWByteBuffer::MakeHost((uint32_t)lAddr.sin_addr.s_addr), LWByteBuffer::MakeHost(lAddr.sin_port), LWByteBuffer::MakeHost((uint32_t)rAddr.sin_addr.s_addr), LWByteBuffer::MakeHost(rAddr.sin_port), m_Flag&~LWSocket::Listen);
	return true;
}

LWSocket &LWSocket::Close(void) {
	if (m_SocketID) {
		m_Flag |= LWSocket::Closeable;
		close(m_SocketID);
		m_SocketID = 0;
	}
	return *this;
}

LWSocket::~LWSocket() {
	if (m_SocketID) close(m_SocketID);
}