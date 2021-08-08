#include "tcp_mini.h"

// NOTE: std includes here (no c++ includes, they will break)
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <unistd.h>
#include <fcntl.h>
#if defined(_WIN32)
#include <winsock2.h>
int poll(WSAPOLLFD a[], ULONG b, INT c)
{
	return WSAPoll(a, b, c);
}
int ioctl(SOCKET a, __LONG32 b, u_long* c)
{
	return ioctlsocket(a, b, c);
}
#define socklen_t int
#elif defined (__linux__)
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <poll.h>
#include <netdb.h>

#define SOCKET int
#define INVALID_SOCKET -1
#else
#error "unsupported platform"
#endif

#pragma GCC diagnostic ignored "-Wsign-compare"
#define TCP_MINI_ALWAYS_INLINE gnu::always_inline

[[TCP_MINI_ALWAYS_INLINE]]
inline void* operator new[](std::size_t a)
{
	return malloc(a);
}
[[TCP_MINI_ALWAYS_INLINE]]
inline void operator delete(void* a, std::size_t)
{
    free(a); //< there is no check here on purpose, please catch this issue elsewhere
}

namespace
{
	template<typename T>
	T max(T a, T b)
	{
		  if(a > b)
		  {
			  return a;
		  }
		  return b;
	}
	// NOTE: T& is a reference to adress "decay issue" (where T[] becomes T*) as explained here http://www.cplusplus.com/articles/D4SGz8AR/
	template<typename T, int A>
	int length(T (&a)[A])
	{
		return sizeof a/sizeof *a;
	}
	// NOTE: don't use directly (used as template for other functions of same name)
#define _TM_GET_INDEX_OF_FIRST_IN_BITFIELD(a, b) \
	int c = 0;\
	while(a != 0) \
	{ \
		b d = a & 1; \
		if(d != 0) \
		{ \
			return c; \
		} \
		++c; \
		a = a >> 1; \
	} \
	return sizeof(b) * 8; //< not in bitfield

	template<typename T>
	int get_index_of_first_in_bitfield(T a)
	{
		_TM_GET_INDEX_OF_FIRST_IN_BITFIELD(a, T);
	}

	// a means index here
	template<typename T>
	T remove_from_bitfield(int a, T b)
	{
	  T c = 1 << a;
	  return b & ~c;
	}
}

#define MAX_MESSAGE_T struct { char b[TCP_MINI_MAX_MESSAGE_SIZE]; } //< NOTE: b to avoid confusion (don't use this directly)

#define TIMEOUT max((int)CLOCKS_PER_SEC / 2, 1) //< i.e. "wait max half a second"

//*********************************************************

namespace
{
	template<typename T>
	void copy_array_except_element_at(int length, T* destination, T* source, int index)
	{
		printf("length %i\n", length);

		int numElementsBeforeIndex = index + 1 - 1;

		int numElementsAfterIndex = length - (index + 1);

		printf("numElementsBeforeIndex %i\n", numElementsBeforeIndex);
		printf("numElementsAfterIndex %i\n", numElementsAfterIndex);

		int a = 0;
		if(numElementsBeforeIndex > 0)
		{
			memcpy(destination, source, sizeof(T) * numElementsBeforeIndex);
			a += numElementsBeforeIndex;
		}
		if(numElementsAfterIndex > 0)
		{
			memcpy(destination + a, &source[index] + 1, numElementsAfterIndex);
			//a += numElementsAfterIndex;
		}
	}

	using ip_address_or_hostname_t = char[254];

	struct scout_t
	{
		int port;
		SOCKET socket;
		ip_address_or_hostname_t otherIpAddressOrHostname;
		// NOTE: if processing previous message timed out but size was read..
		//       .. will store this here so rest of message can still be..
		//       .. read
		//       if sizeOfTimedOutMessage == 0, last message processed..
		//       .. "cleanly" (i.e. no time out)
		int sizeOfTimedOutMessage = 0;
	};

#if defined TM_MAXSCOUTS
	scout_t scouts[TM_MAXSCOUTS];
#else
	scout_t* scouts;
#endif
	int numScouts;

	struct match_t
	{
		SOCKET socket;
		int port;
		// NOTE: otherIpAddressOrHostnameses == other <ip address|hostname>s (i.e. convention to represent that each element can be one of multiple)
#if defined TM_MAXCONNECTIONS
		SOCKET otherSockets[TM_MAXCONNECTIONS]; //< per scout, socket on which to listen to messages from scout
		ip_address_or_hostname_t otherIpAddressOrHostnameses[TM_MAXCONNECTIONS];
		int sizeOfTimedOutMessagePerConnection[TM_MAXCONNECTIONS];
#else
		SOCKET* otherSockets = NULL; //< per scout, socket on which to listen to messages from scout
		ip_address_or_hostname_t* otherIpAddressOrHostnameses = NULL;
		// NOTE: per other socket..
		//       .. if processing previous message timed out but size was..
		//          .. read will store this here so rest of message can..
		//          .. still be read
		//          if sizeOfTimedOutMessagePerConnection[..] == 0, last..
		//          .. message processed "cleanly" (i.e. no time out)
		int* sizeOfTimedOutMessagePerConnection = NULL;
#endif
		int numConnections = 0;
	};

#if defined TM_MAXMATCHES
	match_t matches[TM_MAXMATCHES];
#else
	match_t* matches;
#endif
	int numMatches;

