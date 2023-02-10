/******************************************************************************
 * @file            main.c
 *****************************************************************************/
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "cc.h"
#include    "cpp.h"
#include    "lib.h"
#include    "ll.h"
#include    "macro.h"
#include    "report.h"
#include    "token.h"
#include    "vector.h"

static struct vector temp_files = { 0 };

struct cc_state *state = 0;
const char *program_name = 0;

static char *make_output_filename (char *base) {

    char *p1 = base, *p2;
    size_t len = 0;
    
    if ((p2 = strrchr (p1, '/'))) {
    
        base = (p2 + 1);
        p1 = base;
    
    }
    
    while (*p1) {
    
        if (*p1 == '.') {
            len = (p1 - base);
        }
        
        p1++;
    
    }
    
    p2 = xmalloc (len + 3);
    
    strncpy (p2, base, len);
    strcat (p2, ".i");
    
    return p2;

}

int main (int argc, char **argv) {

    unsigned long i;
    
    if (argc && *argv) {
    
        char *p;
        program_name = *argv;
        
        if ((p = strrchr (program_name, '/'))) {
            program_name = (p + 1);
        }
    
    }
    
    init_macros ();
    init_punct ();
    
    state = xmalloc (sizeof (*state));
    parse_args (&argc, &argv, 1);
    
    if (state->mode < CC_PREPROCESS) {
    
        report_at (program_name, 0, REPORT_ERROR, "currently only -E is supported");
        exit (EXIT_FAILURE);
    
    }
    
    init_cpp ();
    
    for (i = 0; i < state->nb_files; i++) {
    
        char *file = state->files[i], *outfile;
        int result;
        
        outfile = make_output_filename (file);
        
        if ((state->ofp = fopen (outfile, "w")) == NULL) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for writing", outfile);
            continue;
        
        }
        
        clear_macros ();
        
        if ((result = preprocess (file)) < 0) {
        
            if (result == -1) {
                report_at (program_name, 0, REPORT_ERROR, "failed to preprocess '%s'", file);
            } else if (result == -2) {
                report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for reading", file);
            }
            
            fclose (state->ofp);
            remove (outfile);
            
            continue;
        
        }
        
        vec_push (&temp_files, outfile);
        fclose (state->ofp);
        
        if (get_error_count () > 0) {
            remove (outfile);
        }
    
    }
    
    /*while ((temp_file = vec_pop (&temp_files))) {
        report_at (program_name, 0, REPORT_FATAL_ERROR, "%s", temp_file);
    }*/
    
    return (get_error_count () == 0 ? EXIT_SUCCESS : EXIT_FAILURE);

}
