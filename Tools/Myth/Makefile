BUILD_PATH  ?= Build
OBJ_PATH    ?= $(BUILD_PATH)/Myth_Objects/Myth
TARGET_EXEC ?= Myth

SRCS := $(shell find -name '*.c')
OBJS := $(SRCS:%=$(OBJ_PATH)/%.o)

CC    ?= gcc
RM    ?= rm
ECHO  ?= echo
MKDIR ?= mkdir

$(BUILD_PATH)/$(TARGET_EXEC): $(OBJS)
	@$(MKDIR) -p $(dir $@)
	@$(ECHO) Linking final executable $@
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_PATH)/%.c.o: %.c
	@$(MKDIR) -p $(dir $@)
	@$(ECHO) Compiling $< to $@
	@$(CC) $(CFLAGS) -c $< -o $@
