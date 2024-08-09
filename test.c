#include <tcp_mini.h>

#include <test_mini.h>

#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

#include <string.h>
#if defined(__linux__)
#include <pthread.h>

#define SOCKET int
#define INVALID_SOCKET -1
#else
// NOTE: "pthread implementation" in mingw "10.4.0" (from chocolatey) is unusably broken
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#define pthread_t HANDLE
#define pthread_mutex_t HANDLE
int pthread_mutex_init(pthread_mutex_t* a, void* b)
{
	*a = CreateMutex(NULL, FALSE, NULL);
	return 0;
}
void pthread_mutex_destroy(pthread_mutex_t* a)
{
	CloseHandle(*a);
}
void pthread_mutex_unlock(pthread_mutex_t* a)
{
	ReleaseMutex(*a);
}
void pthread_mutex_lock(pthread_mutex_t* a)
{
	WaitForSingleObject(*a, INFINITE);
}
void pthread_create(pthread_t* a, void* b, void(*c)(void*), void* d)
{
	*a = (HANDLE)_beginthread(c, 0, d);
}
void pthread_join(pthread_t a, void* b)
{
	WaitForSingleObject(a, INFINITE);
	CloseHandle(a);
}
#endif
#include <stdio.h>
#include <unistd.h>

#include <errno.h>
#include <time.h>

#if defined(_WIN32)
#include <winsock2.h>
#endif


int testPort = 44444;

// IDEA: test 7 -> "bad message" test #3 (size of message according to size bytes is > actual # bytes sent)
// IDEA: test 6 -> "bad message" test #2 (size of message according to size bytes is < actual # bytes sent)
// IDEA: test 5 -> TM_MAXSCOUTS,TM_MAXMATCHES,TM_MAXCONNECTIONS test (i.e. test that no more scouts/matches can connect then defined)
// IDEA: test 4 -> "maxMessages test" (i.e. test that no more than maxMessages are processed if specified in "poll call")

//*****************************************************************************

pthread_mutex_t test_3_tcp_mini_mutex;
struct tm_match_blob_t test_3_a;
char test_3_scout_ip_address_or_hostname[254];
int test_3_scout_connected;
int test_3_match_hung_up;
int test_3_on_receive_from_match_called;

void test_3_on_match_hung_up(struct tm_match_blob_t a)
{
	if(a.port != test_3_a.port)
	{
		fputs("warning: wrong port in \"match blob\" parameter that was passed to test_3_on_match_hung_up\n", stdout);
	}

	if(strcmp(a.hostname, test_3_a.hostname) != 0)
	{
		fputs("warning: wrong <ip address|hostname> in \"match blob\" parameter that was passed to test_3_on_match_hung_up\n", stdout);
	}

	test_3_match_hung_up = 1;
}

void test_3_on_receive_from_match(struct tm_match_blob_t a, struct tm_message_t* message, int b)
{
	if(a.port != test_3_a.port)
	{
		fputs("warning: wrong port in \"match blob\" parameter that was passed to test_3_on_receive_from_match\n", stdout);
	}

	if(strcmp(a.hostname, test_3_a.hostname) != 0)
	{
		fputs("warning: wrong <ip address|hostname> passed to test_3_on_receive_from_match\n", stdout);
	}

	test_3_on_receive_from_match_called = 1;
}

void test_3_on_scout_connected(int port, char* ipAddressOrHostname)
{
	if(port != testPort)
	{
		fputs("warning: wrong port passed to test_3_on_scout_connected\n", stdout);
	}

	if(strlen(ipAddressOrHostname) == 0)
	{
		fputs("warning: <ip address|hostname> passed to test_3_on_scout_connected is empty\n", stdout);
	}

	strcpy(test_3_scout_ip_address_or_hostname, ipAddressOrHostname);

	test_3_scout_connected = 1;
}