	// NOTE: for scouts,matches.. index is not suitable for use as index..
	//       .. elsewhere, as index to an element might change when another..
	//       .. element is removed
	//       i.e. these arrays are kept "smallest fit" (no gaps)

	// NOTE: index must be valid
	void remove_scout_at(int index)
	{
#if defined TM_MAXSCOUTS
		int b = numScouts - (index + 1); //< i.e. # scouts after scout @ index
		memcpy(&scouts[index], &scouts[index] + 1, sizeof(scout_t) * b);
#else
		if(numScouts - 1 == 0)
		{
			delete scouts;
			scouts = NULL;
		}
		else
		{
			scout_t* b = scouts;
			scouts = (scout_t*)new char[sizeof(scout_t) * (numScouts - 1)];

			copy_array_except_element_at(numScouts, scouts, b, index);

			delete b;
		}
#endif

		--numScouts;
	}

	void add_connection(match_t* match)
	{
#if !defined TM_MAXCONNECTIONS
		SOCKET* a = match->otherSockets;
		match->otherSockets = (SOCKET*)new char[sizeof(SOCKET) * (match->numConnections + 1)];

		memcpy(match->otherSockets, a , sizeof(SOCKET) * match->numConnections);

		delete a;

		ip_address_or_hostname_t* b = match->otherIpAddressOrHostnameses;

		match->otherIpAddressOrHostnameses = (ip_address_or_hostname_t*)new char[sizeof(ip_address_or_hostname_t) * (match->numConnections + 1)];

		memcpy(match->otherIpAddressOrHostnameses, b, sizeof(ip_address_or_hostname_t) * match->numConnections);

		delete b;

		int* c = match->sizeOfTimedOutMessagePerConnection;

		match->sizeOfTimedOutMessagePerConnection = (int*)new char[sizeof(int) * (match->numConnections + 1)];

		memcpy(match->sizeOfTimedOutMessagePerConnection, c, sizeof(int) * match->numConnections);

		delete c;
#endif
		++match->numConnections;
	}

	// NOTE: index must be valid
	void remove_connection_at(match_t* match, int index)
	{
#if (!defined TM_MAXCONNECTIONS)
		SOCKET* a = match->otherSockets;
		ip_address_or_hostname_t* b = match->otherIpAddressOrHostnameses;
		int* c = match->sizeOfTimedOutMessagePerConnection;

		if(match->numConnections - 1 > 0)
		{
			match->otherSockets = (SOCKET*)new char[sizeof(SOCKET) * (match->numConnections - 1)];
			match->otherIpAddressOrHostnameses = (ip_address_or_hostname_t*)new char[sizeof(ip_address_or_hostname_t) * (match->numConnections - 1)];
			match->sizeOfTimedOutMessagePerConnection = (int*)new char[sizeof(int) * (match->numConnections - 1)];

			copy_array_except_element_at(match->numConnections, match->otherSockets, a, index);
			copy_array_except_element_at(match->numConnections, match->otherIpAddressOrHostnameses, b, index);
			copy_array_except_element_at(match->numConnections, match->sizeOfTimedOutMessagePerConnection, c, index);
		}

		delete a;
		delete b;
		delete c;

		if(match->numConnections - 1 == 0)
		{
			match->otherSockets = NULL;
			match->otherIpAddressOrHostnameses = NULL;
			match->sizeOfTimedOutMessagePerConnection = NULL;
		}
#else
		if(match->numConnections - 1 > 0)
		{
			int a = match->numConnections - (index + 1);
			memcpy(&match->otherSockets[index], &match->otherSockets[index+1], a);
			memcpy(&match->otherIpAddressOrHostnameses[index], &match->otherIpAddressOrHostnameses[index+1], a);
			memcpy(&match->sizeOfTimedOutMessagePerConnection[index], &match->sizeOfTimedOutMessagePerConnection[index+1], a);
		}
#endif

		--match->numConnections;
	}
}

//*********************************************************

//#if (!defined TCP_MINI_SCOUT_ONLY)
namespace
{
	void (*on_scout_connected)(int port, char* ipAddress);
	void (*on_scout_hung_up)(int port, char* ipAddress);
}

extern "C" void tm_set_on_scout_connected(void(*a)(int, char*))
{
	on_scout_connected = a;
}
extern "C" void tm_unset_on_scout_connected()
{
	on_scout_connected = NULL;
}
extern "C" void tm_set_on_scout_hung_up(void(*a)(int, char*))
{
	on_scout_hung_up = a;
}
extern "C" void tm_unset_on_scout_hung_up()
{
	on_scout_hung_up = NULL;
}

extern "C" int tm_become_a_match(int port)
{
	for(int i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			return -1; //< already a match on that port
		}
	}

#if defined TM_MAXMATCHES
	if(numMatches == TM_MAXMATCHES)
	{
		return -1;
	}
#endif

	match_t tmp;
	match_t* match = &tmp;

	match->port = port;
#if defined(__linux__)
	match->socket = socket(AF_INET, SOCK_STREAM /* TCP */ | SOCK_NONBLOCK, 0);
#else // #elif defined(_WIN32)
	match->socket = socket(AF_INET, SOCK_STREAM /* TCP */, 0);
	//printf("WSAGetLastError() %i\n", WSAGetLastError());
#endif
	if(match->socket == INVALID_SOCKET)
	{
		// NOTE: I am not sure if this is likely to fail, though EACCES seems like it "could" happen
		// NOTE: it seems to not make much sense to stop_being_a_match (i.e...
		//       .. close down), when there is not even a socket up
		return 0;
	}

