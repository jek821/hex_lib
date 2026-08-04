### Compiling:
`gcc hex_sum.c ../../hex_lib/hex_debug/hex_debug.c -lsodium -o main`

#### Compiling for debugger:
`gcc -g -O0 hex_sum.c ../../hex_lib/hex_debug/hex_debug.c -pthread -lsodium -o hex_sum`

#### Run binary with debug enabled:
`./main --hex_debug`