// NOTE: script for test 3..
//       tm_set_on_match_hung_up + tm_set_on_receive_from_match
//       tm_become_a_match + while(1) { <tm_connect_to_match> }
//       TM_SEND_TO_SCOUT(/*...*/);
//       tm_stop_being_a_match
//       tm_unset_on_match_hung_up + tm_unset_on_receive_from_match

#if defined(__linux__)
void* test_3_match(void* a)
#else
void test_3_match(void* a)
#endif
{
	int* b = (int*)a;

	test_3_scout_connected = 0;

	tm_set_on_scout_connected(test_3_on_scout_connected);

	pthread_mutex_lock(&test_3_tcp_mini_mutex);
	int e = tm_become_a_match(testPort);
	pthread_mutex_unlock(&test_3_tcp_mini_mutex);
	if(e != 1)
	{
		*b = -1; //< *b = 0;..?
#if defined(__linux__)
		return NULL;
#else
		return;
#endif
	}

	while(test_3_scout_connected == 0)
	{
		// NOTE: e should be displayed once (for connected)
		fputs("e\n", stdout);
		tm_poll_from_scouts(testPort, -1);
	}

	struct tm_message_t c;

	pthread_mutex_lock(&test_3_tcp_mini_mutex);
	TM_SEND_TO_SCOUT(testPort, test_3_scout_ip_address_or_hostname, &c, NULL, 0);
	//pthread_mutex_unlock(&test_3_tcp_mini_mutex);
	// will message still be received if disconnect is "right after"?
	//pthread_mutex_lock(&test_3_tcp_mini_mutex);
	tm_stop_being_a_match(testPort);
	pthread_mutex_unlock(&test_3_tcp_mini_mutex);

	tm_unset_on_scout_connected();

	*b = 1;
#if defined(__linux__)
	return NULL;
#endif
}

#if defined(__linux__)
void* test_3_scout(void* a)
#else
void test_3_scout(void* a)
#endif
{
	int* b = (int*)a;

	test_3_a.port = testPort;
	strcpy(test_3_a.hostname, "localhost");
	test_3_match_hung_up = 0;
	test_3_on_receive_from_match_called = 0;

	tm_set_on_match_hung_up(test_3_on_match_hung_up);
	tm_set_on_receive_from_match(test_3_on_receive_from_match);

	while(1)
	{
		pthread_mutex_lock(&test_3_tcp_mini_mutex);
		int d = tm_connect_to_match(test_3_a);
		pthread_mutex_unlock(&test_3_tcp_mini_mutex);
		if(d == 1)
		{
			break;
		}
	}

	while(test_3_match_hung_up == 0)
	{
		// NOTE: d should be displayed once (for message received + to detect hang up)
		fputs("d\n", stdout);
		pthread_mutex_lock(&test_3_tcp_mini_mutex);
		tm_poll_from_match(test_3_a, -1);
		pthread_mutex_unlock(&test_3_tcp_mini_mutex);

		//sleep(1);
	}

	tm_unset_on_match_hung_up();
	tm_unset_on_receive_from_match();

	*b = test_3_on_receive_from_match_called == 1 ? 1 : 0;
#if defined(__linux__)
	return NULL;
#endif
}

// test_3 -> "callback" test #2 (i.e. match will stop being a match)
// NOTE: i.e. if callbacks get called when they should
int test_3()
{
	int a; //< "return value" for match
	int b; //< "return value" for scout

	if(pthread_mutex_init(&test_3_tcp_mini_mutex, NULL) != 0)
	{
		return -1;
	}

	pthread_t matchThread;
	pthread_t scoutThread;
	pthread_create(&matchThread, NULL, test_3_match, &a);
	pthread_create(&scoutThread, NULL, test_3_scout, &b);

	pthread_join(matchThread, NULL);
	pthread_join(scoutThread, NULL);

	pthread_mutex_destroy(&test_3_tcp_mini_mutex);

	if(a == -1 && b == -1)
	{
		return -1;
	}
	else if(a == 0 || b == 0)
	{
		return 0;
	}
	else if (a == -1 || b == -1)
	{
		return -1;
	}

	return 1;
}