#if defined(_WIN32)
	u_long b = 1;
	ioctlsocket(match->socket, FIONBIO, &b);
#endif

#if (!defined TM_MAXMATCHES)
	match_t* a = matches;
	matches = (match_t*)new char[sizeof(match_t) * (numMatches + 1)];
	if(a != NULL)
	{
		memcpy(matches, a, sizeof(match_t) * numMatches);
		delete a;
	}
#endif

	++numMatches;
	matches[numMatches - 1] = *match;
	match = &matches[numMatches - 1];

	// NOTE: SO_REUSEADDR "makes TCP less reliable" according to https://www.man7.org/linux/man-pages/man7/ip.7.html
#if defined(__linux)
	int d = 1;
#else
	char d = 1;
#endif
	setsockopt(match->socket, SOL_SOCKET, SO_REUSEADDR, &d, sizeof(int));

	sockaddr_in c;
	c.sin_family = AF_INET;
	c.sin_port = htons(port);
	c.sin_addr.s_addr = INADDR_ANY;

	//printf("%i\n", port);

	if(bind(match->socket, (struct sockaddr*) &c, sizeof c) != 0)
	{
		// NOTE: I expect that EADDRINUSE is reasonable here..
		tm_stop_being_a_match(port); //< not sure what will happen if this fails..
		return 0;
	}

	listen(match->socket, 1); //< errors that can occur here should be caught by bind

	return 1;
}

extern "C" int tm_stop_being_a_match(int port)
{
	int i;
	for(i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			break;
		}
	}
	if(i == numMatches)
	{
		return -1;
	}

	while(matches[i].numConnections > 0)
	{
		tm_disconnect_scout(port, matches[i].otherIpAddressOrHostnameses[matches[i].numConnections - 1]);
		//fputs("tm_disconnect_scout\n", stdout);
	}

#if defined(__linux__)
	close(matches[i].socket);
#else
	closesocket(matches[i].socket);
#endif

#if defined TM_MAXMATCHES
	int c = numMatches - (i + 1); //< i.e. # matches after match a
	memcpy(&matches[i], &matches[i] + 1, sizeof(match_t) * c);
#else

	if(numMatches - 1 == 0)
	{
		delete matches;
		matches = NULL;
	}
	else
	{
		match_t* d = matches;
		matches = (match_t*)new char[sizeof(match_t) * (numMatches - 1)];

		copy_array_except_element_at(numMatches, matches, d, i);

		delete d;
	}
#endif
	--numMatches;

	return 1;
}

extern "C" int tm_disconnect_scout(int port, char* ipAddressOrHostname)
{
	int i;
	for(i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			break;
		}
	}
	if(i == numMatches)
	{
		return -1;
	}

	int j;
	for(j = 0; j < matches[i].numConnections; ++j)
	{
		if(strcmp(matches[i].otherIpAddressOrHostnameses[j], ipAddressOrHostname) == 0)
		{
			break;
		}
	}
	if(j == matches[i].numConnections)
	{
		return -1;
	}

#if defined(__linux__)
	// NOTE: I think EIO should not happen (as multi threaded is not supported for tcp-mini)
	//       perhaps EINTR can happen..?
	// TODO: write test to see what happens when EINTR happens while in close call (not sure how to do this..)
	close(matches[i].otherSockets[j]);
#else
	closesocket(matches[i].otherSockets[j]);
#endif

	remove_connection_at(&matches[i], j);

	return 1;
}
//#endif

//#if (!defined TCP_MINI_MATCH_ONLY)
namespace
{
	void(*on_match_hung_up)(tm_match_blob_t);
}

extern "C" void tm_set_on_match_hung_up(void(*a)(tm_match_blob_t))
{
	on_match_hung_up = a;
}
extern "C" void tm_unset_on_match_hung_up()
{
	on_match_hung_up = NULL;
}

extern "C" int tm_connect_to_match(tm_match_blob_t a)
{
	for(int i = 0; i < numScouts; ++i)
	{
		if(scouts[i].port == a.port && strcmp(scouts[i].otherIpAddressOrHostname, a.hostname) == 0)
		{
			return -1; //< already connected to that match
		}
	}

#if defined TM_MAXSCOUTS
	if(numScouts == TM_MAXSCOUTS)
	{
		return -1;
	}
#endif

	scout_t tmp;
	scout_t* scout = &tmp;

	scout->port = a.port;
	strcpy(scout->otherIpAddressOrHostname, a.hostname);
	scout->socket = socket(AF_INET, SOCK_STREAM /* TCP */, 0);

	if(scout->socket == INVALID_SOCKET)
	{
		// NOTE: I am not sure if this is likely to fail, though EACCES seems like it "could" happen
		// it seems to not make much sense to try to disconnect, when there is not even a socket up
		return -1;
	}

#if (!defined TM_MAXSCOUTS)
	scout_t* b = scouts;
	scouts = (scout_t*)new char[sizeof(scout_t) * (numScouts + 1)];

	if(b != NULL)
	{
		memcpy(scouts, b, sizeof(scout_t) * numScouts);
		delete b;
	}
#endif

	++numScouts;
	scouts[numScouts - 1] = *scout;
	scout = &scouts[numScouts - 1];

	// NOTE: SO_REUSEADDR "makes TCP less reliable" according to https://www.man7.org/linux/man-pages/man7/ip.7.html
#if defined(__linux__)
	int c = 1;
#else
	char c = 1;
#endif
	setsockopt(scout->socket, SOL_SOCKET, SO_REUSEADDR, &c, sizeof c);

	// if a is set via a.ipAddress, hostname should catch that case
	hostent* e = gethostbyname(a.hostname);
	if(e == NULL)
	{
		tm_disconnect_from_match(a); //< even though there is no "connection" here yet, the socket still has to be closed. That's why this call is still needed.
        return 0; //< failed to resolve hostname
	}

	sockaddr_in d;
	memcpy((void*)&d.sin_addr.s_addr, (void*)e->h_addr, e->h_length);
	d.sin_family = AF_INET;
	d.sin_port = htons(a.port);

	//printf("%s\n", a.hostname);
	//printf("%i\n", a.port);
	if(connect(scout->socket, (sockaddr*)&d, sizeof d) != 0)
	{
		tm_disconnect_from_match(a);
		return 0;
	}

	// once connected, enable O_NONBLOCK so that reading wont wait for data
#if defined(__linux__)
	fcntl(scout->socket, F_SETFL, O_NONBLOCK);
#else
	u_long g = 1;
	ioctlsocket(scout->socket, FIONBIO, &g);
#endif

	return 1;
}

