UNAME = $(shell uname -o)

CC ?= gcc
CFLAGS = -Wextra -Wall -std=gnu99 -Iinclude -Wno-unused-parameter -Wno-unused-variable -Wno-duplicate-decl-specifier

ifeq ($(UNAME), Msys)
MSFLAGS = -lws2_32
endif

MQTT_C_SOURCES = src/mqtt.c src/mqtt_pal.c
MQTT_C_EXAMPLES = bin/simple_publisher bin/simple_subscriber bin/reconnect_subscriber bin/bio_publisher bin/openssl_publisher bin/dws
MQTT_C_UNITTESTS = bin/tests
BINDIR = bin

all: $(BINDIR) $(MQTT_C_UNITTESTS) $(MQTT_C_EXAMPLES)

dumb-ws/dws.o: dumb-ws/dws.c
	$(CC) $(CLFAGS) `pkg-config --cflags libtls` $^ -c -o $@

bin/dws: dumb-ws/dws.o examples/dws.c src/mqtt.c src/mqtt_pal_dws.c
	$(CC) $(CFLAGS) -DMQTT_USE_CUSTOM_SOCKET_HANDLE -DMQTTC_PAL_FILE="mqtt_dws.h" -I./dumb-ws $^ -lpthread $(MSFLAGS) `pkg-config --libs libtls` -o $@

bin/simple_%: examples/simple_%.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) $^ -lpthread $(MSFLAGS) -o $@

bin/reconnect_%: examples/reconnect_%.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) $^ -lpthread $(MSFLAGS) -o $@

bin/bio_%: examples/bio_%.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) `pkg-config --cflags openssl` -D MQTT_USE_BIO $^ -lpthread $(MSFLAGS) `pkg-config --libs openssl` -o $@

bin/openssl_%: examples/openssl_%.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) `pkg-config --cflags openssl` -D MQTT_USE_BIO $^ -lpthread $(MSFLAGS) `pkg-config --libs openssl` -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

$(MQTT_C_UNITTESTS): tests.c $(MQTT_C_SOURCES)
	$(CC) $(CFLAGS) `pkg-config --cflags cmocka` $^ `pkg-config --libs cmocka` $(MSFLAGS) -o $@

clean:
	rm -rf $(BINDIR)

check: all
	./$(MQTT_C_UNITTESTS)
