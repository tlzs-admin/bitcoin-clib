TARGET=ln-network-daemon

DEBUG=1
OPTIMIZE=-O2
VERBOSE=1

CC=gcc -std=gnu99 -D_DEFAULT_SOURCE -D_GNU_SOURCE
LINKER=gcc -std=gnu99 -D_DEFAULT_SOURCE -D_GNU_SOURCE
AR=ar crf

SRC_DIR=src
OBJ_DIR=obj
BIN_DIR=bin
LIB_DIR=lib

CFLAGS = -Wall -Iinclude -Iinclude/crypto -Iutils -D_VERBOSE=$(VERBOSE)
LIBS = -lm -lpthread -ljson-c -lcurl -luuid
LIBS += -lcurl -lgnutls -ldb -lgmp
LIBS += -lsecp256k1

ifeq ($(DEBUG),1)
CFLAGS += -g
OPTIMIZE = -O0
endif

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
OBJECTS := $(filter-out %app.o,$(OBJECTS))

BASE_SRC_DIR := $(SRC_DIR)/base
BASE_OBJ_DIR := $(OBJ_DIR)/base

UTILS_SRC_DIR := utils
UTILS_OBJ_DIR := obj/utils

BASE_SOURCES := $(wildcard $(BASE_SRC_DIR)/*.c)
BASE_OBJECTS := $(BASE_SOURCES:$(BASE_SRC_DIR)/%.c=$(BASE_OBJ_DIR)/%.o)

UTILS_SOURCES := $(wildcard $(UTILS_SRC_DIR)/*.c)
UTILS_OBJECTS := $(UTILS_SOURCES:$(UTILS_SRC_DIR)/%.c=$(UTILS_OBJ_DIR)/%.o)

BITCOIN_MESSAGES_SRC_DIR := src/bitcoin-messages
BITCOIN_MESSAGES_OBJ_DIR = obj/bitcoin-messages
BITCOIN_MESSAGES_SOURCES := $(wildcard $(BITCOIN_MESSAGES_SRC_DIR)/*.c)
BITCOIN_MESSAGES_OBJECTS := $(BITCOIN_MESSAGES_SOURCES:$(BITCOIN_MESSAGES_SRC_DIR)/%.c=$(BITCOIN_MESSAGES_OBJ_DIR)/%.o)


all: do_init bin/spv-node

bin/spv-node: $(OBJECTS) $(BASE_OBJECTS) $(UTILS_OBJECTS) $(BITCOIN_MESSAGES_OBJECTS)
	$(LINKER) -o $@ $(SRC_DIR)/spv_node_app.c $^ $(CFLAGS) $(LIBS) 

$(OBJECTS): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

$(BASE_OBJECTS): $(BASE_OBJ_DIR)/%.o : $(BASE_SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)
	
$(UTILS_OBJECTS): $(UTILS_OBJ_DIR)/%.o : $(UTILS_SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(CFLAGS)

$(BITCOIN_MESSAGES_OBJECTS): $(BITCOIN_MESSAGES_OBJ_DIR)/%.o: $(BITCOIN_MESSAGES_SRC_DIR)/%.c $(DEPS)
	$(CC) -o $@ -c $< $(CFLAGS)

	
.PHONY: do_init clean tests
do_init:
	mkdir -p obj/base obj/utils bin lib data conf tests
	mkdir -p $(BITCOIN_MESSAGES_OBJ_DIR)

clean:
	rm -rf obj/* $(TARGET)
	
tests: json-rpc

json-rpc: src/rpc/json-rpc.c $(OBJECTS)
	$(CC) -o tests/$@ src/rpc/json-rpc.c $(CFLAGS) $(LIBS) -D_TEST -D_STAND_ALONE

