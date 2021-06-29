#define TCP_MINI_MATCH
#define TCP_MINI_SCOUT
#include "tcp_mini.h"

// NOTE: std includes here (no c++ includes, they will break)
#include <cstdlib>
#include <cstring>
#include <cstdio>

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
inline void* operator new(std::size_t a) //< can throw std::bad_alloc?
{
	return malloc(a);
}
[[TCP_MINI_ALWAYS_INLINE]]
inline void operator delete(void* a)
{
    free(a);
}
[[TCP_MINI_ALWAYS_INLINE]]
inline void operator delete(void* a, std::size_t)
{
    free(a); //< there is no check here on purpose, please catch this issue elsewhere
}

namespace
{
	//int length(char* a)
	int length(const char* a)
	{
		return strlen(a);
	}
	// NOTE: T& is a reference to adress "decay issue" (where T[] becomes T*) as explained here http://www.cplusplus.com/articles/D4SGz8AR/
	template<typename T, int A>
	int length(T (&a)[A])
	{
		return sizeof a/sizeof *a;
	}
	// returns index of (first occurance of) b in a, or length of a if b is not in a
	int findc(char* a, char b)
	{
		int c = length(a);
		for(int i = 0; i < b; ++i)
		{
			if(a[i] == ':')
			{
	          return i;
			}
		}
		return c;
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

#define TM_RETRY_ONCE_IF_NOT(a,b,c) { c = a; if((c) != (b)) { c = a; } } //< b is the success value
#define TM_RETRY_ONCE(a,b) { b = a; if(!b) { b = a; }

//*********************************************************

namespace
{
	enum EMatchType
	{
		EMatchType_A = 1, //< wait's for B
		EMatchType_B = 2,
	};

	class String
	{
	public:
		String()
		{
		};
		//String(char* b)
		String(const char* b)
		{
			this->a = new char[length(b) + 1];
			strcpy(this->a, b);
		}
		~String()
		{
			if(a != NULL)
			{
			  delete a; //< delete[] is really the same as delete (both use free and internal "block sizes")
			}
		}

		void operator=(const String& b)
		{
			if(this->a != NULL)
			{
				delete this->a;
			}

			if(b.a != NULL)
			{
				this->a = new char[length(b.a) + 1];
				strcpy(this->a, b.a);
			}
			else
			{
				a = NULL;
			}
		}

		operator char*() //< (char*) (should also work for char* .. = (String) ..)
		{
			return a;
		}

	private:
		char* a = NULL;
	};

	struct match_t
	{
		int c; //< 0 if invalid, one of EMatchType_* otherwise
	};
	struct match_a_t
	{
		int c = EMatchType_A;
		int port;
	#if defined(__linux__)
		int socket;
		int otherSockets[TM_MAXCONNECTIONS]; //< sockets on which to listen to messages from b(s)
	#else
		SOCKET socket;
		SOCKET otherSockets[TM_MAXCONNECTIONS]; //< sockets on which to listen to messages from b(s)
	#endif
		String otherIPAddresses[TM_MAXCONNECTIONS];
		int numOtherSockets = 0; //< NOTE: the index is not suitable for use as index elsewhere, as the index of one b might change when another b disconnects
	};
	struct match_b_t
	{
		int c = EMatchType_B;
		int port;
	#if defined(__linux__)
		int socket;
	#else
		SOCKET socket;
	#endif
	};
	struct invalid_match_t
	{
		int c = 0;
	};

	//match_t* match;
	void* match;
}

//*********************************************************

//#ifdef TCP_MINI_MATCH
namespace
{
	void (*on_connected_to_us)(char* ipAddress);
	void (*on_scout_hung_up)(char* ipAddress);
}

extern "C" void tm_set_on_connected_to_us(void(*a)(char*))
{
	on_connected_to_us = a;
}
extern "C" void tm_unset_on_connected_to_us()
{
	on_connected_to_us = 0;
}
extern "C" void tm_set_on_scout_hung_up(void(*a)(char*))
{
	on_scout_hung_up = a;
}
extern "C" void tm_unset_on_scout_hung_up()
{
	on_scout_hung_up = NULL;
}

// can new match to match_a_t
// delete match when it is match_a_t
extern "C" int tm_become_a_match(int port)
{
  if(match == NULL)
  {
	match = (void*)new match_a_t;
  }
  else
  {
	  return -1; //< already a match, disconnect/stop being a match first
  }

  match_a_t* c = (match_a_t*)match; //< convention is to avoid a/b here to avoid confusion
  c->port = port;
#if defined(__linux__)
  c->socket = socket(AF_INET, SOCK_STREAM /* TCP */ | SOCK_NONBLOCK, 0);
  if(c->socket == -1)
#else // #elif defined(_WIN32)
  c->socket = socket(AF_INET, SOCK_STREAM /* TCP */, 0);
  if(c->socket == INVALID_SOCKET)
#endif
  {
	  // NOTE: I am not sure if this is likely to fail, though EACCES seems like it "could" happen
	  delete c;
	  match = NULL; //< it seems to not make much sense to stop_being_a_match (i.e. close down), when there is not even a socket up
	  return 0;
  }

#if defined(_WIN32)
  u_long e = 1;
  ioctlsocket(c->socket, FIONBIO, &e);
#endif

  // NOTE: SO_REUSEADDR "makes TCP less reliable" according to https://www.man7.org/linux/man-pages/man7/ip.7.html
#if defined(__linux)
  int d = 1;
#else
  char d = 1;
#endif
  setsockopt(c->socket, SOL_SOCKET, SO_REUSEADDR, &d, sizeof(int));

  sockaddr_in a;
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = INADDR_ANY;

  printf("%i\n", port);

  if(bind(c->socket, (struct sockaddr*) &a, sizeof a) != 0)
  {
	  // NOTE: I expect that EADDRINUSE is reasonable here..
	  tm_stop_being_a_match(); //< not sure what will happen if this fails..
	  return 0;
  }

  listen(c->socket, 1); //< errors that can occur here should be caught by bind

  return 1;
}

// delete match when it is match_a_t
extern "C" int tm_stop_being_a_match()
{
  if(match == NULL)
  {
	  return -1;
  }
  match_t* c = (match_t*)match;
  if(c->c == EMatchType_B)
  {
	  return -1; //< not a match, but a scout
  }

  match_a_t* d = (match_a_t*)c;
#if defined(__linux__)
  int e;
  TM_RETRY_ONCE_IF_NOT(close(d->socket), 0, e);
  if(e != 0)
  {
	  return 0;
  }
#else
  closesocket(d->socket);
#endif

  delete d;
  match = NULL;

  return 1;
}

extern "C" int tm_send_to(tm_message_t* a, int d, void* b, int c, char* ipAddress)
{
	if(match == NULL)
	{
		return -1;
	}

	match_t* e = (match_t*)match;
	if(e->c == EMatchType_B)
	{
		return -1; //< scouts may not pick
	}

	match_a_t* f = (match_a_t*)e;

	int i;
	for(i = 0; i < f->numOtherSockets;)
	{
	  if(strcmp(f->otherIPAddresses[i], ipAddress) == 0)
	  {
		break;
	  }

	  ++i;
	}
	if(i == f->numOtherSockets)
	{
	  return -1; //< ipAddress is not "one of connected"
	}

	if(d == 0)
	{
	  return -1; //< message with empty header is not allowed
	}

    int g = d+c;

    if(g > TCP_MINI_MAX_MESSAGE_SIZE)
    {
    	return -1; //< message not send, it is too big
    }

	char* buffer = new char[g];
	memcpy(&buffer[0], a, d);
	memcpy(&buffer[d], b, c);

#if defined(__linux__)
	write(f->otherSockets[i], buffer, g);
#else
	send(f->otherSockets[i], buffer, g, 0);
#endif

	return 1;
}
//#endif

//#ifdef TCP_MINI_SCOUT
namespace
{
	void(*on_match_hung_up)();
}

extern "C" void tm_set_on_match_hung_up(void(*a)())
{
	on_match_hung_up = a;
}
extern "C" void tm_unset_on_match_hung_up()
{
	on_match_hung_up = NULL;
}

// a == "out parameter"
extern "C" int tm_search_for_match(char* ip_pattern, tm_match_blob_t* a)
{
	// find : in ip_pattern
    int b = findc(ip_pattern, ':');
    if(b == length(ip_pattern))
    {
    	return -1;
    }

    int c = b - 1;
    strncpy(a->ipAddress, ip_pattern, c + 1);
    a->ipAddress[c + 1] = '\0';

	// see if any results match the provided pattern
    return 0;
}

// can delete match when it is match_b_t
// can new match to match_b_t
extern "C" int tm_connect(tm_match_blob_t a)
{
	if(match == NULL)
	{
		match = (void*)new match_b_t;
	}
	else
	{
		return -1; //< already a match stop being a match/disconnect first
	}

	match_b_t* d = (match_b_t*)match;
	d->port = a.port;
	d->socket = socket(AF_INET, SOCK_STREAM /* TCP */, 0);

#if defined(__linux__)
	if(d->socket == -1)
#else
	if(d->socket == INVALID_SOCKET)
#endif
	{
		// NOTE: I am not sure if this is likely to fail, though EACCES seems like it "could" happen
		delete d;
		match = NULL; //< it seems to not make much sense to try to disconnect, when there is not even a socket up
		return -1;
	}

	// NOTE: SO_REUSEADDR "makes TCP less reliable" according to https://www.man7.org/linux/man-pages/man7/ip.7.html
#if defined(__linux__)
	int f = 1;
#else
	char f = 1;
#endif
	setsockopt(d->socket, SOL_SOCKET, SO_REUSEADDR, &f, sizeof f);

	// if a is set via a.ipAddress, hostname should catch that case
	hostent* e = gethostbyname(a.hostname);
	if(e == NULL)
	{
		tm_disconnect(); //< even though there is no "connection" here yet, the socket still has to be closed. That's why this call is still needed.
        return 0; //< failed to resolve hostname
	}

	sockaddr_in b;
	memcpy((void*)&b.sin_addr.s_addr, (void*)e->h_addr, e->h_length);
	b.sin_family = AF_INET;
	b.sin_port = htons(a.port);

	printf("%s\n", a.hostname);
	printf("%i\n", a.port);
	if(connect(d->socket, (sockaddr*)&b, sizeof b) != 0)
	{
		tm_disconnect();
		return 0;
	}

	// once connected, enable O_NONBLOCK so that reading wont wait for data
#if defined(__linux__)
	fcntl(d->socket, F_SETFL, O_NONBLOCK);
#else
	u_long g = 1;
	ioctlsocket(d->socket, FIONBIO, &g);
#endif

	return 1;
}

// can delete match when it is match_b_t;
extern "C" int tm_disconnect()
{
  if(match == NULL)
  {
	  return -1;
  }
  match_t* c = (match_t*)match;
  if(c->c == EMatchType_A)
  {
    return -1; //< not a scout but a match, disconnect is what scouts do from the match.
  }

  match_b_t* d = (match_b_t*) c;
#if defined(__linux__)
  int e;
  TM_RETRY_ONCE_IF_NOT(close(d->socket), 0, e);
  if(e != 0)
  {
	return 0;
  }
#else
  closesocket(d->socket);
#endif

  delete d;
  match = NULL;

  return 1;
}
//#endif

//#ifdef TCP_MINI_MATCH | TCP_MINI_SCOUT
//#endif

//*********************************************************

namespace
{
	void(*on_receive_from_scout)(tm_message_t* /*message*/, int /*a*/, char* /*ipAddress*/);
	void(*on_receive_from_match)(tm_message_t* /*message*/, int /*a*/);
}

extern "C" void tm_set_on_receive_from_scout(void(*a)(tm_message_t*, int, char*))
{
	on_receive_from_scout = a;
}
extern "C" void tm_unset_on_receive_from_scout()
{
	on_receive_from_scout = NULL;
}

extern "C" void tm_set_on_receive_from_match(void(*a)(tm_message_t*, int))
{
	on_receive_from_match = a;
}
extern "C" void tm_unset_on_receive_from_match()
{
	on_receive_from_match = NULL;
}

extern "C" int tm_send(tm_message_t* a, int d, void* b, int c)
{
  if(match == NULL)
  {
	  return -1;
  }

  if(d == 0)
  {
	  return -1; //< message with empty header is not allowed
  }

  int h = d+c;
  if(h > TCP_MINI_MAX_MESSAGE_SIZE)
  {
    return -1; //< message not send, it is too big
  }

  int g = sizeof(int)+d+c; //< prepend the size of the message before the message

  char* buffer = new char[g];
  int i = 0;
  memcpy(&buffer[i], &h, sizeof(int));
  i += sizeof(int);
  memcpy(&buffer[i], a, d);
  i += d;
  memcpy(&buffer[i], b, c);

  match_t* e = (match_t*)match;
  switch(e->c)
  {
  case EMatchType_A:
      {
    	  match_a_t* f = (match_a_t*)e;
		  for(int i = 0; i < f->numOtherSockets; ++i)
		  {
#if defined(__linux__)
			  write(f->otherSockets[i], buffer, g);
#else
			  send(f->otherSockets[i], buffer, g, 0);
#endif
		  }
      }
      return 1;
  case EMatchType_B:
      {
	    match_b_t* f = (match_b_t*)e;
#if defined(__linux__)
	    write(f->socket, buffer, g);
#else
	    send(f->socket, buffer, g, 0);
#endif
	    printf("sent message of %i bytes (including size bytes)\n", g);
      }
      return 1;
  }

  return -1;
}

// NOTE: *c -> # bytes left available for reading
// NOTE: b -> socket to process messages for
// NOTE: *a -> # messages processed so far
namespace
{
#if defined(__linux__)
	void process_messages(int* c, int b, int* a, int max_messages)
#else
	void process_messages(u_long* c, SOCKET b, int* a, int max_messages)
#endif
	{
#if defined(__linux)
#define DO_IF_CANNOT_READ(q, p) if(*c < p) { q }
#else
// NOTE: s == # bytes that must be readable
#define DO_IF_CANNOT_READ(s, u) \
	{ \
		u_long t; \
		ioctlsocket(b, FIONREAD, &t); \
		if(t < s) \
		{ \
			u \
		} \
	}
#define WAIT_UNTIL_CAN_READ(s) \
	while(1) \
	{ \
		u_long t; \
		ioctlsocket(b, FIONREAD, &t); \
		if(t >= s) \
		{ \
			break; \
		} \
	}
#endif

		char* o = new char[*c]; //< for storing the remainder of the message when not enough data is available for reading

		while(1)
		{
		  if(*a < max_messages || max_messages == -1)
		  {
			  // first sizeof(int) bytes store the size of the message.
			  int l;
#if defined(__linux__)
			  DO_IF_CANNOT_READ(read(b, o, *c); break;, sizeof(int)); //< read last bytes as to discard them and break
			  /*int m =*/ read(b, &l, sizeof(int));
#else
			  // NOTE: currently, this is exploitable by sending an incomplete message (I think)
			  WAIT_UNTIL_CAN_READ(sizeof(int));

			  WSABUF p;
			  p.buf = (char*)&l;
			  p.len = sizeof(int);
			  DWORD q = MSG_PARTIAL;
			  DWORD r;
			  WSARecv(b, &p, 1, &r, &q, NULL, NULL);
#endif
			  *c -= sizeof(int);

			  // NOTE: if (m != sizeof(int)) should not happen
			  printf("l %i\n", l);
			  char* n = new char[l];
#if defined(__linux__)
			  DO_IF_CANNOT_READ(read(b, o, *c); break;, l); //< read last bytes as to discard them and break
			  /*int k =*/ read(b, n, l);
#else
			  WAIT_UNTIL_CAN_READ(l);

			  p.buf = n;
			  p.len = l;
			  //q = 0; //< set to MSG_PARTIAL if another part of the message needs to be processed after
			  WSARecv(b, &p, 1, &r, &q, NULL, NULL);
#endif
			  *c -= l;

			  // NOTE: if (k != l) should not happen

			  match_t* d = (match_t*)match;
			  switch(d->c)
			  {
			  case EMatchType_A:
			  {
				  match_a_t* e = (match_a_t*)d;
				  for(int i = 0; i < e->numOtherSockets; ++i)
				  {
					  if(e->otherSockets[i] == b)
					  {
						  on_receive_from_scout((tm_message_t*)n, l, e->otherIPAddresses[i]);
					  }
				  }
				  break;
			  }
			  case EMatchType_B:
				  on_receive_from_match((tm_message_t*)n, l);
				  break;
			  }

			  delete n;

			  ++*a;

#if defined(_WIN32)
			  DO_IF_CANNOT_READ(1, break;); //< break if there is no more readable data
#endif
		  }
		  else
		  {
			  break; //< can't process any more messages this poll
		  }
		}
		delete o;
#undef DO_IF_CANNOT_READ
	}

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

	//void process_next_event(short int& a, struct {int a; void(*b)(int)}* b, int c)
	struct process_next_event_arg_t
	{
		int a; //< one of EPollEvent
		// NOTE: a is one of EPollEvent
		//       b is a "pointer to data" (for arguments)
		//void(*b)(int a, void* b);
		void(*b)(int, void*);
		void* c; //< gets passed to b (i.e. b(.., c))
	};
	// NOTE: a is &<pollfd>.revents
	//       b is array of process_next_event_arg_t structures
	//       c is length(b)
	void process_next_event(short int* a, process_next_event_arg_t* b, int c)
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

	int poll_for_match(match_a_t* c, int max_messages)
	{
		// 1. check the listening socket (e.g. for pending connections)

		process_next_event_arg_t f;
		f.a = EPollEvent_POLLIN;
		using g_t = struct { match_a_t* a; };
		g_t g;
		g.a = c;
		f.c = &g;
		f.b = [](int a, void* b)
		{
			g_t* g = (g_t*)b;

			sockaddr_in c;
			socklen_t d = sizeof c;
#if defined(__linux__)
			int e = accept(g->a->socket, (sockaddr*)&c, &d);
			// NOTE: documentation says that d might be greater than sizeof c now (should this be checked for?)
			if(e == -1)
#else
			SOCKET e = accept(g->a->socket, (sockaddr*)&c, &d);
			if(e == INVALID_SOCKET)
#endif
			{
				// TODO: make sure that the errors that can be thrown here are managed..?
				return;
			}
//#if defined(__linux__)
			if(g->a->numOtherSockets == length(g->a->otherSockets))
//#else
//		if(c->numOtherSockets == (sizeof c->otherSockets / sizeof *c->otherSockets))
//#endif
			{
				// refuse ("lobby is full")
			}
			else
			{
#if defined(__linux__)
				hostent* f = gethostbyaddr((void*)&c.sin_addr, sizeof c.sin_addr, AF_INET);
#else
				hostent* f = gethostbyaddr((char*)&c.sin_addr, sizeof c.sin_addr, AF_INET);
#endif
				if(f == NULL)
				{
					// kick the player from the lobby (can't obtain a readable ip address)
				}

#if defined(__linux__)
				fcntl(e, F_SETFL, O_NONBLOCK);
#else
				u_long h = 1;
				ioctlsocket(e, FIONBIO, &h);
#endif

				g->a->otherSockets[g->a->numOtherSockets] = e;
				if(f->h_name != NULL)
				{
					g->a->otherIPAddresses[g->a->numOtherSockets] = f->h_name;
				}
				else
				{
					// TODO: copy addr as readable string
					g->a->otherIPAddresses[g->a->numOtherSockets] = "ok";
				}
				++g->a->numOtherSockets;

				on_connected_to_us(g->a->otherIPAddresses[g->a->numOtherSockets - 1]);
			}
		};

		pollfd a;
		a.fd = c->socket;
		a.events = POLLIN;
		int e = poll(&a, 1, 0); //< NOTE: at the time of writing this, there is only one connection request accepted at a time
		while(!(e == -1 || e == 0) && a.revents != 0) //< NOTE: poll can fail if interrupted (EINTR), not sure though if this applies if timeout value is 0
		{
			process_next_event(&a.revents, &f, 1);
		}

		// 2. check for messages on sockets of peers

		int b = 0;
#if defined(__linux__) //< # bytes left available for reading
		int o;
#else
		u_long o;
#endif

		for(int i = 0; i < c->numOtherSockets;)
		{
			  pollfd g;
			  g.fd = c->otherSockets[i];
			  g.events = POLLIN;
			  //< NOTE: can poll be called multiple times if there are messages "queued up"?
			  int h = poll(&g, 1, 0);
			  //< NOTE: perhaps call poll once with an array of all file descriptors?

			  if(!(h == -1 || h == 0))

			  {
				  ioctl(c->otherSockets[i], FIONREAD, &o);
				  if(o == 0)
				  {
					  // empty message, this means the other socket has hung up (I think)
					  --c->numOtherSockets;
					  memcpy(&c->otherSockets[i], &c->otherSockets[i+1], c->numOtherSockets - i);
					  // NOTE: c->numOtherSockets - i == "remaining sockets"


					  if(on_scout_hung_up != NULL)
					  {
						  on_scout_hung_up(c->otherIPAddresses[i]);
					  }

					  memcpy(&c->otherIPAddresses[i], &c->otherIPAddresses[i+1], c->numOtherSockets - i);

					  continue; //< onto the next socket
				  }

				  f.a = EPollEvent_POLLIN;
#if defined(__linux__)
				  using h_t = struct { int* a; match_a_t* b; int* c; int d; int e; };
#else
				  using h_t = struct { u_long* a; match_a_t* b; int* c; int d; int e; };
#endif
				  h_t h;
				  h.a = &o;
				  h.b = c;
				  h.c = &b;
				  h.d = max_messages;
				  h.e = i;
				  f.c = &h;
				  f.b = [](int a, void* b)
				  {
					h_t* c = (h_t*)b;
					process_messages(c->a, c->b->otherSockets[c->e], c->c, c->d);
				  };

				  while(g.revents != 0)
				  {
					  printf("g.revents before %hi\n", g.revents);

					  process_next_event(&g.revents, &f, 1);

					  printf("g.revents after %i\n", g.revents);
				  }
			  }

			  ++i; //< advance to next socket
		}

		return o > 0;
	}

	int poll_for_scout(match_b_t* c, int max_messages)
	{
		pollfd a;
		a.fd = c->socket;
		a.events = POLLIN;
		int d = poll(&a, 1, 0);

		if(!(d == -1 || d == 0))
		{
		  int b = 0;
#if defined(__linux__) //< # bytes left available for reading
		  int e;
#else
		  u_long e;
#endif

		  ioctl(c->socket, FIONREAD, &e);

		  if(e == 0)
		  {
			  // empty message, this means the other socket has hung up (I think)
			  tm_disconnect();

			  if(on_match_hung_up != NULL)
			  {
				 on_match_hung_up();
			  }

			  return 0;
		  }

		  process_next_event_arg_t f;
		  f.a = EPollEvent_POLLIN;
		  // IDEA: replace void* to struct with ... (va_arg)
#if defined(__linux__)
		  using g_t = struct { int* a; match_b_t* b; int* c; int d; };
#else
		  using g_t = struct { u_long* a; match_b_t* b; int* c; int d; };
#endif
		  g_t g;
		  g.a = &e;
		  g.b = c;
		  g.c = &b;
		  g.d = max_messages;
		  f.c = &g;
		  f.b = [](int a, void* b)
		  {
			  g_t* c = (g_t*)b;
			  process_messages(c->a, c->b->socket, c->c, c->d);
		  };

		  while(a.revents != 0)
		  {
			  printf("a.revents before %hi\n", a.revents);

			  process_next_event(&a.revents, &f, 1);

			  printf("a.revents after %i\n", a.revents);
		  }

		  return e > 0;
		}

		return 0;
	}
}

extern "C" int tm_poll(int max_messages)
{
	if(match == NULL)
	{
		return -1;
	}

	match_t* c = (match_t*)match;
	if(c->c == EMatchType_A)
	{
      return poll_for_match((match_a_t*)c, max_messages);
	}
	if(c->c == EMatchType_B)
	{
      return poll_for_scout((match_b_t*)c, max_messages);
	}

	return -1;
}
