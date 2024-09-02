include makefile_mini.mk


$(call mm_start_parameters_t,a)
a.ignoredbinaries:=^test$(MM_EXECUTABLE_EXTENSION)$$
$(call mm_start,a)

$(call mm_add_library_parameters_t,b)
b.filetypes:=EMMLibraryfiletype_Static
b.cpp:=tcp_mini.cpp
b.h:=tcp_mini.h
b.hpp:=tcp_mini.hpp
$(call mm_add_library,tcp-mini,b)

$(call mm_add_executable_parameters_t,c)
c.c:=test.c
#c.libraries:=tcp-mini test-mini
c.libraries:=tcp-mini
c.libFolders:=../test-mini/
c.hFolders:=../test-mini/
c.lib:=test-mini
ifeq ($(MM_OS),windows)
c.lib+=ws2_32
else #< else ifeq ($(MM_OS),chromeos)
c.lib+=pthread
endif
c.gccOrG++:=-Wl,--wrap=malloc,--wrap=free,--wrap=main
$(call mm_add_executable,test,c)

$(call mm_add_test_parameters_t,d)
d.executables:=test
$(call mm_add_test,test,d)

$(call mm_stop_parameters_t,e)
$(call mm_stop,e)
