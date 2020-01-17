OBJS = server client

.PHONY: $(OBJS)
all: $(OBJS)

$(OBJS):
	@echo "Make $@"
	@$(MAKE) -C $@

%.o: %.c
	@echo "CC $@"
	@$(CC) -o $@ $(CFLAGS) -c $<
