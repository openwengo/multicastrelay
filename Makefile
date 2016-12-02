NAME = mcastreplay

CC = g++

RM	= rm -f

SRC = mcastreplay.cpp

CPPFLAGS = -I ../boost_1_61_0/

LDLIBS = ../boost_1_61_0/bin.v2/libs/program_options/build/gcc-4.8.3/release/link-static/threading-multi/libboost_program_options.a

all: $(NAME)

$(NAME):
		$(CC) $(SRC) -o $(NAME) $(CPPFLAGS) $(LDLIBS)

clean: 
		$(RM) $(NAME)

re: clean all

.PHONY: all clean re