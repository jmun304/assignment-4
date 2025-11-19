#define _XOPEN_SOURCE 700
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INPUT_LENGTH 2048
#define MAX_ARGS 512

int foreground_only = 0;

// Structure to store a command
struct command_line {
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    bool is_bg;
};

// Function to parse input
struct command_line *parse_input() {
    char input[INPUT_LENGTH];
    struct command_line *cmd = calloc(1, sizeof(struct command_line));

    printf(": ");
    fflush(stdout);
    fgets(input, INPUT_LENGTH, stdin);

    char *token = strtok(input, " \n");
    while (token) {
        if (!strcmp(token, "<")) {
            cmd->input_file = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, ">")) {
            cmd->output_file = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, "&") && strtok(NULL, " \n") == NULL) {
            // Only treat & as background if it is last token
            cmd->is_bg = true;
            break;
        } else {
            cmd->argv[cmd->argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }
    return cmd;
}

void handle_SIGTSTP(int signo) {
    char* msg;
    if (foreground_only == 0) {
        msg = "\nEntering foreground-only mode (& is now ignored)\n: ";
        foreground_only = 1;
    } else {
        msg = "\nExiting foreground-only mode\n: ";
        foreground_only = 0;
    }
    write(1, msg, strlen(msg));
}

int main() {
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};
    SIGINT_action.sa_handler = SIG_IGN; //No need to define handler
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    //No flags set
    sigfillset(&SIGINT_action.sa_mask);
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    SIGTSTP_action.sa_flags = 0;
    // Install our signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);    
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    struct command_line *cmd;
    int last_status = 0; // store status for "status" command

    while (1) {
        cmd = parse_input();

        // Skip blank lines or comments
        if (cmd->argc == 0 || cmd->argv[0][0] == '#') {
            free(cmd);
            continue;
        }

        // Built-in commands
        if (strcmp(cmd->argv[0], "exit") == 0) {
            // TODO: kill background processes if any
            for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
            free(cmd->input_file); free(cmd->output_file); free(cmd);
            exit(0);
        }
        else if (strcmp(cmd->argv[0], "cd") == 0) {
            if (cmd->argc == 1) {
                chdir(getenv("HOME"));
            } else {
                if (chdir(cmd->argv[1]) != 0) perror("cd failed");
            }
        }
        else if (strcmp(cmd->argv[0], "status") == 0) {
            printf("exit value %d\n", last_status);
        }

        if (foreground_only && cmd->is_bg) {
            cmd->is_bg = false;  
        }

        if (!cmd->is_bg || !foreground_only) {
            // Non built-in commands
            pid_t spawnpid = fork();
            if (spawnpid == -1) {
                perror("fork failed");
                last_status = 1;
            }
            else if (spawnpid == 0) {
                // Child process: handle input/output redirection
                if (cmd->is_bg) {
                    // Background: ignore SIGINT
                    signal(SIGINT, SIG_IGN);
                } else {
                    // Foreground: default SIGINT
                    signal(SIGINT, SIG_DFL);
                }

                // Always ignore SIGTSTP for children
                signal(SIGTSTP, SIG_IGN);

                if (cmd->input_file) {
                    int fd_in = open(cmd->input_file, O_RDONLY);
                    if (fd_in == -1) { perror("cannot open input file"); exit(1); }
                    dup2(fd_in, 0); close(fd_in);
                }
                if (cmd->output_file) {
                    int fd_out = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out == -1) { perror("cannot open output file"); exit(1); }
                    dup2(fd_out, 1); close(fd_out);
                }

                execvp(cmd->argv[0], cmd->argv);
                perror("exec failed"); // command not found
                exit(1);
            }
            else {
                // Parent: wait for foreground commands
                if (!cmd->is_bg) {
                    int status;
                    waitpid(spawnpid, &status, 0);
                    if (WIFEXITED(status)) {
                        last_status = WEXITSTATUS(status);
                    } else if (WIFSIGNALED(status)) {
                        int sig = WTERMSIG(status);
                        printf("terminated by signal %d\n", sig);
                        fflush(stdout);
                        last_status = sig;
                    }
                }
            }
        }

        // Free memory
        for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
        free(cmd->input_file); free(cmd->output_file); free(cmd);
    }

    return 0;
}
