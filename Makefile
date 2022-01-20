tcp-mini: tcp_mini.o
	rm -f libtcp-mini.a
	ar rcs libtcp-mini.a tcp_mini.o

tcp_mini.o: tcp_mini.cpp
	g++ -c tcp_mini.cpp

clean:
	rm -f libtcp-mini.a tcp_mini.o
