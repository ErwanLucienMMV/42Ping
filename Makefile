NAME = ft_ping
CC = cc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -D_POSIX_C_SOURCE=200809L
CPPFLAGS = -Iinclude
LDLIBS = -lm -lrt

SRC = src/main.c src/options.c src/network.c src/packet.c src/receive.c \
	src/run.c src/statistics.c src/util.c
OBJ = $(SRC:src/%.c=obj/%.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDLIBS) -o $@

obj/%.o: src/%.c include/ft_ping.h
	@mkdir -p obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

obj/packet_tests: tests/packets.c src/packet.c src/receive.c src/statistics.c src/util.c include/ft_ping.h
	@mkdir -p obj
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/packets.c src/packet.c src/receive.c \
		src/statistics.c src/util.c $(LDLIBS) -o $@

test: all obj/packet_tests
	./obj/packet_tests
	python3 tests/integration.py ./$(NAME)

.PHONY: all clean fclean re test
