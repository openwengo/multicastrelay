NAME = mcastreplay

CC = g++

RM	= rm -f

SRC = mcastreplay.cpp

OBJS = mcastreplay.o

#BOOST_INC=-I../boost_1_61_0/
BOOST_INC=

CPPFLAGS = $(BOOST_INC) -std=gnu++11 -pthread

#LDLIBS = $(BOOST_PATH)/bin.v2/libs/program_options/build/gcc-4.8.3/release/link-static/threading-multi/libboost_program_options.a\
#	 $(BOOST_PATH)/bin.v2/libs/system/build/gcc-4.8.3/release/link-static/threading-multi/libboost_system.a\
#
LDLIBS =  -lboost_program_options -lboost_system -lboost_filesystem




all: $(NAME)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CPPFLAGS)

$(NAME): $(OBJS)
		$(CXX) $(CPPFLAGS) -o $(NAME) $(OBJS) $(LDLIBS)

clean: 
		$(RM) $(NAME) $(OBJS)

re: clean all

.PHONY: all clean re
