#include "systemcalls.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 * successfully using the system() call, false if an error occurred,
 * either in invocation of the system() call, or if a non-zero return
 * status was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    if (cmd == NULL) {
        return false;
    }

    int ret = system(cmd);
    
    if (ret == -1) {
        return false;
    }
    
    return true;
}

/**
* @param count - The numbers of variables passed to the function. The first argument
* should be the absolute path to the program to execute with execv()
* @param ... - A list of 1 or more arguments after the @param count argument.
* The first is the program to execute, and the following arguments are the arguments
* to be passed to the program.
* @return true if the command @param ... with arguments @param ... was executed successfully
* using the execv() call, false if an error occurred, either in invocation of the
* fork or execv() command, or if a non-zero return status was returned by the
* command issued in @param ...
*/
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    fflush(stdout);

    pid_t pid = fork();

    if (pid == -1) {
        va_end(args);
        return false;
    }
    else if (pid == 0) {
        execv(command[0], command);
        exit(EXIT_FAILURE);
    }
    else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            va_end(args);
            return false;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            va_end(args);
            return true;
        } else {
            va_end(args);
            return false;
        }
    }
}

/**
* @param outputfile - The full path to the file to write with command output.
* Passed to open(), see man pages of open() and dup2() for details.
* @param count - The numbers of variables passed to the function. The first argument
* should be the absolute path to the program to execute with execv()
* @param ... - A list of 1 or more arguments after the @param count argument.
* The first is the program to execute, and the following arguments are the arguments
* to be passed to the program.
* @return true if the command @param ... with arguments @param ... was executed successfully
* using the execv() call, false if an error occurred, either in invocation of the
* fork or execv() command, or if a non-zero return status was returned by the
* command issued in @param ...
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    fflush(stdout);

    pid_t pid = fork();

    if (pid == -1) {
        va_end(args);
        return false;
    }
    else if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            exit(EXIT_FAILURE);
        }

        if (dup2(fd, 1) < 0) {
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);

        execv(command[0], command);
        exit(EXIT_FAILURE);
    }
    else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            va_end(args);
            return false;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            va_end(args);
            return true;
        } else {
            va_end(args);
            return false;
        }
    }
}
