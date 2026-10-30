# Native C11 build. No downloads or previous-project dependencies.
CC ?= gcc
BUILD ?= build
WARN = -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wundef -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wold-style-definition -Wpointer-arith -Wwrite-strings
CFLAGS ?= -O2 -g
CPPFLAGS += -Iinclude -Iinclude/eip -D_POSIX_C_SOURCE=200809L -DGW_VERSION='"1.8.0"' -DEAD_VERSION='"1.8.0"'
LDLIBS += -pthread -lm
SOURCES := $(sort $(wildcard src/*/*.c))
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
TESTS := $(patsubst tests/%.c,$(BUILD)/%,$(wildcard tests/test_*.c))
.PHONY: all test clean
all: $(BUILD)/ek9500_modbus_tcp_gateway
$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 -pthread -MMD -MP -c $< -o $@
$(BUILD)/ek9500_modbus_tcp_gateway: $(BUILD)/app/main.o $(OBJECTS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(BUILD)/test_%: tests/test_%.c $(OBJECTS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 -DTEST_CONFIG_DIR='"config"' -DTEST_GATEWAY_CONFIG='"config/gateway.conf"' $< $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@
test: all $(TESTS)
	@set -e; for test in $(TESTS); do "$$test"; done
	bash tests/test_masterctl.sh
	python3 tests/test_integration.py $(BUILD)/ek9500_modbus_tcp_gateway
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d) $(BUILD)/app/main.d

# Optional systemd installation; preserves existing configuration files.
.PHONY: install-service
install-service: all
	getent passwd ek9500gw >/dev/null || useradd --system --home-dir /nonexistent --shell /sbin/nologin ek9500gw
	install -d -m 0755 /usr/local/bin /etc/ek9500-modbus-gateway /etc/systemd/system
	install -m 0755 $(BUILD)/ek9500_modbus_tcp_gateway /usr/local/bin/ek9500_modbus_tcp_gateway
	@for file in masters.conf coupler.conf; do test -e /etc/ek9500-modbus-gateway/$$file || install -m 0644 config/$$file /etc/ek9500-modbus-gateway/$$file; done
	@test -e /etc/ek9500-modbus-gateway/gateway.conf || install -m 0644 systemd/gateway.conf /etc/ek9500-modbus-gateway/gateway.conf
	install -m 0644 systemd/ek9500-modbus-tcp-gateway.service /etc/systemd/system/
	systemctl daemon-reload