extern "C" int tm_disconnect_from_match(tm_match_blob_t a)
{
	int i;
	for(i = 0; i < numScouts; ++i)
	{
		if(scouts[i].port == a.port && strcmp(scouts[i].otherIpAddressOrHostname, a.hostname) == 0)
		{
			break;
		}
	}
	if(i == numScouts)
	{
		return -1;
	}

#if defined(__linux__)
	close(scouts[i].socket);
#else
	closesocket(scouts[i].socket);
#endif

	remove_scout_at(i);

	return 1;
}
//#endif

//*********************************************************

namespace
{
	// NOTE: a == size of "header" (i.e. "tm_message_t struct")
	//       b == size of "payload"
	int is_message_size_valid(int a, int b)
	{
		if(a < sizeof(int))
		{
			return 0; //< message without initial "type" variable is not allowed
		}

		if((a + b) > TCP_MINI_MAX_MESSAGE_SIZE)
		{
			return 0;
		}

		return 1;
	}

	// NOTE: will assume message is valid (see is_message_size_valid)
	void send_message(SOCKET socket, tm_message_t* a, int b, void* c, int d)
	{
	    int e = b+d;
	    int f = sizeof(int)+e; //< prepend the size of the message before the message

		char* buffer = new char[f];
		int g = 0;
		memcpy(&buffer[g], &e, sizeof(int));
		g += sizeof(int);
		memcpy(&buffer[g], a, b);
		g += b;
		memcpy(&buffer[g], c, d);
		//g += d;

#if defined(__linux__)
		// NOTE: should loop until all bytes are written (i.e. if write returns "<f" or -1)..?
		write(socket, buffer, f);
#else
		send(socket, buffer, f, 0);
#endif

		//printf("sent message of %i bytes (including size bytes)\n", g);

		delete buffer;
	}

	int num_bytes_available_for_reading(SOCKET socket)
	{
#if defined(__linux__) //< # bytes left available for reading
		  int a;
#else
		  u_long a;
#endif
		  ioctl(socket, FIONREAD, &a);

		  return a;
	}

