# Licensed under the MIT License.
# See the LICENSE file in the project root for more information.

# Compiler and flags
VERSION := 0.0.1-local
CC = gcc

# Main build flags
CFLAGS := -Wall -Iinclude -lcrypto -DBUILD_VERSION=\"$(VERSION)\"
LDFLAGS = -lcrypto

# PAM module build flags
PAM_CFLAGS = $(CFLAGS) -fPIC -fno-stack-protector
PAM_LDFLAGS := $(LDFLAGS) -lpam -shared

# Test build flags
TEST_CFLAGS := -Wall -Iinclude -I/usr/include
TEST_LDFLAGS := -lcmocka $(LDFLAGS)

# Directories
SRCDIR = src
LIBDIR = $(SRCDIR)/lib
PAMDIR = $(SRCDIR)/pam
TESTDIR = test
BINDIR = bin
PAMOUTDIR = pam
BUILDDIR = build
TESTBUILDDIR = $(BUILDDIR)/test

# Install
BIN_DEST = /usr/bin/
PAM_DEST = /lib/security/

# Libraries
LIBS = $(BUILDDIR)/users.o $(BUILDDIR)/crypt.o $(BUILDDIR)/state.o

# Targets
TARGETS = $(BINDIR)/ppedit $(PAMOUTDIR)/pam_pin.so
TEST_TARGET = $(TESTBUILDDIR)/test_main

.PHONY: all clean test

all: $(TARGETS)

test: $(TEST_TARGET)
	./build/test/test_main

# Targets for executables
$(BINDIR)/ppedit: $(LIBS) $(BUILDDIR)/ppedit.o
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Target for PAM module
$(PAMOUTDIR)/pam_pin.so: $(LIBS) $(BUILDDIR)/pam_pin.o
	@mkdir -p $(PAMOUTDIR)
	$(CC) -o $@ $^ $(PAM_LDFLAGS)

# Compile library sources
$(BUILDDIR)/%.o: $(LIBDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile source files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Compile PAM module sources
$(BUILDDIR)/pam_pin.o: $(PAMDIR)/pinpam.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(PAM_CFLAGS) -c -o $@ $<

# Test targets
$(TESTBUILDDIR)/%.o: $(TESTDIR)/%.c
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(TEST_CFLAGS) -c -o $@ $<

$(TEST_TARGET): $(TESTBUILDDIR)/test_main.o $(TESTBUILDDIR)/users.o $(TESTBUILDDIR)/crypt.o $(LIBS)
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(TEST_CFLAGS) -o $@ $^ $(TEST_LDFLAGS)

# Compile test sources
$(TESTBUILDDIR)/test_main.o: $(TESTDIR)/test_main.c
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(TEST_CFLAGS) -c -o $@ $<

$(TESTBUILDDIR)/users.o: $(TESTDIR)/users.c
	@mkdir -p $(TESTBUILDDIR)
	$(CC) $(TEST_CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(BUILDDIR)/*.o $(BINDIR)/* $(PAMOUTDIR)/* $(TESTBUILDDIR)/*

# Check UID (root)
.PHONY: check-root
check-root:
	@if [ $$(id -u) -ne 0 ]; then \
		echo "This target must be run as root."; \
		exit 1; \
	fi

# Create necessary files and folders if they do not exist
.PHONY: create-files
create-files:
	@mkdir -pv /var/pinpam
	@mkdir -pv /etc/pinpam
	@if [ ! -f /var/pinpam/users ]; then \
		touch /var/pinpam/users; \
		echo "Created /var/pinpam/users"; \
	fi
	@if [ ! -f /etc/pinpam/users ]; then \
		touch /etc/pinpam/users; \
		echo "Created /etc/pinpam/users"; \
	fi

# Copy binaries to target locations
.PHONY: install-binaries
install-binaries: $(TARGETS)
	@cp -v $(BINDIR)/* $(BIN_DEST)
	@cp -v $(PAMOUTDIR)/* $(PAM_DEST)

.PHONY: install
install: check-root create-files install-binaries
	@echo "Installation completed"	

.PHONY: license
license:
	@cat LICENSE

