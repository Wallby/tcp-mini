#ifndef TCP_MINI_H
#define TCP_MINI_H

// NOTE: match -> "active host" reachable on a specific port
//       scout -> can attempt to connect to a match
//       match won't be "disturbed" by the disconnecting of a scout
//       scout will no longer be scout if match disconnects

// NOTE: "endianness" of..
//       .. buffers is left-to-right
//       .. "integer variables" is left-to-right decreases

#ifdef __cplusplus
#define TCP_MINI_FUNCTION extern "C"
#else
#define TCP_MINI_FUNCTION
#endif


enum ETMMessageType
{
  ETMMessageType_None //< for empty messages
};

// to use this file, create an enum for each supported concrete message
struct tm_message_t
{
  int type;
};

#define TCP_MINI_MAX_MESSAGE_SIZE sizeof(int) * 48 //< 48 ints in a messages should be enough? exceeding this will NOT send the message

// NOTE: comment TM_MAXSCOUTS/TM_MAXMATCHES,TM_MAXCONNECTIONS out for "unlimited" scouts/matches/connections
//#define TM_MAXSCOUTS 1
//#define TM_MAXMATCHES 1
// NOTE: i.e. how many scouts may connect to a match
//#define TM_MAXCONNECTIONS 4

//********************* phase 1 - establishing connection *********************

#if (!defined TCP_MINI_SCOUT_ONLY)
// NOTE: returns 0 if failed to become a match, 1 if successfully became match
//       returns -1 if no code was executed (i.e. there was already a match..
//       .. at port OR becoming a match would cause # matches to exceed..
//       .. TM_MAXMATCHES)
TCP_MINI_FUNCTION int tm_become_a_match(int port);
// NOTE: returns 1 if succeeded
//       returns -1 if no code was executed (i.e. there was no match at port)
TCP_MINI_FUNCTION int tm_stop_being_a_match(int port);

// NOTE: returns 1 if succeeded
//       returns -1 if no code was executed (i.e. there was no match at port..
//       .. or there was no scout w. ipAddressOrHostname for match at port)
TCP_MINI_FUNCTION int tm_disconnect_scout(int port, char* ipAddressOrHostname);

// NOTE: tm_disconnect_scout may be called from tm_set_on_scout_connected to..
//       .. "refuse" connection
//void(*on_scout_connected)(int port, char* ipAddressOrHostname)
TCP_MINI_FUNCTION void tm_set_on_scout_connected(void(*a)(int, char*));
TCP_MINI_FUNCTION void tm_unset_on_scout_connected();

// NOTE: void(*a)(int port, char* ipAddressOrHostname)
TCP_MINI_FUNCTION void tm_set_on_scout_hung_up(void(*a)(int, char*));
TCP_MINI_FUNCTION void tm_unset_on_scout_hung_up();
#endif

#if (!defined TCP_MINI_MATCH_ONLY)
struct tm_match_blob_t
{
  union
  {
    // NOTE: hostname has a maximum length of 253 + 1 characters (there is always a
	//       trailing 0), and is in the format [63].[63].[63].[61] where each [..]
	//       indicates an array of characters. Not all characters have to be used
	//       up. My source is https://web.archive.org/web/20190518124533/https://devblogs.microsoft.com/oldnewthing/?p=7873
    char hostname[254];
    // NOTE: ipAddress is in format "x(xx).x(xx).x(xx).x(xx)", and has a null character @ [7:15]
    char ipAddress[16]; //< + [254-16] bytes that are "unused"
  };
  int port;
};

// NOTE: returns 0 if failed to connect for some reason, 1 if connection was established successfully
//       returns -1 if no code was executed (i.e. already connected to match..
//       .. OR establishing connection would cause # scouts to exceed..
//       .. TM_MAXSCOUTS)
TCP_MINI_FUNCTION int tm_connect_to_match(struct tm_match_blob_t a);
// NOTE: returns 1 if succeeded
//       returns -1 if no code was executed (i.e. was not connected to match)
TCP_MINI_FUNCTION int tm_disconnect_from_match(struct tm_match_blob_t a);

//void(*on_match_hung_up)(struct tm_match_blob_t a);
TCP_MINI_FUNCTION void tm_set_on_match_hung_up(void(*a)(struct tm_match_blob_t));
TCP_MINI_FUNCTION void tm_unset_on_match_hung_up();
#endif

//**************** phase 2 - not a phase **********************

#if (!defined TCP_MINI_SCOUT_ONLY)
// NOTE: returns 1 if message was sent
//       returns 0 if something went wrong (i.e. message is invalid)
//       return -1 if no code was executed
TCP_MINI_FUNCTION int tm_send_to_scout(int port, char* ipAddressOrHostname, struct tm_message_t* a, int b, void* c, int d);
#define TM_SEND_TO_SCOUT(port, ipAddressOrHostname, a, b, c) tm_send_to_scout(port, ipAddressOrHostname, (struct tm_message_t*)a, sizeof *a, b, c)
#define TM_SEND_BLOCK_TO_SCOUT(port, ipAddressOrHostname, a, b) tm_send_to_scout(port, ipAddressOrHostname, (struct tm_message_t*)a, sizeof *a, b, sizeof *b)

