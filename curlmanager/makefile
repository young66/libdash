SOURCES := $(wildcard *.cpp)
OBJS := $(SOURCES:.cpp=.o)

SHELL := /bin/bash

LDFLAGS += -Wall -L./lib -lnghttp2 -lcurl -lpthread

CPPFLAGS += -Wall -I./include -g -std=c++11

main: $(OBJS)
	g++ $(OBJS) $(LDFLAGS) -o $@
show:
	echo $(SOURCES)
	echo $(OBJS)

clean:
	-rm *.o
	-rm *.m4s
