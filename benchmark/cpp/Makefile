CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-g -O3 -std=c++14 -Igc 
LDFLAGS=-g
LDLIBS=

TEST_SRCS=src/GCTest.cpp src/StressTest.cpp
TEST_OBJS=$(subst .cpp,.o,$(TEST_SRCS))
OBJS=$(subst src/,obj/,$(TEST_OBJS))

.PHONY: all DEBUG

all: main

DEBUG: CPPFLAGS=-g -std=c++14 -Igc -DDEBUG -DGC_DEBUG 
DEBUG: main

obj:
	mkdir -p obj
	
main: $(OBJS)
	$(CXX) $(LDFLAGS) -o gctest.out $(OBJS) $(LDLIBS)

obj/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) -c -o $@ $< 

.depend: $(RTGC_SRCS) $(TEST_SRCS)
	$(CXX) $(CPPFLAGS) -MM $^ > ./.depend;
	sed -Ei 's/(.*)\.o: /obj\/\1\.o: /' $@

clean:
	$(RM) ./.depend
	$(RM) $(OBJS)


include .depend