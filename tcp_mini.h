#ifndef TCP_MINI_H
#define TCP_MINI_H

// NOTE: "endianness" of..
//       .. buffers is left-to-right
//       .. "integer variables" is left-to-right decreases

#ifdef __cplusplus
#define TCP_MINI_FUNCTION extern "C"
#else
#define TCP_MINI_FUNCTION
#endif

enum EMessageType //< NOTE: should this be ETMMessageType?
{
  EMessageType_None //< for empty messages
};

// to use this file, create an enum for each supported concrete message
struct tm_message_t
{
  int type;
};

#define TCP_MINI_MAX_MESSAGE_SIZE sizeof(int) * 48 //< 48 ints in a messages should be enough? exceeding this will NOT send the message

//**************** phase 1 - matchmaking **********************

#ifdef TCP_MINI_MATCH
#define TM_MAXCONNECTIONS 4
TCP_MINI_FUNCTION int tm_stop_being_a_match();
// NOTE: returns 0 if failed to become a match, 1 if sucessfully become match
//       returns -1 if no code was executed
TCP_MINI_FUNCTION int tm_become_a_match(int port);

//int get_port();
// NOTE: returns 1 is message was sent
//       returns -1 if no code was executed
#define TM_SEND_BLOCK_TO(a, b) tm_send_to(a, sizeof *a, b, sizeof b)
#define TM_SEND_TO(a, b, c) tm_send_to(a, sizeof *a, b, c)
TCP_MINI_FUNCTION int tm_send_to(struct tm_message_t* a, int d, void* b, int c, char* ip);

//void(*on_connected_to_us)(char* ip)
TCP_MINI_FUNCTION void tm_set_on_connected_to_us(void(*a)(char*));
TCP_MINI_FUNCTION void tm_unset_on_connected_to_us();

// NOTE: void(*a)(char* ip)
TCP_MINI_FUNCTION void tm_set_on_scout_hung_up(void(*a)(char*));
TCP_MINI_FUNCTION void tm_unset_on_scout_hung_up();
#define TM_SET_ON_HUNG_UP(a) tm_set_on_scout_hung_up(a)
#define TM_UNSET_ON_HUNG_UP() tm_unset_on_scout_hung_up()
#endif

#ifdef TCP_MINI_SCOUT
struct tm_match_blob_t
{
  union
  {
    // NOTE: hostname has a maximum length of 253 + 1 characters (there is always a
	//       trailing 0), and is in the format [63].[63].[63].[61] where each [..]
	//       indicates an array of characters. Not all characters have to be used
	//       up. My source is https://web.archive.org/web/20190518124533/https://devblogs.microsoft.com/oldnewthing/?p=7873
    char hostname[254];
    // NOTE: ip is in format "x(xx).x(xx).x(xx).x(xx)", and has a null character @ [7:15]
    char ip[16]; //< + [254-16] bytes that are "unused"
  };
  int port;
};
// NOTE: ip_pattern is in format "xxx.xxx.xxx.xxx:<port>"
// NOTE: returns 0 if no match was found, 1 if a match was found
//       returns -1 if no code was executed
// tm_search_for_match("192.168.*.*:1025", ..); //< will scan for LAN-hosted matches
TCP_MINI_FUNCTION int tm_search_for_match(char* ip_pattern, struct tm_match_blob_t* a);
//TCP_MINI_FUNCTION int tm_search_for_matches(char* ip_pattern, struct tm_match_blob_t** a, int b);
// NOTE: returns 0 if failed to connect for some reason, 1 if connection was established successfully
//       returns -1 if no code was executed*
//       *calling this function may cause the running of initialization code that is non relevant to the function's execution (this may happen even if -1 is returned)
TCP_MINI_FUNCTION int tm_connect(struct tm_match_blob_t a);
// NOTE: returns 0 if disconnect happened partially, 1 if disconnect completed without issues
//       returns -1 if no code was executed
TCP_MINI_FUNCTION int tm_disconnect();

TCP_MINI_FUNCTION void tm_set_on_match_hung_up(void(*a)());
TCP_MINI_FUNCTION void tm_unset_on_match_hung_up();
#define TM_SET_ON_HUNG_UP(a) tm_set_on_match_hung_up(a)
#define TM_UNSET_ON_HUNG_UP() tm_unset_on_match_hung_up()

char* tm_get_ip();
//int get_port();
#endif

#if defined TCP_MINI_MATCH | defined TCP_MINI_SCOUT
int tm_get_port(); //< will return port for "match | scout" depending on which is "active"
#endif

//**************** phase 2 - not a phase **********************

// NOTE: don't call poll asynchronously (it is not officially supported)
//< NOTE: set max_messages to -1 to remove the limit (i.e. no # messages is "too much")
//< will call on_receive for each "polled" message
//< will return 0 if no more messages left to process or 1 if there are
//< will return -1 if no code was executed
TCP_MINI_FUNCTION int tm_poll(int max_messages);
//< NOTE: a is the exact number of bytes that the message holds.
//void(*on_receive)(message_t* message, int a);
TCP_MINI_FUNCTION void tm_set_on_receive(void(*a)(struct tm_message_t*, int));
TCP_MINI_FUNCTION void tm_unset_on_receive();

// NOTE: will send to all ip's if match, or to the match if scout
// NOTE: returns 1 if message was sent
//       return -1 if no code was executed
#define TM_SEND_BLOCK(a, b) tm_send(a, sizeof *a, b, sizeof b) //< alias for blocks (e.g. struct or [])
#define TM_SEND(a,b,c) tm_send(a, sizeof *a, b, c)
TCP_MINI_FUNCTION int tm_send(struct tm_message_t* a, int d, void* b, int c);

#endif
