#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <pwd.h>
#include <grp.h>

#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PROGRAM_NAME "ls"
#define VERSION "1.1"
#define AUTHOR "niels@n-ve.be"
#define DESCRIPTION ""

#include "xlib.h"

enum {
    DT_UNKNOWN = 0,
    DT_FIFO = 1,
    DT_CHR = 2,
    DT_DIR = 4,
    DT_BLK = 6,
    DT_REG = 8,
    DT_LNK = 10,
    DT_SOCK = 12,
    DT_WHT = 14
};

typedef enum {
    /* Block device. */
    FT_BLOCK,

    FT_CHAR,

    /* Directory. */
    FT_DIR,

    /* First In First Out. */
    FT_FIFO,

    /* Symbolic link. */
    FT_LINK,

    /* Regular file. */
    FT_REG,

    /* Socket. */
    FT_SOCK,

    /* Whiteout */
    FT_WHITE,

    /* Executable. */
    FT_EXEC,

    /* Unkown, for when we can't determine the filetype. */
    FT_UNKOWN
} Filetype;

struct file_data
{
    /* Filename. */
    char *name;

    /* File mode as a string, type - permissions. */
    char **mode;

    /* User who owns the file. */
    char *user;

    /* Group that owns the file. */
    char *group;

    /* Number of links. */
    int nlink;

    /* Filesize in bytes. */
    long fsize;

    /* Filetype (see enum filetype) */
    Filetype type;

    /* Time last modified. */
    char *mtime;

    /* Length of the filename. */
    size_t nlen;

    /* File indicator (see f_no_classify). */
    char indicator;
};

typedef struct file_data File_data;

struct dir_data 
{
    /* Full path to this directory. */
    char *path;

    /* Files in this directory. */
    File_data **files;

    /* Size of files. */
    size_t num_files;

    /* Maximum length of each type to order the columns
       when long format is set. */
    size_t luser, lgroup, lnlink, lfsize, lname;

    /* Number of rows when displaying the files horizontally. */
    size_t num_rows;
 
    /* Number of columns when displaying the files horizontally. */
    size_t num_cols;

    /* Columns for displaying horizontally .*/
    File_data ***columns;

    /* Maximum width per column */
    size_t *max_per_col;

    /* Current row when printing. */
    size_t current_row;

    /* Current col when printing. */
    size_t current_col;

    /* Current file to print. */
    size_t current_file;
};

typedef struct dir_data Dir_data;

/* Directories to list. */
static Dir_data **dirs = NULL;

/* Total number of directories. */
static size_t num_dirs = 0;

/* Filetypes to ignore if specified. */
enum {
    /* Hide hidden files. All files starting with 
       '.' (set by default). */
    I_HIDDEN = 0x01,

    /* Hide directories. */
    I_DIR = 0x02,

    /* Hide '.' and '..' */
    I_DOTS = 0x04,

    /* Hide all other files. */ 
    I_REG = 0x08
};

/* File types to ignore when listing. */
static int ignore_files = I_HIDDEN;

/* Print the files on a new line. */
static int print_file_nl = 0;

/* When set to non zero, print the contents of 
   each directory as well. */
static Option f_recursive = 0;

/* Width of the window we're working in. */
static size_t window_width = 0;

/* Show "hidden" files starting with '.'. */
static Option f_show_hidden = 0;

/* Show full file info. */
static Option f_long_format = 0;

/* No coloured output. */
static Option f_no_color = 0;

/* Print perms in numerical format. */ 
static Option f_numeric_perms = 0;

/* Print the group and user id instead of name. */
static Option f_print_owner_id;

/* Display file size in human readable format. */
static Option f_human_readable = 0;

/* If set to zero (default) add file indicator
   '/' => directory,
   '*' => executable,
   '|' -> FIFO,
   '=' => socket,
   '@' => symlink */

static Option f_no_classify = 0;

static void
version(void)
{
    xversion(PROGRAM_NAME, VERSION, AUTHOR, DESCRIPTION);
}