//*****************************************************************************

pthread_mutex_t test_2_tcp_mini_mutex;
char test_2_scout_ip_address_or_hostname[254]; //< i.e. might not be e.g. "localhost" or "127.0.0.1" (string will be determined using either gethostbyaddr or inet_ntoa)
struct tm_match_blob_t test_2_a;
int test_2_scout_connected;
int test_2_scout_hung_up;
int test_2_on_receive_from_scout_called;

void test_2_on_scout_connected(int port, char* ipAddressOrHostname)
{
	if(port != testPort)
	{
		fputs("warning: wrong port passed to test_2_on_scout_connected\n", stdout);
	}

	if(strlen(ipAddressOrHostname) == 0)
	{
		fputs("warning: <ip address|hostname> passed to test_2_on_scout_connected is empty\n", stdout);
	}

	strcpy(test_2_scout_ip_address_or_hostname, ipAddressOrHostname);

	test_2_scout_connected = 1;
}

void test_2_on_scout_hung_up(int port, char* ipAddressOrHostname)
{
	if(port != testPort)
	{
		fputs("warning: wrong port passed to test_2_on_scout_connected\n", stdout);
	}

	if(strcmp(ipAddressOrHostname, test_2_scout_ip_address_or_hostname) != 0)
	{
		fputs("warning: wrong <ip address|hostname> passed to test_2_on_scout_hung_up\n", stdout);
	}

	test_2_scout_hung_up = 1;
}

void test_2_on_receive_from_scout(int port, char* ipAddressOrHostname, struct tm_message_t* message, int a)
{
	if(port != testPort)
	{
		fputs("warning: wrong port passed to test_2_on_scout_connected\n", stdout);
	}

	if(strcmp(ipAddressOrHostname, test_2_scout_ip_address_or_hostname) != 0)
	{
		fputs("warning: wrong <ip address|hostname> passed to test_2_on_scout_hung_up\n", stdout);
	}

	test_2_on_receive_from_scout_called = 1;
}

// NOTE: "script" for test 2..
//       tm_set_on_scout_connected + tm_set_on_scout_hung_up + tm_set_on_receive_from_scout
//       tm_become_a_match + while(1) { <tm_connect_to_match> }
//       TM_SEND_TO_MATCH(/*...*/);
//       tm_disconnect_from_match
//       tm_unset_on_scout_connected + tm_unset_on_scout_hung_up + tm_unset_on_receive_from_scout
#if defined(__linux__)
void* test_2_scout(void* a)
#else
void test_2_scout(void* a)
#endif
{
	int* b = (int*)a;

	test_2_a.port = testPort;
	strcpy(test_2_a.hostname, "localhost");

	while(1)
	{
		pthread_mutex_lock(&test_2_tcp_mini_mutex);
		int d = tm_connect_to_match(test_2_a);
		pthread_mutex_unlock(&test_2_tcp_mini_mutex);
		if(d == 1)
		{
			break;
		}
	}

	struct tm_message_t c;

	pthread_mutex_lock(&test_2_tcp_mini_mutex);
	TM_SEND_TO_MATCH(test_2_a, &c, NULL, 0); //< i.e. "dummy message" (to trigger callback only)
	//pthread_mutex_unlock(&test_2_tcp_mini_mutex);
	// will message still be received if disconnect is "right after"?
	//pthread_mutex_lock(&test_2_tcp_mini_mutex);
	tm_disconnect_from_match(test_2_a);
	pthread_mutex_unlock(&test_2_tcp_mini_mutex);

	*b = 1;
#if defined(__linux__)
	return NULL;
#endif
}

