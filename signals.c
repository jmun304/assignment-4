#define _XOPEN_SOURCE 700
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Our signal handler for SIGINT */
void handle_SIGINT(int signo){
  char* message = "Caught SIGINT, sleeping for 10 seconds\n";
  // We are using write rather than printf
  write(STDOUT_FILENO, message, 39);
  sleep(10);
}

int main(){
  // Initialize SIGINT_action struct to be empty
  struct sigaction SIGINT_action = {0};

  // Fill out the SIGINT_action struct
  // Register handle_SIGINT as the signal handler
  SIGINT_action.sa_handler = handle_SIGINT;
  // Block all catchable signals while handle_SIGINT is running
  sigfillset(&SIGINT_action.sa_mask);
  // No flags set
  SIGINT_action.sa_flags = 0;

  // Install our signal handler
  sigaction(SIGINT, &SIGINT_action, NULL);

  printf("Send the signal SIGINT to this process by entering Control-C. \
    That will cause the signal handler to be invoked\n");
  fflush(stdout);

  // Calling pause() causes the process to sleep until it is sent a signal.
  // Enter Control-C to end pause
  pause();
  // If pause is ended by SIGINT, then after handle_SIGINT ends, the execution
  // proceeds with the following statement.
  printf("pause() ended. The process will now end.\n");
  return 0;