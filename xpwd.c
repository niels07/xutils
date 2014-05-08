#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "xlib.h"

#define PROGRAM_NAME "pwd"
#define VERSION 1.1
#define AUTHOR "niels@n-ve.be"

/* Get current working directory from the 
   PWD environment variable. */
static Option f_logical = 0;

/* Avoid symlinks if this flag is set. */
static Option f_physical = 0;

static void usage(void);

static Flag flags[] = {
    { "logical",  'L', &f_logical,  NULL  },
    { "physical", 'P', &f_physical, NULL  },
    { "help",     ' ', NULL,        usage },
    { NULL, 0, NULL, NULL }
};

static char *cwd;

static int
get_cwd_logical(void)
{
    cwd = getenv("PWD");
    if (cwd == NULL) {
        xerror("failed to get 'PWD'");
        return 0;
    }
    return 1;
}

static void 
usage(void)
{
    set_color(C_WHITE, CT_LIGHT);
    fprintf(stdout, "Usage: pwd [");

    set_color(C_RED, CT_LIGHT);
    fprintf(stdout, "OPTION");
    
    set_color(C_WHITE, CT_LIGHT);
    fprintf(stdout, "]... \n");

    set_color(C_WHITE, CT_LIGHT);
    fputs("Print the current working directory.\n\n", stdout);
    fputs("Options\n", stdout);

    lusage('L', "logical",  "Get cwd from the PWD environment variable");
    lusage('P', "physical", "avoid all symlinks");
    lusage( 0,  "help",     "display this help and exit");
    lusage( 0,  "version",  "output version information and exit");
    fputc('\n', stdout);

    set_color(C_WHITE, CT_LIGHT);
    fputs("Exit status:\n", stdout);
    fputs("  0  OK,\n", stdout);
    fputs("  1  minor problems (e.g., cannot access subdirectory), \n", stdout);
    fputs("  2  serious trouble (e.g., cannot access command-line argument).\n", stdout);
    fputc('\n', stdout);
    author();

    clear_color();
    exit(EXIT_SUCCESS);
}

static void 
print_cwd(void)
{
    size_t i, len;
    char c;

    len = strlen(cwd);
    set_color(C_WHITE, CT_LIGHT); 

    for (i = 0; i < len; ++i) {
        c = cwd[i];
        if (c == '/')
            set_color(C_RED, CT_NORMAL);
        fputc(c, stdout);
        if (c == '/')
            set_color(C_WHITE, CT_LIGHT); 
    }
    clear_color();
    fputc('\n', stdout);
}

static int
get_cwd(void)
{
    cwd = xmalloc(1024);
    if (getcwd(cwd, 1024) == NULL) {
        xerror("failed to get current directory");
        return 0;
    }
    return 1;
}

int 
pwd(char **args)
{
    int rval = 0;

    args = get_options(args, flags);
    if (*args != NULL) {
        xerror("too many arguments");
        return EXIT_FAILURE;
    }
    rval = f_logical ? get_cwd_logical() : get_cwd();
    if (rval) {
        print_cwd();
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

int 
main(int argc, char *argv[]) 
{
    return pwd(argv);
} 

