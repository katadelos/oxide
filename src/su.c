#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    const char* program;
    int firstArgument;
    if (argc == 3 && strcmp(argv[1], "-c") == 0) {
        program = "/bin/sh";
        firstArgument = 1;
    } else if (argc >= 4 && strcmp(argv[1], "-s") == 0 && strcmp(argv[3], "root") == 0) {
        program = argv[2];
        firstArgument = 4;
    } else {
        fputs("Usage: su -c COMMAND | su -s PROGRAM root [ARGS...]\n", stderr);
        return 2;
    }
    if (geteuid() != 0) {
        fputs("su: install this helper owned by root with mode 4755\n", stderr);
        return 1;
    }
    if (setgroups(0, NULL) != 0 || setgid(0) != 0 || setuid(0) != 0) {
        perror("su: cannot become root");
        return 1;
    }
    const struct passwd* root = getpwuid(0);
    if (root == NULL) {
        fputs("su: root account is missing\n", stderr);
        return 1;
    }
    char home[PATH_MAX + sizeof("HOME=")];
    snprintf(home, sizeof(home), "HOME=%s", root->pw_dir);
    char** arguments = (char**)calloc((size_t)argc + 1, sizeof(*arguments));
    if (arguments == NULL) {
        perror("su: cannot allocate arguments");
        return 1;
    }
    arguments[0] = (char*)program;
    for (int source = firstArgument, destination = 1; source < argc; ++source, ++destination) {
        arguments[destination] = argv[source];
    }
    char* const environment[] = {
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        home,
        "USER=root",
        "LOGNAME=root",
        "SHELL=/bin/sh",
        NULL};
    execve(program, arguments, environment);
    perror("su: cannot execute program");
    free((void*)arguments);

    enum {
        execFailure = 127
    };

    return execFailure;
}
