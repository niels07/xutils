#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h> 
#include <sys/statvfs.h> 
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define PROGRAM_NAME "df"
#define VERSION 1.0
#define AUTHOR "niels@n-ve.be"

#include "xlib.h"

typedef unsigned long long int Bytes;

typedef struct {
    /* Bytes / 1024. */
    Bytes in_kbytes;

    /* Total bytes. */
    Bytes in_bytes;

    /* Human readable textual representation. */
    char *human_readable;

    /* Number of digits in kbytes. */
    size_t num_digits;

    /* string length of human_readable. */
    size_t hr_length;
} Size;

typedef struct {
    /* Device filesystem name (i.e. /dev/sda1) */
    char *devfs;

    /* Path to where this entry is mounted. */
    char *mountpoint;

    /* Type of filesystem. */
    char *type;

    /* Size of the filesystem */
    Size *size;
    Size *used;
    Size *avail;

    /* Used disc space in percentage. */
    int use;

    Size *free;
} Mtab_entry;

/* Entries red from '/etc/mtab' */
static Mtab_entry **entries;

/* Total number of entries. */
static size_t num_entries;

/* Longest strings for printing values in columns. */
static size_t ldevfs, lmountp, ltype, lblocks, lavail, lsize, luse, lused, lfree;

static Option f_human_readable = 0;

/* Include special filesystems. */
static Option f_show_all = 0;

static Option f_no_color = 0;

/* Pseudo and virtual filesystems to ignore if f_ignore_special
   is set to zero. */
static const char *SPECIAL_FS[] = { "devfs", "debugfs", "procfs", "tmpfs", "specfs", "sysfs" };

static void usage(void);
static char *human_readable(long double);
static char *bytes_to_str(Bytes);
static void free_mtab_entry(Mtab_entry *);
static Mtab_entry *get_mtab_entry(FILE *);
static int bytes_count_digits(Bytes );

static Flag flags[] = {
    { "human-readable", 'h', &f_human_readable , NULL     },
    { "no-color",       'C', &f_no_color,        NULL     },
    { "help",           ' ', NULL,               usage    },
    { NULL, 0, NULL, NULL }
};

/* Colors associated with each filetype. */
const static struct color_assoc {
    char *fs;
    Color color;
} 

COLOR_ASSOC[] = {
    { "ext4",  C_BLUE  },
    { "nfs",   C_BROWN }, 
    { "tmpfs", C_GREEN },
    { NULL, 0          } 
};

static void 
usage(void)
{
    puts("df");
}