	// NOTE: will return once no more data is available for reading
	//       if part of a message is already available.. will wait until..
	//       .. the rest is available as well for a maximum duration of..
	//       .. maxWaitInClocks (i.e. will wait for every "chunk", where the..
	//       .. size bytes make up the first chunk, and the message makes up..
	//       .. the second chunk)
	// NOTE: a-> first parameter to on_receive
	// NOTE: numMessagesProcessed may not be NULL
	// NOTE: if time till desired number of bytes are available exceeds..
	//       .. maxWaitInClocks..
	//          .. will set *bTimedOut == 1
	//          .. if size bytes were read already, will write the size of..
	//          .. the timed out messages to *sizeOfMessageThatTimedOut
	// NOTE: if *sizeOfMessageThatTimedOut > 0.. will "continue were left..
	//       .. off" (i.e. will not attempt to re-read size of message)
	// NOTE: e.g. if an "incomplete message" (i.e. without "size bytes") is..
	//       .. sent, will time out
	void process_messages(SOCKET socket, int maxMessages, int maxWaitInClocks, void(*on_receive)(void*, tm_message_t*, int), void* a, int* bTimedOut, int* numMessagesProcessed, int* sizeOfMessageThatTimedOut)
	{
// NOTE: m == # bytes that must be readable
//       n -> "what to do" if time out
#define WAIT_UNTIL_CAN_READ(m, n) \
		{ \
			clock_t l = clock(); \
			while(1) \
			{ \
				if(num_bytes_available_for_reading(socket) >= m) \
				{ \
					break; \
				} \
				if(clock() - l > maxWaitInClocks) \
				{ \
					n \
					*bTimedOut = 1; \
					return; \
				} \
			} \
		}
// NOTE: m == # bytes that must be readable
#define DO_IF_CANNOT_READ(l, m) \
		{ \
			if(num_bytes_available_for_reading(socket) < m) \
			{ \
				l \
			} \
		}

		*numMessagesProcessed = 0;

		while(1)
		{
			if(maxMessages == -1 || *numMessagesProcessed < maxMessages)
			{
#if defined(_WIN32)
				DO_IF_CANNOT_READ(break;, 1); //< break if there is no more readable data
#endif

				// first sizeof(int) bytes store the size of the message.
				int c;

#if defined(_WIN32)
				WSABUF e;
				DWORD f = MSG_PARTIAL;
				DWORD g;
#endif

				if(*sizeOfMessageThatTimedOut > 0)
				{
					c = *sizeOfMessageThatTimedOut;
					*sizeOfMessageThatTimedOut = 0;
				}
				else
				{
					// IDEA: alternatively.. if < sizeof(int) bytes are..
					//       .. available.. disconnect the scout..? (i.e...
					//       .. assume that message is incomplete) if..
					//       .. arriving takes "that long"
					WAIT_UNTIL_CAN_READ(sizeof(int),);

#if defined(__linux__)
					// NOTE: should never return -1..?
					// NOTE: not sure if read here can be interrupted by signal (perhaps loop to make sure read finished..?)
					/*int e =*/ read(socket, &c, sizeof(int));
#else
					e.buf = (char*)&c;
					e.len = sizeof(int);
					WSARecv(socket, &e, 1, &g, &f, NULL, NULL);
#endif
				}

				//printf("c %i\n", c);

				char* d = new char[c];

				// IDEA: read message payload in "chunks" (i.e. read at..
				//       .. least sizeof(int) bytes every "time" (if not..
				//       .. atleast that many bytes can be read.. disconnect)
				// NOTE: reading sizeof(int) chunks every time is to prevent..
				//       .. a scout stalling a match (or vice versa) by..
				//       .. sending an incomplete message
				// struct match_t
				// {
				//   //...
				//   //~sizeOfTimedOutMessage~
				//   int numBytesOfNextMessageLeftToRead; //< i.e. initially "c" (i.e. size bytes)
				//   void* bufferForNextMessage;
				// };
				//int f = read(socket, d, /*numBytesOfNextMessageLeftToRead*/);
				//if(/*numBytesOfNextMessageLeftToRead*/ == f)
				//{
				//	// i.e. done
				//}
				//else if(f < sizeof(int))
				//{
				//	// disconnect scout (i.e. there are more bytes left to..
				//	// .. read, but only sizeof(int) bytes arrived since last poll (i.e. too slow)
				//}
				//
				///*numBytesOfNextMessageLeftToRead*/ -= f;

				WAIT_UNTIL_CAN_READ(c, *sizeOfMessageThatTimedOut = c;);
#if defined(__linux__)
				// NOTE: should never return -1..?
				// NOTE: see "IDEA:" above -> i.e. to read as much as..
				//       .. possible call read once w. non blocking..?)
				//       then subtract f from /*numBytesOfNextMessageLeftToRead*/..?
				/*int f =*/ read(socket, d, c);
#else
				e.buf = d;
				e.len = c;
				//f = 0; //< set to MSG_PARTIAL if another part of the message needs to be processed after
				WSARecv(socket, &e, 1, &g, &f, NULL, NULL);
#endif

				on_receive(a, (tm_message_t*)d, c); //< "on receive"

				delete d;

				++*numMessagesProcessed;
			}
			else
			{
				break; //< can't process any more messages this poll
			}
		}
#undef DO_IF_CANNOT_READ
#undef WAIT_UNTIL_CAN_READ

		*bTimedOut = 0;
	}
}

//#if (!defined TCP_MINI_SCOUT_ONLY)
namespace
{
	void(*on_receive_from_scout)(int port, char* ipAddressOrHostname, tm_message_t* message, int a);
}

extern "C" void tm_set_on_receive_from_scout(void(*a)(int, char*, tm_message_t*, int))
{
	on_receive_from_scout = a;
}
extern "C" void tm_unset_on_receive_from_scout()
{
	on_receive_from_scout = NULL;
}

extern "C" int tm_send_to_scout(int port, char* ipAddressOrHostname, tm_message_t* a, int b, void* c, int d)
{
	match_t* match = NULL;
	for(int i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			match = &matches[i];
		}
	}
	if(match == NULL)
	{
		return -1;
	}

	int i;
	for(i = 0; i < match->numConnections; ++i)
	{
		if(strcmp(match->otherIpAddressOrHostnameses[i], ipAddressOrHostname) == 0)
		{
			break;
		}
	}
	if(i == match->numConnections)
	{
		return -1; //< ipAddress is not "one of connected"
	}

	if(is_message_size_valid(b, d) == 0)
	{
		return 0;
	}

	send_message(match->otherSockets[i], a, b, c, d);

	return 1;
}

extern "C" int tm_send_to_scouts(int port, tm_message_t* a, int b, void* c, int d)
{
	match_t* match = NULL;
	for(int i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			match = &matches[i];
		}
	}
	if(match == NULL)
	{
		return -1;
	}

	if(is_message_size_valid(b, d) == 0)
	{
		return 0;
	}

	int i;
	for(i = 0; i < match->numConnections; ++i)
	{
		send_message(match->otherSockets[i], a, b, c, d);
	}

	return 1;
}
//#endif

//#if (!defined TCP_MINI_MATCH_ONLY)
namespace
{
	void(*on_receive_from_match)(struct tm_match_blob_t a, tm_message_t* message, int b);
}

extern "C" void tm_set_on_receive_from_match(void(*a)(struct tm_match_blob_t, tm_message_t*, int))
{
	on_receive_from_match = a;
}
extern "C" void tm_unset_on_receive_from_match()
{
	on_receive_from_match = NULL;
}

