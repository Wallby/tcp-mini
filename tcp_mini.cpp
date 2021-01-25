#define TCP_MINI_MATCH
#define TCP_MINI_SCOUT
#include "tcp_mini.h"

// NOTE: std includes here (no c++ includes, they will break)
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <netdb.h>

void* operator new[](std::size_t a)
{
	return malloc(a);
}
void* operator new(std::size_t a) //< can throw std::bad_alloc?
{
	return malloc(a);
}
void operator delete(void* a)
{
    free(a);
}
void operator delete(void* a, std::size_t)
{
    free(a); //< there is no check here on purpose, please catch this issue elsewhere
}

int length(char* a)
{
	return strlen(a);
}
template<typename T>
int length(T a[])
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
	b c = 0; \
	while(a != 0) \
	{ \
		b d = a & 1; \
		if(d != 0) \
		{ \
			return c; \
		} \
		++c; \
		a = a << 1; \
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

#define MAX_MESSAGE_T struct { char b[TCP_MINI_MAX_MESSAGE_SIZE]; } //< NOTE: b to avoid confusion (don't use this directly)

#define TM_RETRY_ONCE_IF_NOT(a,b,c) { c = a; if((c) != (b)) { c = a; } } //< b is the success value
#define TM_RETRY_ONCE(a,b) { b = a; if(!b) { b = a; }

// //*********************************************************

// enum EPacketStamp
// {
// 	EPacketStamp_Raw,
// 	EPacketStamp_ASCII
// };

// struct packet_t
// {
// 	int stamp; // one of EPacketStamp
// 	char* whatIsInThePacket;
// };

//*********************************************************

enum EMatchType
{
	EMatchType_A = 1, //< wait's for B
	EMatchType_B = 2,
};

class String
{
public:
	String() {};
	String(char* a)
	{
		this->a = new char[length(a) + 1];
		strcpy(this->a, a);
	}
	~String()
	{
		if(a != NULL)
		{
          delete a; //< delete[] is really the same as delete (both use free and internal "block sizes")
		}
	}

	operator char*() //< (char*) (should also work for char* .. = (String) ..)
	{
		return a;
	}

private:
	char* a;
};

struct match_t
{
	int c; //< 0 if invalid, one of EMatchType_* otherwise
};
struct match_a_t
{
	int c = EMatchType_A;
	int port;
	int socket;
	int otherSockets[4]; //< sockets on which to listen to messages from b(s)
	String otherIPs[4];
	int numOtherSockets = 0;
};
struct match_b_t
{
	int c = EMatchType_B;
	int port;
	int socket;
};
struct invalid_match_t
{
	int c = 0;
};

//match_t* match;
void* match;

//*********************************************************

//#ifdef TCP_MINI_MATCH
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
  c->socket = socket(AF_INET, SOCK_STREAM /* TCP */ /*| SOCK_NONBLOCK*/, 0);
  if(c->socket == -1)
  {
	  // NOTE: I am not sure if this is likely to fail, though EACCES seems like it "could" happen
	  delete c;
	  match = NULL; //< it seems to not make much sense to stop_being_a_match (i.e. close down), when there is not even a socket up
	  return 0;
  }

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

  sockaddr_in g;
  socklen_t h = sizeof g;
  int i = accept(c->socket, (sockaddr*)&g, &h);

  hostent* j = gethostbyaddr((void*)&g.sin_addr, sizeof g.sin_addr, AF_INET);
  if(j == NULL)
  {
    // kick the player from the lobby (can't obtain a readable ip)
  }

  c->otherSockets[c->numOtherSockets] = i;
  c->otherIPs[c->numOtherSockets] = j->h_name;
  ++c->numOtherSockets;

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
  int e;
  TM_RETRY_ONCE_IF_NOT(close(d->socket), 0, e);
  if(e != 0)
  {
	  return 0;
  }

  delete d;
  match = NULL;

  return 1;
}

extern "C" int tm_send_to(tm_message_t* a, int d, void* b, int c, char* ip)
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
	  if(strcmp(f->otherIPs[i], ip) == 0)
	  {
		break;
	  }

	  ++i;
	}
	if(i == f->numOtherSockets)
	{
	  return -1; //< ip is not "one of connected"
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

	write(f->otherSockets[i], buffer, g);

	return 1;
}
//#endif

//#ifdef TCP_MINI_SCOUT

/*
 * provides a function to connect that can be overridden for different
 * concrete protocols
 */
class IConnector
{
public:
	// attempt to establish a connection or return 0
	virtual int Connect(tm_match_blob_t a) = 0;
};

/*
 * TCP version of IConnector
 */
class TCPConnector : IConnector
{
public:
	virtual int Connect(tm_match_blob_t a) override
	{
		// assure that all settings in b are valid for TCP?
	}
};

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
    strncpy(a->ip, ip_pattern, c + 1);
    a->ip[c + 1] = '\0';

	// see if any results match the provided pattern
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
	if(d->socket == -1)
	{
		// NOTE: I am not sure if this is likely to fail, though EACCES seems like it "could" happen
		delete d;
		match = NULL; //< it seems to not make much sense to try to disconnect, when there is not even a socket up
		return -1;
	}

	// if a is set via a.ip, hostname should catch that case
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
  int e;
  TM_RETRY_ONCE_IF_NOT(close(d->socket), 0, e);
  if(e != 0)
  {
	return 0;
  }

  delete d;
  match = NULL;

  return 1;
}
//#endif

