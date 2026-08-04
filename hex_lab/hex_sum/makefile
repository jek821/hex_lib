CFLAGS = -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Wpedantic -Wshadow \
         -Wstrict-prototypes -Wmissing-prototypes -Wconversion -pthread
LDLIBS = -lsodium

hex_sum: hex_sum.c ../../hex_lib/hex_debug/hex_debug.c 

clean: 
	rm -f hex_sum
