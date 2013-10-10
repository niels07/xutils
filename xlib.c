#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>

#include "xlib.h"

static const char *COLOR_FORMAT = "\033[%d;%dm";
static const char *COLOR_RESET  = "\033[0m";

void *
xmalloc(size_t size)
{
    void *ptr = malloc (size);

    if (ptr == NULL)    
    {
        fprintf(stderr, "fatal: memory exhausted (malloc of %u bytes).\n", size);
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void *
xrealloc(void *p, size_t size)
{
    void *ptr = realloc(p, size);

    if (ptr == NULL)    
    {
        free(p);

        fprintf(stderr, "fatal: memory exhausted (malloc of %u bytes).\n", size);
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void
xfree(void *p)
{
    if (p == NULL)
        return;
    free(p);
    p = NULL;
}

void 
xversion(void)
{
#ifdef PROGRAM_NAME
    fprintf(stdout, PROGRAM_NAME);
#endif
#ifdef VERSION
    fprintf(stdout, "%d", VERSION);
#endif
#ifdef COPYRIGHT
    fprintf(stdout, "\n" COPYRIGHT "\n");
#endif
#ifdef DESCRIPTION
    fprintf(stdout, "\n" DESCRIPTION);
#endif
    fputc('\n', stdout);
}

char *
dupstr(const char *str)
{
    char *dup;
    size_t len;

    len = strlen(str);
    
    dup = xmalloc(len + 1);
    
    strncpy(dup, str, len);
    dup[len] = '\0';
    
    return dup;
}

void 
xerror(const char *format, ...)
{
    char buf[255];
    va_list vl;

    va_start(vl, format);
    vsprintf(buf, format, vl);

#ifdef PROGRAM_NAME
    if (errno == 0)
        fprintf(stderr, PROGRAM_NAME ": %s\n", buf);
    else
        fprintf(stderr, PROGRAM_NAME ": %s: %s\n", buf, strerror(errno));
#else
    if (errno == 0)
        fprintf(stderr, "%s\n", buf);
    else
        fprintf(stderr, "%s: %s\n", buf, strerror(errno));
#endif

    errno = 0;
    
    va_end(vl);
}

int
streq(const char *s1, const char *s2)
{
    return (strcmp(s1, s2) == 0);
}

void
free_array(char **p, const size_t size)
{
    size_t i;

    for (i = size; i--;) 
        if (p[i]) free(p[i]);
    
    free(p);
}

void
set_color(const Color color, const Color_type type)
{
    fprintf(stdout, COLOR_FORMAT, type, color);
}

void
clear_color(void)
{
    fprintf(stdout, "%s", COLOR_RESET);
}

char *
color_string(const Color color, const Color_type type, const char *string)
{
    char *color_str;
    char color_code[25];

    sprintf(color_code, COLOR_FORMAT, type, color);

    color_str = xmalloc(strlen(string) + strlen(color_code) + strlen(COLOR_RESET) + 1);

    sprintf(color_str, "%s%s%s", color_code, string, COLOR_RESET);

    return color_str;
}

char *
color_num(const Color color, const Color_type type, const int num)
{
    char *color_str;
    char color_code[25];

    sprintf(color_code, COLOR_FORMAT, type, color);

    color_str = xmalloc(strlen(color_code) + strlen(COLOR_RESET) + 10);

    sprintf(color_str, "%s%d%s", color_code, num, COLOR_RESET);

    return color_str;
}

char *
color_char(const Color color, const Color_type type, const char c)
{
    char *color_str;
    char color_code[25];

    sprintf(color_code, COLOR_FORMAT, type, color);

    color_str = xmalloc(strlen(color_code) + strlen(COLOR_RESET) + 10);

    sprintf(color_str, "%s%c%s", color_code, c, COLOR_RESET);

    return color_str;
}

static void
ch_flag_short(char *arg, Flag *flags)
{
    char c;

    while ((c = *arg++) != '\0')
    {
        int found = 0;
        for (size_t i = 0; flags[i].flag != NULL || flags[i].function != NULL; ++i)
        {
            if (flags[i].ch != c)
            {
                continue;
            }

            if (flags[i].flag != NULL)
                *(flags[i].flag) = 1;

            if (flags[i].function != NULL)
                flags[i].function();

            found = 1;
            break;
        }

        if (!found)
        {
            xerror("invalid option -- '%c'", c);
            exit(EXIT_FAILURE);
        }
    }
}

static void
ch_flag_long(char *arg, Flag *flags)
{
    size_t i;
    int found = 0;

    for (i = 0; flags[i].flag != NULL || flags[i].function != NULL; ++i)
    {
        if (flags[i].string == NULL || !streq(flags[i].string, arg))
            continue;

        if (flags[i].flag != NULL)
            *(flags[i].flag) = 1;

        if (flags[i].function != NULL)
            flags[i].function();

        found = 1;
        break;

    }

    if (!found)
    {
        xerror("invalid option -- '%s'", arg);
        exit(EXIT_FAILURE);
    }
}

char **
get_options(char **args, Flag *flags)
{
    char **no_flags = args;
    size_t i = 0;

    for (++args; *args; ++args)
    {
        if (**args != '-' || *++*args == '\0')
        {
            no_flags[i++] = *args;
            continue;
        }

        if (**args != '-') 
            ch_flag_short(*args, flags);
        else
            ch_flag_long(++*args, flags);
    }

    no_flags[i] = NULL;
    return no_flags;
}

int 
count_digits(int num)
{
    int digits = 0;

    while (num > 0) 
    {
        num /= 10;
        digits++;
    }

    return digits;
}


char *
num_to_str(const int num)
{
    char *str;

    str = xmalloc(count_digits(num) + 1);

    sprintf(str, "%d", num);

    return str;
}

void
lusage(const char short_opt, const char *long_opt, const char *desc)
{
    size_t spaces;

    if (short_opt != 0)
    {
        set_color(C_BLUE, CT_LIGHT);

        fprintf(stdout, "    -%c", short_opt);

        set_color(C_WHITE, CT_LIGHT);

        fprintf(stdout, ", ");
    }
    else
        fprintf(stdout, "        ");

    if (long_opt != NULL)
    {
        set_color(C_GREEN, CT_LIGHT);
        fprintf(stdout, "--%s", long_opt);
 
        spaces = 16 - strlen(long_opt);
    }
    else
        spaces = 18;

    while (spaces--)
        fputc(' ', stdout);

    set_color(C_WHITE, CT_LIGHT);

    fprintf(stdout, "%s", desc);

    clear_color();

    fputc('\n', stdout);
}

Xpasswd *
get_passwd(uid_t uid)
{
    char line[255], **pline = NULL, chuid[50];
    FILE *fpwd;
    size_t i, j, k, len;
    Xpasswd *xpwd;

    if (!(fpwd = fopen("/etc/passwd", "r"))) 
    {
        fputs("Failed to open /etc/passwd\n", stderr);
        return NULL;
    }

    sprintf(chuid, "%d", uid);
    i = j = k = len = 0;

    while ((fgets(line, 255, fpwd))) 
    {
        pline = xmalloc(8 * sizeof(char *));
        len = strlen(line);
        pline[0] = xmalloc(len);

        for (i = j = k = 0; line[i]; ++i) 
        {
            if (line[i] != ':' && line[i] != '\0') 
            {
                pline[j][k++] = line[i];
                continue;
            }
                
            pline[j++][k] = '\0';
            pline[j] = xmalloc(len);
            k = 0;
        }
        pline[7] = NULL;

        if (streq(chuid, pline[2]))
            break;

        free_array(pline, 8);
    }
    fclose(fpwd);

    if (pline[j][k - 1])
        pline[j][k] = '\0';

    xpwd = xmalloc(sizeof(Xpasswd));

    xpwd->name = dupstr(pline[0]);
    xpwd->uid = atoi(pline[2]);
    xpwd->gid = atoi(pline[3]);
    xpwd->home = dupstr(pline[5]);
    xpwd->shell = dupstr(pline[6]);
    
    free_array(pline, 7);

    return xpwd;
}

Xgroup *
get_group(gid_t gid)
{
    char line[255], **pline = NULL, chgid[50];
    FILE *fgroup;
    size_t i, j, k, len;
    Xgroup *group;

    if (!(fgroup = fopen("/etc/group", "r"))) 
    {
        fputs("Failed to open /etc/group\n", stderr);
        return NULL;
    }

    sprintf(chgid, "%d", gid);
    i = j = k = len = 0;

    while ((fgets(line, 255, fgroup))) 
    {
        pline = xmalloc(5 * sizeof(char *));
        len = strlen(line);
        pline[0] = xmalloc(len);

        for (i = j = k = 0; line[i] != '\0'; ++i) 
        {
            if (line[i] == ':' || line[i] == '\0') 
            {
                pline[j++][k] = '\0';
                pline[j] = xmalloc(len);
                k = 0;
            } 
            else
                pline[j][k++] = line[i];
        }
        pline[4] = NULL;

        if (streq(chgid, pline[2]))
            break;

        free_array(pline, 4);
    }
    fclose(fgroup);

    if (pline[j][k - 1])
        pline[j][k] = '\0';

    group = xmalloc(sizeof(Xgroup));

    group->name = dupstr(pline[0]);
    group->pass = dupstr(pline[0]);
    group->gid = atoi(pline[2]);
    
    free_array(pline, 4);

    return group;
}

void
free_group(Xgroup *group)
{
    free(group->name);
    free(group->pass);
    free(group);
}

void
free_passwd(Xpasswd *xpwd)
{
    free(xpwd->name);
    free(xpwd->home);
    free(xpwd->shell);
    free(xpwd);
}