static char * 
human_readable(long double size)
{
    int i = 0;
    const char* units[] = {"B ", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
    char *unit;
    char *hr;
    
    while (size > 1024) {
        size /= 1024;
        i++;
    }

    hr = xmalloc(255);
    
    if (f_no_color)
        unit = dupstr(units[i]);
    else
        unit = color_string(C_BLUE, CT_NORMAL, units[i]);

    sprintf(hr, "%.1Lf %s", size, unit);
    free(unit);
    return hr;
}

static Size *
new_size(Bytes b)
{
    Size *size = xmalloc(sizeof(Size));

    size->in_kbytes = b / 1024;
    size->in_bytes = b;
    size->human_readable = human_readable(b);
    size->hr_length = strlen(size->human_readable);
    size->num_digits = bytes_count_digits(size->in_kbytes);

    return size;
}

static char *
bytes_to_str(Bytes b)
{
    char *str;

    str = xmalloc(255);
    sprintf(str, "%lld", b);
    return str;
}

static int
bytes_count_digits(Bytes b)
{
    int digits = 0;

    while (b > 0) {
        b /= 10;
        digits++;
    }

    return digits;
}


static void
free_mtab_entry(Mtab_entry *entry)
{
    free(entry->devfs);
    free(entry->mountpoint);
    free(entry->type);
    free(entry);
}

static int 
ignore_fs(const char *fs)
{
    if (f_show_all)
        return 0;

    for (size_t i = 0; SPECIAL_FS[i] != NULL; ++i) {
        if (streq(SPECIAL_FS[i], fs))
            return 1;
    }
    return 0;
}

static Mtab_entry *
new_entry(void)
{
    Mtab_entry *entry = calloc(1, sizeof(Mtab_entry));
    return entry;
}

static void
store_longest_item(size_t *which, Size *size)
{
    if (f_human_readable)
    {
        if (size->hr_length > *which)
            *which = size->hr_length;
    }
    else
    {
        if (size->num_digits > *which)
            *which = size->num_digits;
    }
}

static void
store_longest(Mtab_entry *entry)
{
    size_t len;
    
    if ((len = strlen(entry->devfs)) > ldevfs)
        ldevfs = len;

    if ((len = strlen(entry->mountpoint)) > lmountp)
        lmountp = len;

    store_longest_item(&lsize, entry->size);
    store_longest_item(&lavail, entry->avail);
    store_longest_item(&lfree, entry->free);
    store_longest_item(&lused, entry->used);
}

static Mtab_entry *
get_mtab_entry(FILE *mt)
{
    char buf[255], cur[255], c;
    size_t col = 1;
    struct statvfs fs;
    
    if ((fgets(buf, 255, mt) == NULL))
        return NULL;

    Mtab_entry *entry = new_entry();

    size_t i, j;
    for (j = i = 0; (c = buf[i]) != '\0'; ++i)
    {
        if (!isspace(c))
        {
            cur[j++] = c;
            continue;
        }
        
        cur[j] = '\0';
            
        size_t len = strlen(cur);
        switch (col)
        {
        case 1:
            if (len > ldevfs) ldevfs = len;
            entry->devfs = dupstr(cur);
            break;

        case 2:
            if (len > lmountp) lmountp = len;
            entry->mountpoint = dupstr(cur);
            break;

        case 3:
            if (len > ltype) ltype = len;
            entry->type = dupstr(cur);
            break;

        default:
            break;
        }
        j = 0;
        ++col;
    }

    if (statvfs(entry->mountpoint, &fs) < 0)
    {
        xerror("Failed to stat '%s'", entry->mountpoint);
        return entry;
    }

    entry->size  = new_size((Bytes)fs.f_blocks * fs.f_frsize);
    entry->avail = new_size((Bytes)fs.f_bavail * fs.f_frsize);
    entry->free  = new_size((Bytes)fs.f_bfree  * fs.f_frsize);
    entry->used  = new_size(entry->size->in_bytes - entry->avail->in_bytes);
    
    if (entry->size->in_bytes == 0)
        entry->use = 0;
    else
        entry->use = (int)((long double)((entry->used->in_bytes * 100) / entry->size->in_bytes));
    
    store_longest(entry);

    return entry;
}

static int
read_mtab(void)
{
    Mtab_entry *entry;
    FILE *mtab_file;

    if ((mtab_file = fopen("/etc/mtab", "r")) == NULL) 
    {
        xerror("Failed to read '/etc/mtab'");
        return 0;
    }

    entries = xmalloc(25 * sizeof(Mtab_entry));
    num_entries = 0;

    while ((entry = get_mtab_entry(mtab_file)) != NULL)
        entries[num_entries++] = entry;

   fclose(mtab_file);

   return 1;
}

static void
print_fs_color(const Mtab_entry *entry)
{
    char *cdevfs, *csize, *cused, *cavail, *cuse, *cmountp;

    if (ignore_fs(entry->type))
        return;

    cdevfs = NULL;

    for (size_t i = 0; COLOR_ASSOC[i].fs != NULL; ++i)
    {
        if (streq(COLOR_ASSOC[i].fs, entry->type))
        {
            cdevfs = color_string(COLOR_ASSOC[i].color, CT_NORMAL, entry->devfs);
            break;
        }
    }

    if (cdevfs == NULL)
        cdevfs = color_string(C_WHITE, CT_NORMAL, entry->devfs);

    if (f_human_readable)
    {
        cused   = color_string(C_WHITE, CT_DARK, entry->used->human_readable);
        csize   = color_string(C_WHITE, CT_DARK, entry->size->human_readable);
        cavail  = color_string(C_WHITE, CT_DARK, entry->avail->human_readable);
    }
    else
    {
        char *used, *size, *avail;
        used  = bytes_to_str(entry->used->in_kbytes);
        size  = bytes_to_str(entry->size->in_kbytes);
        avail = bytes_to_str(entry->avail->in_kbytes);

        cused  = color_string(C_WHITE, CT_DARK, used);
        csize  = color_string(C_WHITE, CT_DARK, size);
        cavail = color_string(C_WHITE, CT_DARK, avail);

        free(used);
        free(size);
        free(avail);
    }
  
    char *use = xmalloc(5);
    sprintf(use, "%d%%", entry->use);

    if (entry->use < 25)
        cuse = color_string(C_GREEN, CT_DARK, use);
    else 
    if (entry->use < 50)
        cuse = color_string(C_BROWN, CT_LIGHT, use);
    else
    if (entry->use < 75)
        cuse = color_string(C_BROWN, CT_DARK,  use);
    else
        cuse = color_string(C_RED,   CT_LIGHT, use);

    free(use);

    cmountp = color_string(C_WHITE, CT_DARK, entry->mountpoint);
    fprintf(stdout, "%-*s %*s %*s %*s %*s %-*s\n", 
            ldevfs + COLOR_SIZE  , cdevfs,  
            lsize  + COLOR_SIZE , csize,
            lused  + COLOR_SIZE , cused,
            lavail + COLOR_SIZE , cavail,  
            luse   + COLOR_SIZE , cuse,
            lmountp + COLOR_SIZE , cmountp);

    free(cdevfs);
    free(cuse);
    free(csize);
    free(cused);
    free(cavail);
    free(cmountp);
}

static void
print_fs(Mtab_entry *entry)
{
    if (ignore_fs(entry->type))
        return;
 
    char * use = xmalloc(5);
    sprintf(use, "%d%%", entry->use);
    
    if (f_human_readable)
        fprintf(stdout, "%-*s %*s %*s %*s %*s%% %-*s\n", 
                ldevfs,  entry->devfs,  
                lsize,   entry->size->human_readable, 
                lused,   entry->used->human_readable,
                lavail,  entry->avail->human_readable,  
                luse,    use,
                lmountp, entry->mountpoint);
    else
        fprintf(stdout, "%-*s %*lld %*lld %*lld %*s%% %-*s\n", 
            ldevfs,  entry->devfs,  
            lsize,   entry->size->in_kbytes, 
            lused,   entry->used->in_kbytes,
            lavail,  entry->avail->in_kbytes,  
            luse,    use,
            lmountp, entry->mountpoint);

}

void
df(char **args)
{
    ldevfs = 11;
    lmountp = 11;
    ltype = 0;
    lblocks = 0;
    lavail = 6;
    lsize = 5;
    lused = 5;
    lfree = 0;
    luse  = 4;

    args = get_options(args, flags);
    
    read_mtab();

    int sp = 0;

    if (!f_no_color)
    {
        set_color(C_WHITE, CT_LIGHT);
        sp = COLOR_SIZE;
    }

    fprintf(stdout, "%-*s %*s %*s %*s %*s %-*s\n", 
            ldevfs,  "Filesystem",
            lsize - sp,   "Size",
            lused - sp,   "Used",
            lavail - sp,  "Avail",
            luse,    "Use%",
            lmountp, "Mounted on");

    if (!f_no_color)
        clear_color();

    for (size_t i = 0; i < num_entries; ++i)
    {
        if (f_no_color)
            print_fs(entries[i]);
        else
            print_fs_color(entries[i]);

        free_mtab_entry(entries[i]);
    }

}

int main(int argc, char *argv[])
{
    df(argv);
    return 0;
}
