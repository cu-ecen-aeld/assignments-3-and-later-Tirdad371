#include "systemcalls.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>

/**
 * Execute a command using system()
 */
bool do_system(const char *cmd)
{
    if (cmd == NULL) {
        return false;
    }

    int status = system(cmd);

    if (status == -1) {
        return false;
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/**
 * Execute a command using fork(), execv(), and waitpid()
 */
bool do_exec(int count, ...)
{
    if (count < 1) {
        return false;
    }

    va_list args;
    va_start(args, count);

    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    pid_t pid = fork();
    if (pid == -1) {
        return false;
    }

    if (pid == 0) {
        execv(command[0], command);
        exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/**
 * Execute a command and redirect stdout to outputfile
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    if ((outputfile == NULL) || (count < 1)) {
        return false;
    }

    va_list args;
    va_start(args, count);

    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    pid_t pid = fork();
    if (pid == -1) {
        return false;
    }

    if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            exit(EXIT_FAILURE);
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);

        execv(command[0], command);
        exit(EXIT_FAILURE);
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