extern "C" int tm_send_to_match(struct tm_match_blob_t a, tm_message_t* b, int c, void* d, int e)
{
	int i;
	for(i = 0; i < numScouts; ++i)
	{
		//printf("scouts[%i].otherIpAddressOrHostname %s\n", i, scouts[i].otherIpAddressOrHostname);
		//printf("a.hostname %s\n", a.hostname);
		if(scouts[i].port == a.port && strcmp(scouts[i].otherIpAddressOrHostname, a.hostname) == 0)
		{
			break;
		}
	}
	if(i == numScouts)
	{
		return -1;
	}

	if(is_message_size_valid(c, e) == 0)
	{
		return 0;
	}

	send_message(scouts[i].socket, b, c, d, e);

	return 1;
}
//#endif

namespace
{
	enum EPollEvent
	{
#if defined(__linux__)
		EPollEvent_POLLIN = 0,
		EPollEvent_POLLRDNORM = 6,
		EPollEvent_POLLRDBAND = 7,
		EPollEvent_POLLPRI = 1,
		EPollEvent_POLLOUT = 2,
		EPollEvent_POLLWRNORM = 8,
		EPollEvent_POLLERR = 3,
		EPollEvent_POLLHUP = 4,
		EPollEvent_POLLNVAL = 5,
		EPollEvent_POLLWRBAND = 9,
		EPollEvent_POLLMSG = 10,
		EPollEvent_POLLREMOVE = 12,
		EPollEvent_POLLRDHUP = 13
#else
		EPollEvent_POLLRDNORM = 8,
		EPollEvent_POLLRDBAND = 9,
		EPollEvent_POLLIN = EPollEvent_POLLRDNORM | EPollEvent_POLLRDBAND,
		EPollEvent_POLLPRI = 10,
		EPollEvent_POLLWRNORM = 4,
		EPollEvent_POLLOUT = EPollEvent_POLLWRNORM,
		EPollEvent_POLLERR = 0,
		EPollEvent_POLLHUP = 1,
		EPollEvent_POLLNVAL = 2,
		EPollEvent_POLLWRBAND = 5
#endif
	};

	struct process_next_event_arg_t
	{
		int a; //< one of EPollEvent
		// NOTE: a is one of EPollEvent
		//       b is a "pointer to data" for arguments (see c)
		//void(*b)(int a, void* b);
		void(*b)(int, void*);
		void* c; //< gets passed to b (i.e. b(.., c))
	};
	// NOTE: a is &<pollfd>.revents
	//       b is array of process_next_event_arg_t structures
	//       c is length(b)
	void process_next_event(short* a, process_next_event_arg_t* b, int c)
	{
		int d = get_index_of_first_in_bitfield(*a);

		for(int e = 0; e < c; ++e)
		{
#if defined(_WIN32)
			if(b[e].a == EPollEvent_POLLIN && (d == EPollEvent_POLLRDNORM || d == EPollEvent_POLLRDBAND))
			{
				b[e].b(EPollEvent_POLLIN, b[e].c);
			}
#endif
			if(b[e].a == d)
			{
				b[e].b(b[e].a, b[e].c);
			}
			//printf("b[e].a %i\n", b[e].a);
			//printf("d %i\n", d);
		}

		*a = remove_from_bitfield(d, *a);
	}

	// NOTE: i.e. for detecting connection requests
	void poll_for_match(match_t* match)
	{
		using a_t = struct { match_t* match; };
		a_t a;
		a.match = match;

		process_next_event_arg_t b;
		b.a = EPollEvent_POLLIN;
		b.c = &a;
		b.b = [](int a, void* b)
		{
			a_t* c = (a_t*)b;

			sockaddr_in f;
			socklen_t d = sizeof f;
			SOCKET e;

#if defined TM_MAXCONNECTIONS
			int bRefuse = c->match->numConnections == length(c->match->otherSockets) ? 1 : 0;

#if defined(__linux__)
			e = accept(c->match->socket, (sockaddr*)&f, &d);
			if(bRefuse == 1)
			{
				close(e);
				return;
			}
#else
			e = WSAAccept(c->match->socket, (sockaddr*)&f, &d, [](LPWSABUF a, LPWSABUF b, LPQOS c, LPQOS d, LPWSABUF e, LPWSABUF f, GROUP* g, DWORD_PTR h)
				{
					int* bRefuse = (int*)h;
					if(*bRefuse == 1)
					{
						return CF_REJECT;
					}
					else
					{
						return CF_ACCEPT;
					}
				}, (DWORD_PTR)&bRefuse);
			if(bRefuse == 1)
			{
				return;
			}
#endif
#else
			e = accept(c->match->socket, (sockaddr*)&f, &d);
#endif

#if defined(__linux__)
			// NOTE: documentation says that d might be greater than sizeof ~c~f..? now (should this be checked for?)
#endif
			if(e == INVALID_SOCKET)
			{
				// TODO: make sure that the errors that can be thrown here are managed..?
				return;
			}

			hostent* g;
#if defined(__linux__)
			g = gethostbyaddr((void*)&f.sin_addr, sizeof f.sin_addr, AF_INET);
#else
			g = gethostbyaddr((char*)&f.sin_addr, sizeof f.sin_addr, AF_INET);
#endif
			if(g == NULL)
			{
				// kick the player from the lobby (can't obtain a readable ip address)
#if defined(__linux__)
				close(e);
#else
				closesocket(e);
#endif
				return;
			}

#if defined(__linux__)
			fcntl(e, F_SETFL, O_NONBLOCK);
#else
			u_long h = 1;
			ioctlsocket(e, FIONBIO, &h);
#endif

			add_connection(c->match);

			c->match->otherSockets[c->match->numConnections - 1] = e;
			if(g->h_name != NULL)
			{
				strcpy(c->match->otherIpAddressOrHostnameses[c->match->numConnections - 1], g->h_name);
			}
			else
			{
				// i.e. copy addr as readable string
				//strcpy(c->match->otherIpAddressOrHostnameses[c->match->numConnections - 1], inet_ntoa(f.sin_addr));
				c->match->otherIpAddressOrHostnameses[c->match->numConnections - 1][0] = '\0';
			}
			c->match->sizeOfTimedOutMessagePerConnection[c->match->numConnections - 1] = 0;

			if(on_scout_connected != NULL)
			{
				// NOTE: it should be ok if tm_disconnect_scout is called from on_scout_connected
				on_scout_connected(c->match->port, c->match->otherIpAddressOrHostnameses[c->match->numConnections - 1]);
			}
		};

		pollfd c;
		c.fd = match->socket;
		c.events = POLLIN;
		int e = poll(&c, 1, 0); //< NOTE: at the time of writing this, there is only one connection request accepted at a time
		while(!(e == -1 || e == 0) && c.revents != 0) //< NOTE: poll can fail if interrupted (EINTR), not sure though if this applies if timeout value is 0
		{
			process_next_event(&c.revents, &b, 1);
		}
	}