static void 
usage(void)
{
    set_color(C_WHITE, CT_LIGHT);
    fprintf(stdout, "Usage: ls [");

    set_color(C_RED, CT_LIGHT);
    fprintf(stdout, "OPTIONS");
    
    set_color(C_GREEN, CT_LIGHT);
    fprintf(stdout, "]... [");

    set_color(C_RED, CT_LIGHT);
    fprintf(stdout, "FILE");
    
    set_color(C_GREEN, CT_LIGHT);
    fprintf(stdout, "]... \n");

    set_color(C_WHITE, CT_LIGHT);
    fputs("Prints the files in the current or specified directory\n\n", stdout);
    fputs("static Options\n", stdout);

    lusage('A', "almost-all",      "show directories starting with a '.' except '.' or '..'");
    lusage( 0,  "author",          "with -l, print the author of each file");
    lusage('c', "ignore-backups",  "ignore directories starting with '~'");
    lusage('C', "no-color",        "output without color");
    lusage('d', "directory",       "list directories only");
    lusage('G', "no-group",        "in a long listing, don't print group names");
    lusage('h', "human-readable",  "with -l, print sizes in human readable format");
    lusage('i', "inode",           "print the index number of each file");
    lusage('I', "ignore=PATTERN",  "do not list implied entries matching shell PATTERN");
    lusage('l', NULL,              "use a long format.");
    lusage('m', NULL,              "fill width with a comma separated list of entries");
    lusage('n', "numeric-uid-gid", "with -l, numeric user and group IDs");
    lusage('r', "reverse",         "reverse order while sorting");
    lusage('R', "recursive",       "list subdirectories recursively");
    lusage( 0,  "help",            "display this help and exit");
    lusage( 0,  "version",         "output version information and exit");
    fputc('\n', stdout);

    set_color(C_WHITE, CT_LIGHT);
    fputs("Exit status:\n", stdout);
    fputs("  0  OK,\n", stdout);
    fputs("  1  minor problems (e.g., cannot access subdirectory), \n", stdout);
    fputs("  2  serious trouble (e.g., cannot access command-line argument).\n", stdout);
    fputc('\n', stdout);
    fputs("Report bugs to niels07@live.com\n", stdout);

    clear_color();
    exit(EXIT_SUCCESS);
}

static void 
set_almost_all(void)
{
    ignore_files |= I_DOTS;
}

static void
set_all(void)
{
    ignore_files &= ~I_HIDDEN;
}

static void
set_no_directories(void)
{
    ignore_files |= I_DIR;
}

static void 
set_no_files(void)
{
    ignore_files |= I_REG;
}

static Flag flags[] = {
    { "all",            'a', &f_show_hidden    , set_all            },
    { "almost-al",      'A', NULL,               set_almost_all     },
    { "directory",      'd', NULL,               set_no_files       },
    { "no-directories", 'D', NULL,               set_no_directories },
    { "no_classify",    'F', &f_no_classify,     NULL     },
    { NULL,             'l', &f_long_format    , NULL     },
    { "no-color",       'C', &f_no_color       , NULL     },
    { "num-perms",      'N', &f_numeric_perms  , NULL     },
    { "recursive",      'R', &f_recursive      , NULL     },
    { "numeric-uid-gid",'n', &f_print_owner_id , NULL     }, 
    { "human-readable", 'h', &f_human_readable , NULL     },
    { "version",        ' ', NULL,               version  },
    { "help",           ' ', NULL,               usage    },
    { NULL, 0, NULL, NULL }
};

static char **
get_mode_string(struct stat st)
{
    char **str;
    mode_t st_mode;
   
    str = xmalloc(4 * sizeof(char *));
    st_mode = st.st_mode;

    /* Type of file. */
    str[0] = xmalloc(2);
    str[0][0] = '-';
    if (S_ISDIR(st_mode)) str[0][0] = 'd'; 
    if (S_ISCHR(st_mode)) str[0][0] = 'c'; 
    if (S_ISBLK(st_mode)) str[0][0] = 'b'; 
    str[0][1] = '\0';

    /* User permissions. */
    str[1] = xmalloc(4);
    str[1][0] = (st_mode & S_IRUSR) ? 'r' : '-'; 
    str[1][1] = (st_mode & S_IWUSR) ? 'w' : '-';
    str[1][2] = (st_mode & S_IXUSR) ? 'x' : '-';
    str[1][3] = '\0';
    
    /* Group permissions. */
    str[2] = xmalloc(4);
    str[2][0] = (st_mode & S_IRGRP) ? 'r' : '-'; 
    str[2][1] = (st_mode & S_IWGRP) ? 'w' : '-';
    str[2][2] = (st_mode & S_IXGRP) ? 'x' : '-';
    str[2][3] = '\0';
    
    /* Other permissions. */
    str[3] = xmalloc(4);
    str[3][0] = (st_mode & S_IROTH) ? 'r' : '-';
    str[3][1] = (st_mode & S_IWOTH) ? 'w' : '-';
    str[3][2] = (st_mode & S_IXOTH) ? 'x' : '-';
    str[3][3] = '\0';

    return str;
}