// NOTE: returns 1 if message was sent
//       returns 0 if something went wrong (i.e. message is invalid)
//       return -1 if no code was executed
TCP_MINI_FUNCTION int tm_send_to_scouts(int port, struct tm_message_t* a, int b, void* c, int d);
#define TM_SEND_TO_SCOUTS(port, a, b, c) tm_send_to_scouts(port, (struct tm_message_t*)a, sizeof *a, b, c)
#define TM_SEND_BLOCK_TO_SCOUTS(port, a, b) tm_send_to_scouts(port, (struct tm_message_t*)a, sizeof *a, b, sizeof *b)

//< NOTE: a is the exact number of bytes that the message holds.
//void(*on_receive_from_scout)(int port, char* ipAddressOrHostname, tm_message_t* message, int a);
TCP_MINI_FUNCTION void tm_set_on_receive_from_scout(void(*a)(int, char*, struct tm_message_t*, int));
TCP_MINI_FUNCTION void tm_unset_on_receive_from_scout();

// NOTE: if a scout disconnected.. will call on_scout_hung_up
// NOTE: don't call poll asynchronously (it is not officially supported)
// NOTE: set maxMessages to -1 to remove the limit (i.e. no # messages is "too much")
// will call on_receive_from_scout for each "polled" message
// will return 0 if no more messages left to process or 1 if there are
// will return -1 if no code was executed
TCP_MINI_FUNCTION int tm_poll_from_scout(int port, char* ipAddress, int maxMessages);
// NOTE: if a scout disconnected.. will call on_scout_hung_up
// NOTE: don't call poll asynchronously (it is not officially supported)
// NOTE: set maxMessages to -1 to remove the limit (i.e. no # messages is "too much")
// will call on_receive_from_scout for each "polled" message
// will return 0 if no more messages left to process or 1 if there are
// will return -1 if no code was executed
TCP_MINI_FUNCTION int tm_poll_from_scouts(int port, int maxMessages);
#endif

#if (!defined TCP_MINI_MATCH_ONLY)
// NOTE: returns 1 if message was sent
//       returns 0 if something went wrong (i.e. message is invalid)
//       return -1 if no code was executed
TCP_MINI_FUNCTION int tm_send_to_match(struct tm_match_blob_t a, struct tm_message_t* b, int c, void* d, int e);
#define TM_SEND_TO_MATCH(a, b, c, d) tm_send_to_match(a, (struct tm_message_t*)b, sizeof *b, c, d)
#define TM_SEND_BLOCK_TO_MATCH(a, b, c) tm_send_to_match(a, (struct tm_message_t*)b, sizeof *b, c, sizeof *c)

//< NOTE: b is the exact number of bytes that the message holds.
//void(*on_receive_from_match)(struct tm_match_blob_t a, tm_message_t* message, int b);
TCP_MINI_FUNCTION void tm_set_on_receive_from_match(void(*a)(struct tm_match_blob_t, struct tm_message_t*, int));
TCP_MINI_FUNCTION void tm_unset_on_receive_from_match();

// NOTE: if match disconnected.. will call on_match_hung_up
// NOTE: don't call poll asynchronously (it is not officially supported)
// NOTE: set maxMessages to -1 to remove the limit (i.e. no # messages is "too much")
// will call on_receive_from_match for each "polled" message
// will return 0 if no more messages left to process or 1 if there are
// will return -1 if no code was executed
TCP_MINI_FUNCTION int tm_poll_from_match(struct tm_match_blob_t a, int maxMessages);
#endif

//*****************************************************************************

#if defined TCP_MINI_MATCH_ONLY
#define tm_disconnect tm_disconnect_scout
#define tm_set_on_hung_up(a) tm_set_on_scout_hung_up(a)
#define tm_unset_on_hung_up() tm_unset_on_scout_hung_up()
#define tm_poll tm_poll_from_scouts
#define tm_poll_from tm_poll_from_scout
#define tm_send tm_send_to_scouts
#define TM_SEND TM_SEND_TO_SCOUTS
#define TM_SEND_BLOCK TM_SEND_BLOCK_TO_SCOUTS
#define tm_send_to tm_send_to_scout
#define TM_SEND_TO TM_SEND_TO_SCOUT
#define TM_SEND_BLOCK_TO TM_SEND_BLOCK_TO_SCOUT
#define tm_set_on_receive tm_set_on_receive_from_scout
#define tm_unset_on_receive tm_unset_on_receive_from_scout
#define tm_set_on_connected tm_set_on_scout_connected
#define tm_unset_on_connected tm_unset_on_scout_connected
#endif

#if defined TCP_MINI_SCOUT_ONLY
#define tm_connect tm_connect_to_match
#define tm_disconnect tm_disconnect_from_match
#define tm_set_on_hung_up(a) tm_set_on_match_hung_up(a)
#define tm_unset_on_hung_up() tm_unset_on_match_hung_up()
#define tm_poll tm_poll_from_match
#define tm_send tm_send_to_match
#define TM_SEND TM_SEND_TO_MATCH
#define TM_SEND_BLOCK TM_SEND_BLOCK_TO_MATCH
#define tm_set_on_receive tm_set_on_receive_from_match
#define tm_unset_on_receive tm_unset_on_receive_from_match
#endif

#endif