	// NOTE: will return 0 if no more messages left to process or 1 if there are
	// NOTE: a == index to scout
	// NOTE: bHungUp may not be NULL

	// NOTE: if numMessagesProcessed != NULL.. will write # messages..
	//       .. processed to *numMessagesProcessed
	int poll_from_scout(match_t* match, int a, int maxMessages, int* bHungUp, int* numMessagesProcessed)
	{
		pollfd c;
		c.events = POLLIN;
		c.fd = match->otherSockets[a];
		int d = poll(&c, 1, 0);

		if(numMessagesProcessed != NULL)
		{
			*numMessagesProcessed = 0;
		}

		*bHungUp = 0;

		if(!(d == -1 || d == 0))
		{
			if(num_bytes_available_for_reading(match->otherSockets[a]) == 0)
			{
				// empty message, this means the other socket has hung up (I think)

				if(on_scout_hung_up != NULL)
				{
					on_scout_hung_up(match->port, match->otherIpAddressOrHostnameses[a]);
				}

				// NOTE: will socket implicitly be "closed" here..?
				remove_connection_at(match, a);

				*bHungUp = 1;

				return 0;
			}

			using d_t = struct { match_t* match; int a; int maxMessages; int numMessagesProcessed; int bHungUp; };

			d_t d;
			d.match = match;
			d.a = a;
			d.maxMessages = maxMessages;

			process_next_event_arg_t e;
			e.a = EPollEvent_POLLIN;
			e.c = &d;
			e.b = [](int a, void* b)
			{
				d_t* c = (d_t*)b;

				c->bHungUp = 0;

				using e_t = struct { match_t* match; int a; };
				e_t e;
				e.match = c->match;
				e.a = c->a;

				void(*my_on_receive)(void*, tm_message_t*, int) = [](void* a, tm_message_t* message, int b)
					{
						e_t* c = (e_t*)a;

						if(on_receive_from_scout != NULL)
						{
							on_receive_from_scout(c->match->port, c->match->otherIpAddressOrHostnameses[c->a], message, b);
						}
					};

				int bTimedOut;

				process_messages(c->match->otherSockets[c->a], c->maxMessages, TIMEOUT, my_on_receive, &e, &bTimedOut, &c->numMessagesProcessed, &c->match->sizeOfTimedOutMessagePerConnection[c->a]);

				if(bTimedOut == 1)
				{
					printf("poll for match @ port %i timed out while polling scout w. <ip address|hostname> %s\n", c->match->port, c->match->otherIpAddressOrHostnameses[c->a]);
				}

				if(bTimedOut == 1 && c->match->sizeOfTimedOutMessagePerConnection[c->a] == 0)
				{
					if(on_scout_hung_up != NULL)
					{
						on_scout_hung_up(c->match->port, c->match->otherIpAddressOrHostnameses[c->a]);
					}

					// NOTE: here the socket is not actually close yet..? (hence remove_connection_at is not sufficient)
					tm_disconnect_scout(c->match->port, c->match->otherIpAddressOrHostnameses[c->a]);

					c->bHungUp = 1;
					//return;
				}
			};

			while(c.revents != 0)
			{
				//printf("c.revents before %hi\n", c.revents);

				process_next_event(&c.revents, &e, 1);

				if(d.maxMessages != -1)
				{
					d.maxMessages -= d.numMessagesProcessed;
				}

				if(numMessagesProcessed != NULL)
				{
					*numMessagesProcessed += d.numMessagesProcessed;
				}

				if(d.bHungUp == 1)
				{
					*bHungUp = 1;
					return 0; //<..? (i.e. hang up means no more messages to process)
				}

				//printf("c.revents after %i\n", c.revents);
			}

			return match->sizeOfTimedOutMessagePerConnection[a] > 0 || num_bytes_available_for_reading(match->otherSockets[a]) ? 1 : 0;
		}

		return 0;
	}
}