static unsigned short int
get_mode_num(const char *perm)
{
    if (streq(perm, "--x"))
        return 1;
    else
    if (streq(perm, "-w-"))
        return 2;
    else
    if (streq(perm, "-wx"))
        return 3;
    else
    if (streq(perm, "r--"))
        return 4;
    else
    if (streq(perm, "r-x"))
        return 5;
    else
    if (streq(perm, "rw-"))
        return 6;
    else
    if (streq(perm, "rwx"))
        return 7;

    return 0;
}

static char *
color_mode(const char *mode)
{
    Color color = C_WHITE;

    if (streq(mode, "r--"))
        color = C_GREEN;
    else
    if (streq(mode, "rw-"))
        color = C_BLUE;
    else
    if (streq(mode, "rwx"))
        color = C_CYAN;
    else
    if (streq(mode, "r-x"))
        color = C_BROWN;

    return color_string(color, CT_LIGHT, mode);

}

static char *
color_mode_num(short unsigned int mode)
{
    char color_str[2];
    Color color = C_WHITE;
    Color_type type = CT_LIGHT;

    sprintf(color_str, "%d", mode);

    switch (mode)
    {
    case 0:
        color = C_RED;
        break;

    case 1:
        color = C_GREEN;
        type = CT_DARK;
        break;

    case 2:
        color = C_BROWN;
        type = CT_DARK;
        break;

    case 3:
        color = C_PURPLE;
        break;

    case 4:
        color = C_GREEN;
        break;

    case 5:
        color = C_BROWN;
        break;
   
    case 6:
        color = C_BLUE;
        break;

    case 7:
        color = C_CYAN;
        break;

    default:
        break;
    
    }

    return color_string(color, type, color_str);
}

static void
indent(size_t len)
{
    int i = len;
    do 
    {
        putchar(' ');
    } while (--i > 0);
}

