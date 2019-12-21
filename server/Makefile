CFLAGS += -I./include -I../common/include -O2 -Wall
LDFLAGS += -lm

OBJS = main.o ../common/cJSON.o ../common/sock.o

SAY=echo "server"
EXEC=server

all: clean $(EXEC)

$(EXEC): $(OBJS)
	@$(SAY) "CC $@"
	@$(CC) -o $@ $^ $(LDFLAGS)

clean:
	@$(SAY) "CLEAN"
	@rm -f $(EXEC) $(OBJS)
	@rm -f *.o

%.o: %.c
	@$(SAY) "CC $@"
	@$(CC) -o $@ $(CFLAGS) -c $<
