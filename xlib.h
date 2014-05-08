#ifndef XUTILS_LIB_H
#define XUTILS_LIB_H

#include <stdlib.h>
#include <pwd.h>
#include <sys/types.h>

extern char *x_program_name;
extern char *x_version;
extern char *x_author;

typedef struct {
    char *name;
    char *home;
    char *shell;
    gid_t gid;
    uid_t uid;
} Xpasswd;

typedef struct{
    char *name;
    char *pass;
    gid_t gid;
} Xgroup;

typedef enum {
    C_BLACK = 30,
    C_RED, 
    C_GREEN,
    C_BROWN,
    C_BLUE,
    C_PURPLE,
    C_CYAN,
    C_WHITE
} Color;


typedef enum {
    INFO_AUTHOR,
    INFO_PROGRAM_NAME,
    INFO_VERSION
} Info_type;


typedef enum {
    CT_LIGHT,
    CT_NORMAL,
    CT_DARK
} Color_type;

typedef void (*Function)(void);
typedef unsigned short int Option;

#define COLOR_SIZE 11

typedef struct {
    char *string;
    char ch;
    Option *flag;
    Function function;
} Flag;

extern char **get_options(char ** /* args */, Flag * /* flag */);

extern void xerror(const char * /* format */, ...);

extern void *xmalloc(size_t /* size */);
extern void *xrealloc(void * /* p */, size_t /* size */);

extern char *dupstr(const char * /* string */);
extern int streq(const char * /* str1 */, const char * /* str2 */);
extern char *num_to_str(const int /* number */);
extern int count_digits(int /* number */);
extern void free_array(char ** /* array */, const size_t /* size */);
extern char *color_string(const Color /* color */, const Color_type /* type */, const char * /* string */);
extern char *color_num(const Color /* color */, const Color_type /* type */, const int /* number */);
extern char *color_char(const Color /* color */, const Color_type /* type */, const char /* character */);
extern void set_color(const Color /* color */, const Color_type /* type */);
extern void clear_color(void);

extern Xpasswd *get_passwd(uid_t /* uid */);
extern Xgroup *get_group(gid_t /* gid */);

extern void xversion(const char * /* program_name */, const char * /* version */, const char * /* copyright */, const char * /* description */);
extern void lusage(const char /* short_flag */, const char * /* long_flag */, const char * /* message */);
extern void author(void);

extern void free_group(Xgroup * /* group */);
extern void free_passwd(Xpasswd * /* pwd */);

#endif /* XUTILS_LIB_H */

