### Command to compile the files:
```sh
make clean all
```

### Run an exploit:
 * Basic Stack overflow attack:
 ```sh
    ./2a_stack_smashing_basic 
 ```
 * Return-to-libc attack: 
 ```sh
    ./2b_stack_smash_return_to_libc 
 ```
 * Stack smashing injected code attack: 
 ```sh
    ./2c_stack_smash_injected_code
 ```
 * Format string attack:
 ```sh
    ./3_format_string_attack 
 ```
 * Heap overflow attack using vulnerability in free: 
 ```sh
    ./4a_heap_overflow_free
 ```
 
### Assembly code Compilation: 
 ```sh
as -a --32 assembly.s -o assem.out
 ```
 
### Get the assembly code: 
 ```sh
objdump -d assem.out
 ```

