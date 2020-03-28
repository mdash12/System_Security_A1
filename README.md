Command to compile the files:
make clean all

Run an exploit:

Basic Stack overflow attack: ./2a_stack_smashing_basic
Return-to-libc attack:  ./2b_stack_smash_return_to_libc
Stack smashing injected code attack: ./2c_stack_smash_injected_code
Format string attack: ./3_format_string_attack
Heap overflow attack using vulnerability in free:  ./4a_heap_overflow_free

Assembly code Compilation: 
as -a --32 assembly.s -o assem.out

Get the assembly code:
objdump -d assem.out
