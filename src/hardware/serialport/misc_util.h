#ifndef SDLNETWRAPPER_H
#define SDLNETWRAPPER_H

#if C_MODEM

#include "SDL_net.h"
#include "support.h"

#if defined LINUX || defined OS2
#define NATIVESOCKETS

#elif defined WIN32 
#define NATIVESOCKETS

#else
#endif

// Netwrapper Capabilities
#define NETWRAPPER_TCP 1
#define NETWRAPPER_TCP_NATIVESOCKET 2

Bit32u Netwrapper_GetCapabilities();


class TCPClientSocket {
	public:
	TCPClientSocket(TCPsocket source);
	TCPClientSocket(const char* destination, Bit16u port);
#ifdef NATIVESOCKETS
	Bit8u* nativetcpstruct;
	TCPClientSocket(int platformsocket);
#endif
	~TCPClientSocket();
	
	// return:
	// -1: no data
	// -2: socket closed
	// >0: data char
	Bits GetcharNonBlock();
	
	
	bool Putchar(Bit8u data);
	bool SendArray(Bit8u* data, Bitu bufsize);
	bool ReceiveArray(Bit8u* data, Bitu* size);
	bool isopen;

	bool GetRemoteAddressString(Bit8u* buffer);

	void FlushBuffer();
	void SetSendBufferSize(Bitu bufsize);
	
	// buffered send functions
	bool SendByteBuffered(Bit8u data);
	bool SendArrayBuffered(Bit8u* data, Bitu bufsize);

	private:
	TCPsocket mysock;
	SDLNet_SocketSet listensocketset;

	// Items for send buffering
	Bitu sendbuffersize;
	Bitu sendbufferindex;
	
	Bit8u* sendbuffer;
};

class TCPServerSocket {
	public:
	bool isopen;
	TCPsocket mysock;
	TCPServerSocket(Bit16u port);
	~TCPServerSocket();
	TCPClientSocket* Accept();
};


#endif

#endif //#if C_MODEM
