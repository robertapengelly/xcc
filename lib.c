/******************************************************************************
 * @file            lib.c
 *****************************************************************************/
#include    <ctype.h>
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "cstr.h"
#include    "lib.h"
#include    "macro.h"
#include    "report.h"

struct option {

    const char *name;
    int index, flags;

};

#define     OPTION_NO_ARG               0x0001
#define     OPTION_HAS_ARG              0x0002

enum options {

    OPTION_IGNORED = 0,
    OPTION_DEFINE,
    OPTION_HELP,
    OPTION_INCLUDE,
    OPTION_PREPROCESS,
    OPTION_UNDEFINE

};

static struct option opts[] = {

    { "D",              OPTION_DEFINE,      OPTION_HAS_ARG  },
    { "E",              OPTION_PREPROCESS,  OPTION_NO_ARG   },
    { "I",              OPTION_INCLUDE,     OPTION_HAS_ARG  },
    { "U",              OPTION_UNDEFINE,    OPTION_HAS_ARG  },
    
    { "-help",          OPTION_HELP,        OPTION_NO_ARG   },
    { 0,                0,                  0               }

};

static int strstart (const char *val, const char **str) {

    const char *p = val;
    const char *q = *str;
    
    while (*p != '\0') {
    
        if (*p != *q) {
            return 0;
        }
        
        ++p;
        ++q;
    
    }
    
    *str = q;
    return 1;

}

static void add_include_path (const char *pathname) {

    const char *in = pathname;
    const char *p;
    
    do {
    
        int c;
        
        CString str;
        cstr_new (&str);
        
        for (p = in; c = *p, c != '\0' && c != PATHSEP[0]; ++p) {
            cstr_ccat (&str, c);
        }
        
        if (str.size) {
        
            cstr_ccat (&str, '\0');
            dynarray_add (&state->inc_paths, &state->nb_inc_paths, xstrdup (str.data));
        
        }
        
        cstr_free (&str);
        in = (p + 1);
    
    } while (*p != '\0');

}

static void print_help (void) {

    if (!program_name) {
        goto _exit;
    }
    
    fprintf (stderr, "Usage: %s [options] file...\n\n", program_name);
    fprintf (stderr, "Options:\n\n");
    
    fprintf (stderr, "    -D MACRO[=str]        Pre-define a macro\n");
    fprintf (stderr, "    -E                    Preprocess only; do not compile\n");
    fprintf (stderr, "    -I DIR                Add DIR to search list for .include directives\n");
    fprintf (stderr, "    -U MACRO              Undefine a macro\n");
    
    fprintf (stderr, "    --help                Print this help information\n");
    fprintf (stderr, "\n");
    
_exit:
    
    exit (EXIT_SUCCESS);

}

char *skip_whitespace (char *p) {

    while (*p == ' ') {
        p++;
    }
    
    return p;

}

char *trim (char *p) {

    size_t len = strlen (p);
    
    while (isspace ((int) p[--len])) {
        p[len] = '\0';
    }
    
    return xstrdup (p);

}

char *xstrdup (const char *p) {

    char *p1 = xmalloc (strlen (p) + 1);
    strcpy (p1, p);
    
    return p1;

}

char *xstrndup (const char *p, unsigned long len) {

    char *p1 = xmalloc (len + 1);
    unsigned long i;
    
    for (i = 0; i < len; i++) {
        p1[i] = p[i];
    }
    
    return p1;

}

void dynarray_add (void *ptab, unsigned long *nb_ptr, void *data) {

    long nb, nb_alloc;
    void **pp;
    
    nb = *nb_ptr;
    pp = *(void ***) ptab;
    
    if ((nb & (nb - 1)) == 0) {
    
        if (!nb) {
            nb_alloc = 1;
        } else {
            nb_alloc = nb * 2;
        }
        
        pp = xrealloc (pp, nb_alloc * sizeof (void *));
        *(void ***) ptab = pp;
    
    }
    
    pp[nb++] = data;
    *nb_ptr = nb;

}