#if defined(__linux__)
void* test_2_match(void* a)
#else
void test_2_match(void* a)
#endif
{
	int* b = (int*)a;

	test_2_scout_connected = 0;
	test_2_scout_hung_up = 0;
	test_2_on_receive_from_scout_called = 0;

	tm_set_on_scout_connected(test_2_on_scout_connected);
	tm_set_on_scout_hung_up(test_2_on_scout_hung_up);
	tm_set_on_receive_from_scout(test_2_on_receive_from_scout);

	pthread_mutex_lock(&test_2_tcp_mini_mutex);
	int d = tm_become_a_match(testPort);
	pthread_mutex_unlock(&test_2_tcp_mini_mutex);
	if(d != 1)
	{
		*b = -1;
#if defined(__linux__)
		return NULL;
#else
		return;
#endif
	}

	while(!(test_2_scout_connected == 1 && test_2_scout_hung_up == 1))
	{
		// NOTE: c should be displayed twice (i.e. once for connected, second time for message received)..?
		fputs("c\n", stdout);

		pthread_mutex_lock(&test_2_tcp_mini_mutex);
		tm_poll_from_scouts(testPort, -1);
		pthread_mutex_unlock(&test_2_tcp_mini_mutex);

		//sleep(1);
	}

	pthread_mutex_lock(&test_2_tcp_mini_mutex);
	tm_stop_being_a_match(testPort);
	pthread_mutex_unlock(&test_2_tcp_mini_mutex);

	tm_unset_on_receive_from_scout();
	tm_unset_on_scout_hung_up();
	tm_unset_on_scout_connected();

	*b = test_2_on_receive_from_scout_called == 1 ? 1 : 0;
#if defined(__linux__)
	return NULL;
#endif
}

// test_2 -> "callback" test #1 (i.e. scout will disconnect)
// NOTE: i.e. if callbacks get called when they should
int test_2()
{
	int a; //< "return value" for match
	int b; //< "return value" for scout

	if(pthread_mutex_init(&test_2_tcp_mini_mutex, NULL) != 0)
	{
		return -1;
	}

	pthread_t matchThread;
	pthread_t scoutThread;
	pthread_create(&matchThread, NULL, test_2_match, &a);
	pthread_create(&scoutThread, NULL, test_2_scout, &b);

	pthread_join(matchThread, NULL);
	pthread_join(scoutThread, NULL);

	pthread_mutex_destroy(&test_2_tcp_mini_mutex);

	if(a == -1 && b == -1)
	{
		return -1;
	}
	else if(a == 0 || b == 0)
	{
		return 0;
	}
	else if(a == -1 || b == -1)
	{
		return -1;
	}

	return 1;
}

//*****************************************************************************