extern "C" int tm_poll_from_scout(int port, char* ipAddress, int maxMessages)
{
	match_t* match = NULL;
	for(int i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			match = &matches[i];
			break;
		}
	}
	if(match == NULL)
	{
		return -1;
	}

	int i;
	for(i = 0; i < match->numConnections; ++i)
	{
		if(strcmp(match->otherIpAddressOrHostnameses[i], ipAddress) == 0)
		{
			break;
		}
	}
	if(i == match->numConnections)
	{
		return -1;
	}

	poll_for_match(match);

	int bHungUp;

	return poll_from_scout(match, i, maxMessages, &bHungUp, NULL);
}

extern "C" int tm_poll_from_scouts(int port, int maxMessages)
{
	match_t* match = NULL;
	for(int i = 0; i < numMatches; ++i)
	{
		if(matches[i].port == port)
		{
			match = &matches[i];
			break;
		}
	}
	if(match == NULL)
	{
		return -1;
	}

	poll_for_match(match);

	int bMoreMessagesToProcess = 0;
	if(maxMessages == -1)
	{
		for(int i = 0; i < match->numConnections; ++i)
		{
			int bHungUp;
			bMoreMessagesToProcess |= poll_from_scout(match, i, -1, &bHungUp, NULL);

			if(bHungUp == 1)
			{
				--i;
			}
		}
	}
	else
	{
		int numMessagesLeftToProcess = maxMessages;

		for(int i = 0; i < match->numConnections; ++i)
		{
			int numMessagesProcessed;

			int bHungUp;

			bMoreMessagesToProcess |= poll_from_scout(match, i, numMessagesLeftToProcess, &bHungUp, &numMessagesProcessed);

			numMessagesLeftToProcess -= numMessagesProcessed;

			if(bHungUp == 1)
			{
				--i;
			}
		}
	}

	return bMoreMessagesToProcess == 1 ? 1 : 0;
}

extern "C" int tm_poll_from_match(tm_match_blob_t a, int maxMessages)
{
	scout_t* scout = NULL;
	int i;
	for(i = 0; i < numScouts; ++i)
	{
		if(scouts[i].port == a.port && strcmp(scouts[i].otherIpAddressOrHostname, a.hostname) == 0)
		{
			scout = &scouts[i];
			break;
		}
	}
	if(scout == NULL)
	{
		return -1;
	}

	pollfd b;
	b.fd = scout->socket;
	b.events = POLLIN;
	// NOTE: does poll consume the event? i.e. will "partial processing"..
	//       .. (i.e. writing size bytes to e.g...
	//       .. sizeOfMessageThatTimedOut) work?
	int c = poll(&b, 1, 0);

	if(!(c == -1 || c == 0))
	{
		if(num_bytes_available_for_reading(scout->socket) == 0)
		{
			// empty message, this means the other socket has hung up (I think)

			struct tm_match_blob_t h;
			h.port = scout->port;
			strcpy(h.hostname, scout->otherIpAddressOrHostname);

			if(on_match_hung_up != NULL)
			{
				on_match_hung_up(h);
			}

			// NOTE: will socket implicitly be "closed" here..?
			remove_scout_at(i);

			return 0;
		}

		using e_t = struct { scout_t* scout; int maxMessages; int numMessagesProcessed; int bHungUp; };
		e_t e;
		e.scout = scout;
		e.maxMessages = maxMessages;

		process_next_event_arg_t f;
		f.a = EPollEvent_POLLIN;
		f.c = &e;
		f.b = [](int a, void* b)
		{
			e_t* c = (e_t*)b;

			c->bHungUp = 0;

			using d_t = struct { scout_t* scout; };

			d_t d;
			d.scout = c->scout;

			void(*my_on_receive)(void*, tm_message_t*, int) = [](void* a, tm_message_t* message, int b)
			{
				d_t* c = (d_t*)a;

				if(on_receive_from_match != NULL)
				{
					tm_match_blob_t d;
					d.port = c->scout->port;
					strcpy(d.hostname, c->scout->otherIpAddressOrHostname);

					on_receive_from_match(d, message, b);
				}
			};

			int bTimedOut;

			process_messages(c->scout->socket, TIMEOUT, c->maxMessages, my_on_receive, &d, &bTimedOut, &c->numMessagesProcessed, &c->scout->sizeOfTimedOutMessage);

			if(bTimedOut == 1)
			{
				printf("scout %s:%i timed out\n", c->scout->otherIpAddressOrHostname, c->scout->port);
			}

			// NOTE: if timed out while reading size bytes.. hang up (i.e. invalid message received)
			if(bTimedOut && c->scout->sizeOfTimedOutMessage == 0)
			{
				struct tm_match_blob_t f;
				f.port = c->scout->port;
				strcpy(f.hostname, c->scout->otherIpAddressOrHostname);

				if(on_match_hung_up != NULL)
				{
					on_match_hung_up(f);
				}

				tm_disconnect_from_match(f);

				c->bHungUp = 1;
			}
		};

		while(b.revents != 0)
		{
			//printf("b.revents before %hi\n", b.revents);

			process_next_event(&b.revents, &f, 1);

			if(e.maxMessages != -1)
			{
				e.maxMessages -= e.numMessagesProcessed;
			}

			if(e.bHungUp == 1)
			{
				return 0; //<..? (i.e. hang up means no more messages to process)
			}

			//printf("b.revents after %i\n", b.revents);
		}

		return scout->sizeOfTimedOutMessage > 0 || num_bytes_available_for_reading(scout->socket) > 0 ? 1 : 0;
	}

	return 0;
}
