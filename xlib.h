#ifndef XUTILS_LIB_H
#define XUTILS_LIB_H

#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>

struct xpasswd 
{
    char *name;
    char *home;
    char *shell;
    gid_t gid;
    uid_t uid;
};

typedef struct xpasswd Xpasswd;

struct xgroup
{
    char *name;
    char *pass;
    gid_t gid;
};

typedef struct xgroup Xgroup;

enum color
{
    C_BLACK = 30,
    C_RED, 
    C_GREEN,
    C_BROWN,
    C_BLUE,
    C_PURPLE,
    C_CYAN,
    C_WHITE
};

typedef enum color Color;

enum color_type
{
    CT_LIGHT,
    CT_NORMAL,
    CT_DARK
};

typedef enum color_type Color_type;

typedef void (*Function)(void);

typedef unsigned short int Option;

#define COLOR_SIZE 11

struct flag
{
    char *string;
    char ch;
    Option *flag;
    Function function;
};

typedef struct flag Flag;

extern char **get_options(char **args, Flag *);

extern void xerror(const char *, ...);

extern void *xmalloc(size_t);
extern void *xrealloc(void *, size_t );
extern void xfree(void *);

extern char *dupstr(const char *);
extern int streq(const char *, const char *);
extern char *num_to_str(const int);
extern int count_digits(int);
extern void free_array(char **, const size_t);

extern char *color_string(const Color, const Color_type, const char *);
extern char *color_num(const Color, const Color_type, const int);
extern char *color_char(const Color , const Color_type , const char );
extern void set_color(const Color , const Color_type );
extern void clear_color(void);

extern Xpasswd *get_passwd(uid_t);
extern Xgroup *get_group(gid_t );

extern void xversion(void);
extern void lusage(const char, const char *, const char *);

extern void free_group(Xgroup *);
extern void free_passwd(Xpasswd *);

#endif /* XUTILS_LIB_H */