// NOTE: test_1 doesn't work currently as it now relies on accessing a..
//       .. static variable from tcp_mini.cpp (implementation used to be..
//       .. different, see note about "manually add tm_get_scouts,..
//       .. tm_get_matches, ..."
// TODO: replace get_scouts and get_matches with parsing /proc/net/tcp..
//       .. (linux only) or using netstat?..
//       .. https://stackoverflow.com/a/8902719
/*
// NOTE: test 1 is "exploitation test" (i.e. to immitate someone modifying..
//       .. the library code to obtain the sockets)
//       to run "exploitation tests".. manually add tm_get_scouts,..
//       .. tm_get_matches,tm_get_num_scouts,tm_get_num_matches to tcp-mini
// NOTE: "hard-coding" (i.e. using "the names known to to the linker", e.g...
//       .. _ZN12_GLOBAL__N_19numScoutsE) did not work

struct scout_t
{
	int port;
	SOCKET socket;
	char otherIpAddressOrHostname[254];
	// NOTE: if processing previous message timed out but size was read..
	//       .. will store this here so rest of message can still be..
	//       .. read
	//       if sizeOfTimedOutMessage == 0, last message processed..
	//       .. "cleanly" (i.e. no time out)
	int sizeOfTimedOutMessage;
};

struct match_t
{
	SOCKET socket;
	int port;
#if defined TM_MAXCONNECTIONS
	SOCKET otherSockets[TM_MAXCONNECTIONS]; //< per scout, socket on which to listen to messages from scout
	char* otherIpAddressOrHostnameses[TM_MAXCONNECTIONS];
	int sizeOfTimedOutMessagePerConnection[TM_MAXCONNECTIONS];
#else
	SOCKET* otherSockets; //< per scout, socket on which to listen to messages from scout
	char** otherIpAddressOrHostnameses; //< i.e. char[254]*
	// NOTE: per other socket..
	//       .. if processing previous message timed out but size was..
	//          .. read will store this here so rest of message can..
	//          .. still be read
	//          if sizeOfTimedOutMessagePerConnection[..] == 0, last..
	//          .. message processed "cleanly" (i.e. no time out)
	int* sizeOfTimedOutMessagePerConnection;
#endif
	int numConnections;
};

struct scout_t* get_scouts()
{
	return (struct scout_t*)tm_get_scouts();
}

struct match_t* get_matches()
{
	return (struct match_t*)tm_get_matches();
}

pthread_mutex_t test_1_tcp_mini_mutex; //< i.e. tcp mini is not multi-thread safe (so only one thread may use at a time)
int test_1_match_hung_up;
int test_1_scout_hung_up;

void test_1_on_match_hung_up(struct tm_match_blob_t a)
{
	test_1_match_hung_up = 1;
}

#if defined(__linux__)
void* test_1_scout(void* a)
#else
void test_1_scout(void* a)
#endif
{
	int* b = (int*)a;

	test_1_match_hung_up = 0;

	tm_set_on_match_hung_up(test_1_on_match_hung_up);

	struct tm_match_blob_t c;
	//strcpy(c.ip, "127.0.0.1");
	strcpy(c.hostname, "localhost");
	c.port = testPort;

	while(1)
	{
		pthread_mutex_lock(&test_1_tcp_mini_mutex);
		int f = tm_connect_to_match(c);
		pthread_mutex_unlock(&test_1_tcp_mini_mutex);
		if(f == 1)
		{
			break;
		}
	}

	char e[3]; //< write 3 bytes (minimum is 4/sizeof(int))

	// send "bad message"
#if defined(__linux__)
	write(get_scouts()[0].socket, e, 3);
#else
	send(get_scouts()[0].socket, e, 3, 0);
#endif

	clock_t d = clock();

	while(clock() - d < CLOCKS_PER_SEC * 3.0f && test_1_match_hung_up == 0)
	{
		// NOTE: a should be displayed once (i.e. to detect hang up)
		fputs("a\n", stdout);

		pthread_mutex_lock(&test_1_tcp_mini_mutex);
		tm_poll_from_match(c, -1); //< i.e. to detect on_match_hung_up
		pthread_mutex_unlock(&test_1_tcp_mini_mutex);

		//sleep(1);
	}

	if(test_1_match_hung_up == 0)
	{
		fputs("error: test 1 reached 3.0 seconds since \"bad message\" was sent (and thus automatically failed)\n", stdout);
		*b = 0;
#if defined(__linux__)
		return NULL;
#else
		return;
#endif
	}

	tm_unset_on_match_hung_up();

	*b = 1;
#if defined(__linux__)
	return NULL;
#endif
}

void test_1_on_scout_hung_up(int port, char* ipAddressOrHostname)
{
	test_1_scout_hung_up = 1;
}

#if defined(__linux__)
void* test_1_match(void* a)
#else
void test_1_match(void* a)
#endif
{
	int* b = (int*)a; //< i.e. return value

	test_1_scout_hung_up = 0;

	tm_set_on_scout_hung_up(test_1_on_scout_hung_up);

	pthread_mutex_lock(&test_1_tcp_mini_mutex);
	// NOTE: tm_become_a_match,tm_stop_being_a_match should only write to..
	//       .. variables that are match-specific? (hence lock should not be..
	//       .. needed)
	int c = tm_become_a_match(testPort);
	pthread_mutex_unlock(&test_1_tcp_mini_mutex);
	if(c != 1)
	{
		fputs("error: test 1 match \"set up\" failed\n", stdout);
		*b = -1;
#if defined(__linux__)
		return NULL;
#else
		return;
#endif
	}

	while(test_1_scout_hung_up == 0)
	{
		// NOTE: b should be displayed once (i.e. to receive "bad message" and immediately hang up as a result)
		fputs("b\n", stdout);

		// NOTE: in tm_poll_<from_scouts|from_matches> e.g. process_messages might cause same registers to be used on all threads..?
		pthread_mutex_lock(&test_1_tcp_mini_mutex);
		tm_poll_from_scouts(testPort, -1);
		pthread_mutex_unlock(&test_1_tcp_mini_mutex);

		//sleep(1);
	}

	pthread_mutex_lock(&test_1_tcp_mini_mutex);
	tm_stop_being_a_match(testPort);
	pthread_mutex_unlock(&test_1_tcp_mini_mutex);

	tm_unset_on_scout_hung_up();

	*b = 1;
#if defined(__linux__)
	return NULL;
#endif
}

// test_1 -> "timeout test" (a.k.a. "bad message" test #1)
// NOTE: i.e. send message of < sizeof(int) bytes (without using tm_send_to_<scout|scouts|match>)
//       should result in other side hanging up
// NOTE: since timeout code is exactly the same for both scout and match..
//       .. testing only "one directional" suffices..?
//       OR
//       add another test that "does test 1 the other way around" (i.e. a match sends a bad message)
// NOTE: returns 1 if test was performed and succeeded
//       returns 0 if test was (partially) performed and failed
//       returns -1 if no code was executed (e.g. if couldn't connect, hence didn't perform test)
int test_1()
{
	if(pthread_mutex_init(&test_1_tcp_mini_mutex, NULL) != 0)
	{
		return -1;
	}

	pthread_t matchThread;
	pthread_t scoutThread;

	int b; //< i.e. "return value" from test_1_match
	int a; //< i.e. "return value" from test_1_scout

	pthread_create(&matchThread, NULL, test_1_match, &b);
	pthread_create(&scoutThread, NULL, test_1_scout, &a);

	pthread_join(matchThread, NULL);
	pthread_join(scoutThread, NULL);

	pthread_mutex_destroy(&test_1_tcp_mini_mutex);

	if(a == -1 && b == -1)
	{
		return -1;
	}
	if(a == 0 || b == 0)
	{
		return 0;
	}
	if(a == -1 || b == -1) //< i.e. one test succeeded somehow, while the other didn't run
	{
		return -1;
	}

	return 1;
}
*/

