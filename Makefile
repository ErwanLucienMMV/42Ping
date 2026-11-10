NAME = ft_ping
CC = cc
CFLAGS = -Wall -Wextra -Werror -std=c99 -pedantic -D_DEFAULT_SOURCE
CPPFLAGS = -Iinclude
LDLIBS = -lm

SRC = src/main.c
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

.PHONY: all clean fclean re
