libtcp-mini.lib: tcp_mini.o
	if exist libtcp-mini.lib del libtcp-mini.lib
	ar rcs libtcp-mini.lib tcp_mini.o

tcp_mini.o: tcp_mini.cpp
	g++ -c tcp_mini.cpp

clean:
	if exist libtcp-mini.lib del libtcp-mini.lib
	if exist tcp_mini.o del tcp_mini.o