//*****************************************************************************

// IDEA: script to compile tcp-mini -> tcp-mini, ms-tcp-mini, mm-tcp-mini,..
//       .. mc-tcp-mini, msmm-tcp-mini, msmc-tcp-mini, mmmc-tcp-mini,..
//       .. msmmmc-tcp-mini
//       i.e. "abbreviation" for..
//       .. TM_MAXSCOUTS -> ms
//       .. TM_MAXMATCHES -> mm
//       .. TM_MAXCONNECTIONS -> mc
//       also.. e.g. tm_connect -> mstm_connect/mmtm_connect/mctm_connect/etc.
//       then.. in tests, e.g...
//       test_1(mstm_set_on_match_hung_up, mstm_unset_on_match_hung_up, mstm_connect, mstm_poll_from_match)
//       test_1(mmtm_set_on_match_hung_up, mmtm_unset_on_match_hung_up, mmtm_connect, mmtm_poll_from_match)

int tests()
{
    //TM_TEST2(1);
    TM_TEST2(2);
    TM_TEST2(3);

    return 0;
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
	WSADATA a;
	if(WSAStartup(MAKEWORD(2,2), &a) != 0)
	{
		fputs("failed to initialize windows sockets 2\n", stdout);
		return 1;
	}
#endif

    int a = tests();    

#if defined(_WIN32)
	WSACleanup();
#endif

    return a;
}