static char * 
human_readable(double size)
{
    int i = 0;
    const char* units[] = {" B", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    char *hr;

    while (size > 1024) 
    {
        size /= 1024;
        i++;
    }

    hr = xmalloc(count_digits(size) + 7);
    
    sprintf(hr, "%.*f %s", i, size, units[i]);
    
    return hr;
}

static char
get_indicator(const int type)
{
    char ind = 0;
 
    switch (type)
    {
        case FT_DIR:
            ind = '/';
            break;

        case FT_FIFO:
            ind = '|';
            break;

        case FT_LINK:
            ind = '@';

            break;
        case FT_SOCK:
            ind = '=';

            break;
        case FT_WHITE:
            ind = '>';
            break;

        case FT_EXEC:
            ind = '*';
            break;

        default:
            break;
    }

    return ind;
}

static Dir_data *
new_dir(void)
{
    Dir_data *dir;

    dir = xmalloc(sizeof(Dir_data));

    dir->num_files = 0;
    dir->luser = dir->lgroup = dir->lnlink = dir->lfsize = dir->lname = 0;
    dir->num_rows = 1;
    dir->current_row = dir->current_col = 0;
    dir->current_file = 0;
    dir->path = NULL;
    dir->files = NULL;

    return dir;
}

static void 
print_file(Dir_data *dir)
{
    char *name, *m_type, *m_user, *m_group, *m_other,
         *nlink, *user, *group, *fsize, *mtime, *tmp,
         *ind;
    Color color = C_WHITE;
    Color_type color_type = CT_LIGHT;
    File_data *file;

    if (f_long_format || print_file_nl)
        file = dir->files[dir->current_file++];
    else
    {
        if (dir->columns[dir->current_col] == NULL)
        {
            dir->current_row++;
            dir->current_col = 0;
        }
        
        if (dir->columns[dir->current_col][dir->current_row] == NULL)
            return;
        
        if (dir->current_col == 0 && dir->current_row > 0)
            putchar('\n');

        file = dir->columns[dir->current_col][dir->current_row];
    }

    if (f_no_color)
    {
        if (!f_no_classify && file->indicator != 0)
        {
            name = xmalloc(file->nlen + 2);
            sprintf(name, "%s%c", file->name, file->indicator);
        }
        else
            name = dupstr(file->name);
        
        m_type  = dupstr(file->mode[0]);
        m_user  = dupstr(file->mode[1]);
        m_group = dupstr(file->mode[2]);
        m_other = dupstr(file->mode[3]);

        user = dupstr(file->user);
        group = dupstr(file->group);
        mtime = dupstr(file->mtime);

        if (f_human_readable)
            fsize = human_readable(file->fsize);
        else 
        {
            fsize = xmalloc(10);
            sprintf(fsize, "%ld", file->fsize);
        }

        nlink = xmalloc(10);
        sprintf(nlink, "%d", file->nlink);
    }
    else
    {
        switch (file->mode[0][0])
        {
        case 'd':
            color = C_PURPLE;
            break;
        case 'c':
            color = C_BROWN;
            break;
        case 'b':
            color = C_RED;
            break;
        default:
            break;
        }
        m_type = color_string(color, CT_LIGHT, file->mode[0]);
        if (f_numeric_perms)
        {
            m_user = color_mode_num(get_mode_num(file->mode[1]));
            m_group = color_mode_num(get_mode_num(file->mode[2]));
            m_other = color_mode_num(get_mode_num(file->mode[3]));
        }
        else
        {
            m_user = color_mode(file->mode[1]);
            m_group = color_mode(file->mode[2]);
            m_other = color_mode(file->mode[3]);
        }

        switch (file->type)
        {
            case FT_BLOCK:
                color = C_BLUE;
                color_type = CT_NORMAL;
                break;

            case FT_CHAR:
                color = C_GREEN;
                color_type = CT_NORMAL;
                break;

            case FT_DIR:
                color = C_BROWN;
                color_type = CT_LIGHT;
                break;

            case FT_FIFO:
                color = C_BROWN;
                color_type = CT_NORMAL;
                break;

            case FT_LINK:
                color = C_BLUE;
                color_type = CT_LIGHT;
                break;

            case FT_SOCK:
                color = C_WHITE;
                color_type = CT_NORMAL;
                break;

            case FT_WHITE:
                color = C_RED;
                color_type = CT_LIGHT;
                break;

            case FT_EXEC:
                color = C_CYAN;
                color_type = CT_LIGHT;
                break;

            default:
                break;
        }
  
        if (!f_no_classify && file->indicator != 0)
        {
            ind = color_char(C_RED, CT_LIGHT, file->indicator);
            tmp = xmalloc(file->nlen + strlen(ind) + 1);
            sprintf(tmp, "%s%s", file->name, ind);
            name = color_string(color, CT_LIGHT, tmp);
            free(tmp);
        }
        else
            name  = color_string(color, CT_LIGHT,  file->name);
        
        user  = color_string(C_GREEN, CT_NORMAL, file->user);
        group = color_string(C_GREEN, CT_NORMAL, file->group);
        mtime = color_string(C_RED,   CT_NORMAL, file->mtime);
  
        if (f_human_readable)
            tmp = human_readable(file->fsize);
        else 
        {
            tmp = xmalloc(10);
            sprintf(tmp, "%ld", file->fsize);
        }

        fsize = color_string(C_WHITE, CT_NORMAL, tmp);
        free(tmp);

        nlink = color_num(C_WHITE, CT_NORMAL, file->nlink);
    }

    if (f_long_format)
    {
        fprintf(stdout, "%s%s%s%s %*s %*s %*s %*s %.19s %s\n", 
                m_type, m_user, m_group, m_other, 
                dir->lnlink, nlink, 
                dir->luser, user, 
                dir->lgroup, group,
                dir->lfsize, fsize, 
                mtime, name);
    }
    else
    if (print_file_nl)
    {
        fprintf(stdout, "%s\n", name); 
    }
    else 
    {
        fprintf(stdout, "%s", name); 
        indent(dir->max_per_col[dir->current_col] -    
               dir->columns[dir->current_col][dir->current_row]->nlen +  1);
        dir->current_col++;
    }

    free(name);
    free(user);
    free(group);
    free(mtime);
    free(fsize);
    free(nlink);
    free(m_user);
    free(m_group);
    free(m_other);
    free(m_type);
}

static void
prepare_columns(Dir_data *dir)
{
    size_t i, j, k;

    dir->num_cols = dir->num_files / dir->num_rows;

    dir->columns = xmalloc((dir->num_cols + 1) * sizeof(File_data **));
    dir->max_per_col = xmalloc((dir->num_cols + 2) * sizeof(size_t));

    for (i = k = 0; i < dir->num_cols; ++i)
    {
        dir->columns[i] = xmalloc((dir->num_rows + 1) * sizeof(char *));
        dir->max_per_col[i] = 0;

        for (j = 0; j < dir->num_rows && k < dir->num_files; ++j)
        {
            if (dir->max_per_col[i] < dir->files[k]->nlen)
                dir->max_per_col[i] = dir->files[k]->nlen;

            dir->columns[i][j] = dir->files[k++];
        }

        dir->columns[i][j] = NULL;
    }
        
    dir->columns[i] = NULL;
}

static void
print_files(Dir_data *dir)
{
    size_t i;

    if (!f_long_format && !print_file_nl)
        prepare_columns(dir);
    else
        dir->columns = NULL;

    if (f_human_readable)
        dir->lfsize = 7; 

    if (!f_no_color) {
        dir->luser += 11;
        dir->lgroup += 11;
        dir->lnlink += 11; 
        dir->lfsize += 11;
        dir->lname += 11;
    }

    for (i = 0; i < dir->num_files; ++i)
        print_file(dir);

    if ((!print_file_nl && !f_long_format))
        fputc('\n', stdout);

    if (f_recursive)
        fputc('\n', stdout);

}

static void
free_dirs(void)
{
    size_t i, j;
    File_data *file;

    for (i = num_dirs; i--;) {
        for (j = dirs[i]->num_files; j--;) {
            file = dirs[i]->files[j];
            free(file->name);
            free(file->user);
            free(file->group);
            free_array(file->mode, 4);
            free(file);
        }
        free(dirs[i]->files);
        for (j = dirs[i]->num_cols; j--;)
            free(dirs[i]->columns[j]);
        
        free(dirs[i]->columns);
        free(dirs[i]->path);
        free(dirs[i]->max_per_col);
        free(dirs[i]);
    }
    free(dirs);
}

static int
sort_by_name(const void *v1, const void *v2)
{
    const File_data *file1 = *(const File_data **)v1;
    const File_data *file2 = *(const File_data **)v2;

    return strcmp(file1->name, file2->name);
}

static char *
get_user_name(struct stat st)
{
    char *name = NULL; 
    uid_t uid = st.st_uid;
    Xpasswd *xpwd;

    if (f_print_owner_id)
        return num_to_str(uid);

	if ((xpwd = get_passwd(uid)) == NULL) {
        name = xmalloc(10);
		sprintf(name, "%d", uid);
	}
	else {
		name = dupstr(xpwd->name);
        free_passwd(xpwd);
    }

    return name;
}

static char *
get_group_name(struct stat st)
{
    char *name = NULL; 
    gid_t gid = st.st_gid;
    Xgroup *group;

    if (f_print_owner_id)
        return num_to_str(gid);

	if ((group = get_group(gid)) == NULL) {
        name = xmalloc(10);
		sprintf(name, "%d", gid);
	}
	else {
		name = dupstr(group->name);
        free_group(group);
    }

    return name;
}
static Filetype
get_filetype(unsigned char t)
{
    switch (t) {
    case DT_BLK:
        return FT_BLOCK;
    case DT_CHR: 
        return FT_CHAR;
    case DT_DIR:
        return FT_DIR;
    case DT_FIFO:
        return FT_FIFO;
    case DT_LNK:
        return FT_LINK;
    case DT_REG:
        return FT_REG;
    case DT_SOCK:
        return FT_SOCK;
    case DT_WHT: 
        return FT_WHITE;
    default:
        break;
    }
    return FT_UNKOWN;
}

static void
store_longest(Dir_data *dir, File_data *file)
{
    int count;

    if (file->nlen > dir->lname)
        dir->lname = file->nlen;
    
    if ((count = count_digits(file->nlink)) > dir->lnlink)
        dir->lnlink = count;

    if ((count = strlen(file->user)) > dir->luser)
        dir->luser = count;

    if ((count = strlen(file->group)) > dir->lgroup)
        dir->lgroup = count;

    if ((count = count_digits(file->fsize)) > dir->lfsize)
        dir->lfsize = count;
}

static int 
is_dir(const char *f)
{
    DIR *dir;

    dir = opendir(f);
    if (dir == NULL) {
        errno = 0;
        return 0;
    }
    closedir(dir);
    return 1;
}

static int
ignore_file(const char *f)
{
    int isdir;

    isdir = is_dir(f);
    return (
        (ignore_files & I_HIDDEN 
            && f[0] == '.') ||
        (ignore_files & I_DOTS   
            && (streq(f, ".") || streq(f, ".."))) ||
        (ignore_files & I_DIR
            && isdir) ||
        (ignore_files & I_REG
            && !isdir));
}

static Dir_data *
get_files(const char *path)
{
    const size_t ALLOC = 25;
    size_t i, j, path_len, 
           row_len = 0;
    DIR *d;
    char *fpath;
    struct dirent *de;
    struct stat st;
    struct winsize w;
    Dir_data *dir;

    if ((d = opendir(path)) == NULL) {
        xerror("Failed to read '%s'", path);
        return 0;
    }

    dir = new_dir();
    dir->files = xmalloc(ALLOC * sizeof(File_data *));

    path_len = strlen(path);
    ioctl(0, TIOCGWINSZ, &w);
    window_width = w.ws_col;

    for (i = 0, j = 1; (de = readdir(d)) != NULL; ++i) {
        if (ignore_file(de->d_name)) {
            --i;
            continue;
        }

        fpath = xmalloc(path_len + strlen(de->d_name) + 2);
        sprintf(fpath, "%s/%s", path, de->d_name);
        
        if (stat(fpath, &st) == -1) {
            free(fpath);
            xerror("failed to stat '%s'", de->d_name);
            return NULL;
        }
        
        dir->files[i] = xmalloc(sizeof(File_data));
        dir->files[i]->name = dupstr(de->d_name);
        dir->files[i]->type = get_filetype(de->d_type);
        if (access(dir->files[i]->name, F_OK|X_OK) == 0 
        &&  dir->files[i]->type != FT_DIR)
            dir->files[i]->type = FT_EXEC;
        else
            errno = 0; /* Permission denied. */

        dir->files[i]->indicator = get_indicator(dir->files[i]->type);
        dir->files[i]->nlen = strlen(de->d_name);
        dir->files[i]->nlink = st.st_nlink;
        dir->files[i]->mode = get_mode_string(st);
        dir->files[i]->user = get_user_name(st);
        dir->files[i]->group = get_group_name(st);
        dir->files[i]->fsize = st.st_size;
        dir->files[i]->mtime = 4 + ctime(&(st.st_ctime));
 
        if (dir->files[i]->type == FT_DIR && !f_no_classify)
            dir->files[i]->nlen++;

        row_len += dir->files[i]->nlen + 1;
        store_longest(dir, dir->files[i]);

        if (j * ALLOC >= i) {
            ++j;
            dir->files = xrealloc(dir->files, (ALLOC * j) * sizeof(File_data *));
        }

        if (dir->files[i]->type == FT_DIR && f_recursive)
            get_files(fpath);

        free(fpath);
    }

    if (errno != 0)
        xerror("an error occured while reading '%s'", path);

    closedir(d);
    dir->path = dupstr(path);
    dir->files[i + 1] = NULL;
    dir->num_rows = i / (window_width / (dir->lname + 1)) + 1;
    qsort(dir->files, i, sizeof(File_data *), sort_by_name);
    dir->num_files = i;
    ++num_dirs;
    if (dirs == NULL)
        dirs = xmalloc(sizeof(Dir_data *));
    else 
        dirs = xrealloc(dirs, sizeof(Dir_data *) * (num_dirs));
    dirs[num_dirs - 1] = dir;

    return dir;
}

int 
ls(char **args)
{
    size_t i;
    int status = EXIT_SUCCESS;

    if (!isatty(1))
        print_file_nl = 1;

    args = get_options(args, flags);

    if (*args == NULL) {
        args[0] = dupstr(".");
        args[1] = NULL;
    }

    for (i = 0; args[i] != NULL; ++i) {
        if (!get_files(args[i]))
            status = 2;
    }

    for (i = num_dirs; i--;) {
        if (num_dirs > 1)
            fprintf(stdout, "%s: \n", dirs[i]->path);
        print_files(dirs[i]);
        if (i > 0) fputc('\n', stdout);
    }

    num_dirs = 0;
    free_dirs();

    return status;
}

int 
main(int argc, char *argv[])
{
    return ls(argv);
}

