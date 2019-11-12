
CC:=gcc
target := threadpool_test
OBJS:=libthpool.o test.o

libs:=libthpool.so libthpool.a
LIBS_OBJS:=libthpool.o

CFLAGS+= -g -Wall -Werror -W -Wshadow -Wwrite-strings -lpthread -Wstrict-prototypes -Wextra -fPIC

all: clean $(target) $(libs)

libthpool.a:$(LIBS_OBJS)
	$(AR) rcs $@ $^

libthpool.so:$(LIBS_OBJS)
	$(CC) $(CFLAGS) -shared $^ -o $@

$(target): $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) -o $(target)

%.o:%.c
	$(CC) -c $^ -g -Wall -Werror -W -Wshadow -Wwrite-strings -Wstrict-prototypes -Wextra -fPIC


clean:
	rm -f *.o $(target) *.a *.so
