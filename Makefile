ifndef OS # linux
LIBRARY_EXTENSION=.a
RM=rm -f $(1)
else ifeq ($(OS), Windows_NT) # windows
LIBRARY_EXTENSION=.lib
RM=if exist $(1) del $(1)
else
$(error os not supported)
endif

libtcp-mini$(LIBRARY_EXTENSION): tcp_mini.o
	$(call RM,libtcp-mini$(LIBRARY_EXTENSION))
	ar rcs $@ $<

tcp_mini.o: tcp_mini.cpp tcp_mini.h
	g++ -c $<

test$(EXECUTABLE_EXTENSION): test.o libtcp-mini$(LIBRARY_EXTENSION) ../test-mini/libtest-mini$(LIBRARY_EXTENSION)
	gcc -Wl,--wrap=malloc,--wrap=free,--wrap=main -o test$(EXECUTABLE_EXTENSION) test.o -L./ -ltcp-mini -L../test-mini -ltest-mini

test.o: test.c tcp_mini.h ../test-mini/test_mini.h
	gcc -c test.c -I./ -I../test-mini/

#******************************************************************************

.PHONY: release
release: test$(EXECUTABLE_EXTENSION)
	./$<

.PHONY: clean
clean:
	$(call RM,tcp_mini.o)
	$(call RM,libtcp-mini.a)
	$(call RM,libtcp-mini.lib)
	
	$(call RM,test.o)
	$(call RM,test)
	$(call RM,test.exe)