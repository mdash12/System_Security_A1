CC=gcc
GRP_ID = 430
CFLAGS=-g -Wall -m32 -DGRP=$(GRP_ID) -DLEN1=1021 -DLEN2=256 -DLEN3=256 -DRANDOM=0
#CFLAGS=-g -Wall -DLEN1=1021 -DLEN2=256 -DLEN3=1000 -DRANDOM=random\(\)

all: vuln.s vuln driver driver_authd_expl 2a_stack_smashing_basic 2b_stack_smash_return_to_libc 2c_stack_smash_injected_code 2d_stack_smash_sys_call 3_format_string_attack 4a_heap_overflow_free

vuln: vuln.o my_malloc.o
	$(CC) $(CFLAGS) -o vuln vuln.o my_malloc.o
	execstack -s vuln

vuln.o: padding.h vuln.c my_malloc.h
	$(CC) $(CFLAGS) -c vuln.c

vuln.s: vuln.c my_malloc.h
	$(CC) $(CFLAGS) -DASM_ONLY -c -g -Wa,-a,-ad vuln.c > vuln.s
	rm vuln.o

my_malloc.o: my_malloc.h my_malloc.c
	$(CC) $(CFLAGS)  -c my_malloc.c

driver: driver.c
	$(CC) $(CFLAGS) -o driver driver.c

driver_authd_expl: driver_authd_expl.c
	$(CC) $(CFLAGS) -o driver_authd_expl driver_authd_expl.c

2a_stack_smashing_basic: 2a_stack_smashing_basic.c
	$(CC) $(CFLAGS) -o 2a_stack_smashing_basic 2a_stack_smashing_basic.c

2b_stack_smash_return_to_libc: 2b_stack_smash_return_to_libc.c
	$(CC) $(CFLAGS) -o 2b_stack_smash_return_to_libc 2b_stack_smash_return_to_libc.c

2c_stack_smash_injected_code: 2c_stack_smash_injected_code.c
	$(CC) $(CFLAGS) -o 2c_stack_smash_injected_code 2c_stack_smash_injected_code.c

2d_stack_smash_sys_call: 2d_stack_smash_sys_call.c
	$(CC) $(CFLAGS) -o 2d_stack_smash_sys_call 2d_stack_smash_sys_call.c

3_format_string_attack: 3_format_string_attack.c
	$(CC) $(CFLAGS) -o 3_format_string_attack 3_format_string_attack.c

4a_heap_overflow_free: 4a_heap_overflow_free.c
	$(CC) $(CFLAGS) -o 4a_heap_overflow_free 4a_heap_overflow_free.c

padding.h:
	./mkpad $(GRP_ID)

clean:
	rm -f vuln vuln.o my_malloc.o vuln.s padding.h driver_authd_expl driver 2a_stack_smashing_basic 2b_stack_smash_return_to_libc 2c_stack_smash_injected_code 2d_stack_smash_sys_call 3_format_string_attack 4a_heap_overflow_free