void parse_args (int *pargc, char ***pargv, int optind) {

    char **argv = *pargv;
    int argc = *pargc;
    
    struct option *popt;
    const char *optarg, *r;
    
    if (argc == optind) {
        print_help ();
    }
    
    while (optind < argc) {
    
        r = argv[optind++];
        
        if (r[0] != '-' || r[1] == '\0') {
        
            dynarray_add (&state->files, &state->nb_files, xstrdup (r));
            continue;
        
        }
        
        for (popt = opts; popt; ++popt) {
        
            const char *p1 = popt->name;
            const char *r1 = (r + 1);
            
            if (!p1) {
            
                report_at (program_name, 0, REPORT_ERROR, "invalid option -- '%s'", r);
                exit (EXIT_FAILURE);
            
            }
            
            if (!strstart (p1, &r1)) {
                continue;
            }
            
            optarg = r1;
            
            if (popt->flags & OPTION_HAS_ARG) {
            
                if (*optarg == '\0') {
                
                    if (optind >= argc) {
                    
                        report_at (program_name, 0, REPORT_ERROR, "argument to '%s' is missing", r);
                        exit (EXIT_FAILURE);
                    
                    }
                    
                    optarg = argv[optind++];
                
                }
            
            } else if (*optarg != '\0') {
                continue;
            }
            
            break;
        
        }
        
        switch (popt->index) {
        
            case OPTION_DEFINE: {
            
                char *p1 = (char *) optarg, *p2 = 0;
                size_t len = 0;
                
                while (*p1) {
                
                    if (*p1 == ' ' || *p1 == '\t') {
                    
                        p2 = xrealloc (p2, len + 1);
                        p2[len++] = ' ';
                        
                        while (*p1 && (*p1 == ' ' || *p1 == '\t')) {
                            p1++;
                        }
                        
                        continue;
                    
                    }
                    
                    if (*p1 == '=') {
                    
                        if (p2[len - 1] != ' ') {
                        
                            p2 = xrealloc (p2, len + 1);
                            p2[len++] = ' ';
                        
                        }
                    
                    }
                    
                    p2 = xrealloc (p2, len + 1);
                    p2[len++] = *p1++;
                    
                    if (*(p1 - 1) == '=' && *p1 != ' ') {
                    
                        p2 = xrealloc (p2, len + 1);
                        p2[len++] = ' ';
                    
                    }
                
                }
                
                p2 = xrealloc (p2, len + 1);
                p2[len] = '\0';
                
                define_macro (p2);
                free (p2);
                
                break;
            
            }
            
            case OPTION_HELP: {
            
                print_help ();
                break;
            
            }
            
            case OPTION_INCLUDE: {
            
                size_t len = strlen (optarg);
                char *buf;
                
                if (optarg[len - 1] == '/') {
                
                    buf = xmalloc (len + 1);
                    sprintf (buf, "%s", optarg);
                
                } else {
                
                    buf = xmalloc (len + 2);
                    sprintf (buf, "%s/", optarg);
                
                }
                
                add_include_path (buf);
                free (buf);
                
                break;
            
            }
            
            case OPTION_PREPROCESS: {
            
                state->mode = CC_PREPROCESS;
                break;
            
            }
            
            case OPTION_UNDEFINE: {
            
                char *p1 = (char *) optarg, *p2 = 0;
                size_t len = 0;
                
                while (*p1) {
                
                    if (*p1 == ' ' || *p1 == '\t') {
                    
                        p2 = xrealloc (p2, len + 1);
                        p2[len++] = ' ';
                        
                        while (*p1 && (*p1 == ' ' || *p1 == '\t')) {
                            p1++;
                        }
                        
                        continue;
                    
                    }
                    
                    if (*p1 == '=' && p2[len - 1] != ' ') {
                    
                        p2 = xrealloc (p2, len + 1);
                        p2[len++] = ' ';
                    
                    }
                    
                    p2 = xrealloc (p2, len + 1);
                    p2[len++] = *p1++;
                    
                    if (*(p1 - 1) == '=' && *p1 != ' ') {
                    
                        p2 = xrealloc (p2, len + 1);
                        p2[len++] = ' ';
                    
                    }
                
                }
                
                p2 = xrealloc (p2, len + 1);
                p2[len] = '\0';
                
                undefine_macro (p2);
                free (p2);
                
                break;
            
            }
            
            default: {
            
                report_at (program_name, 0, REPORT_ERROR, "unsupported option '%s'", r);
                exit (EXIT_FAILURE);
            
            }
        
        }
    
    }

}

void *xrealloc (void *ptr, unsigned long size) {

    void *new_ptr = realloc (ptr, size);
    
    if (!new_ptr && size) {
    
        report_at (program_name, 0, REPORT_ERROR, "memory full (realloc)");
        exit (EXIT_FAILURE);
    
    }
    
    return new_ptr;

}

void *xmalloc (unsigned long size) {

    void *ptr = malloc (size);
    
    if (!ptr && size) {
    
        report_at (program_name, 0, REPORT_ERROR, "memory full (malloc)");
        exit (EXIT_FAILURE);
    
    }
    
    memset (ptr, 0, size);
    return ptr;

}