//#ifdef TCP_MINI_MATCH | TCP_MINI_SCOUT
//#endif

//*********************************************************

void(*on_receive)(tm_message_t* /*message*/, int /*a*/);
extern "C" void tm_set_on_receive(void(*a)(tm_message_t*, int))
{
	on_receive = a;
}
extern "C" void tm_unset_on_receive()
{
	on_receive = NULL;
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

  int g = d+c;

  if(g > TCP_MINI_MAX_MESSAGE_SIZE)
  {
    return -1; //< message not send, it is too big
  }

  char* buffer = new char[g];
  memcpy(&buffer[0], a, d);
  memcpy(&buffer[d], b, c);

  match_t* e = (match_t*)match;
  switch(e->c)
  {
  case EMatchType_A:
      {
    	  match_a_t* f = (match_a_t*)e;
		  for(int i = 0; i < f->numOtherSockets; ++i)
		  {
			  write(f->otherSockets[i], buffer, g);
		  }
      }
      return 1;
  case EMatchType_B:
      {
	    match_b_t* f = (match_b_t*)e;
	    printf("d\n");
	    write(f->socket, buffer, g);
      }
      return 1;
  }

  return -1;
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
      match_a_t* d = (match_a_t*)c;

      // 1. check the listening socket (e.g. for pending connections)

      pollfd a;
      a.fd = d->socket;
      a.events = POLLIN;
      int e = poll(&a, 1, 0); //< NOTE: at the time of writing this, there is only one connection request accepted at a time
      while(!(e == -1 | e == 0) && a.revents != 0) //< NOTE: poll can fail if interrupted (EINTR), not sure though if this applies if timeout value is 0
      {
		  //printf("hmm\n");
		  int f = get_index_of_first_in_bitfield(a.revents);
		  switch(f)
		  {
		  case 0: /* POLLIN */
			  {
				  sockaddr_in g;
				  socklen_t h = sizeof g;
				  int i = accept(d->socket, (sockaddr*)&g, &h);
				  // NOTE: documentation says that h might be greater than sizeof g now (should this be checked for?)
				  if(i == -1)
				  {
					  // TODO: make sure that the errors that can be thrown here are managed..?
					  break;
				  }
				  if(d->numOtherSockets == length(d->otherSockets))
				  {
					  // refuse ("lobby is full")
				  }
				  else
				  {
					  hostent* j = gethostbyaddr((void*)&g.sin_addr, sizeof g.sin_addr, AF_INET);
					  if(j == NULL)
					  {
                        // kick the player from the lobby (can't obtain a readable ip)
					  }

					  d->otherSockets[d->numOtherSockets] = i;
					  d->otherIPs[d->numOtherSockets] = j->h_name;
					  ++d->numOtherSockets;
				  }
			  }
			  break;
		  case 1: /* POLLPRI */
			  break;
		  case 2: /* POLLOUT */
			  break;
		  case 3: /* POLLERR */
			  break;
		  case 4: /* POLLHUP */
			  break;
		  case 5: /* POLLNVAL */
			  break;
		  case 6: /* POLLRDNORM */
			  break;
		  case 7: /* POLLRDBAND */
			  break;
		  case 8: /* POLLWRNORM */
			  break;
		  case 9: /* POLLWRBAND */
			  break;
		  case 10: /* POLLMSG */
			  break;
		  case 12: /* POLLREMOVE */
			  break;
		  case 13: /* POLLRDHUP */
			  break;
		  }
		  a.revents = remove_from_bitfield(f, a.revents);
      }

      // 2. check for messages on sockets of peers

      int b = 0;

      for(int i = 0; i < d->numOtherSockets; ++i)
      {
    	  pollfd g;
    	  g.fd = d->otherSockets[i];
    	  g.events = POLLIN;
    	  //< NOTE: can poll be called multiple times if there are messages "queued up"?
    	  int h = poll(&g, 1, 0);
    	  while(!(h == -1 | h == 0) && g.revents != 0)
    	  {
    		  printf("%hi\n", g.revents);
			  int f = get_index_of_first_in_bitfield(g.revents);
			  switch(f)
			  {
			  case 0: /* POLLIN */
				  {
					  if(b < max_messages)
					  {
						  // NOTE: maybe secretly prepend the size of the message
						  MAX_MESSAGE_T j;
						  int k = read(d->otherSockets[i], &j, sizeof j);
						  on_receive((tm_message_t*)&j, k);

						  ++b;
					  }
				  }
				  break;
			  case 1: /* POLLPRI */
				  break;
			  case 2: /* POLLOUT */
				  break;
			  case 3: /* POLLERR */
				  break;
			  case 4: /* POLLHUP */
				  break;
			  case 5: /* POLLNVAL */
				  break;
			  case 6: /* POLLRDNORM */
				  break;
			  case 7: /* POLLRDBAND */
				  break;
			  case 8: /* POLLWRNORM */
				  break;
			  case 9: /* POLLWRBAND */
				  break;
			  case 10: /* POLLMSG */
				  break;
			  case 12: /* POLLREMOVE */
				  break;
			  case 13: /* POLLRDHUP */
				  break;
			  }
	    	  g.revents = remove_from_bitfield(f, g.revents);
    	  }
      }
	}


	// fetch message
	// "char buffer[sizeof (int) * TCP_MINI_MAX_MESSAGE_SIZE];"

}
