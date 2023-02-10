/******************************************************************************
 * @file            report.c
 *****************************************************************************/
#include    <stdarg.h>
#include    <stdio.h>
#include    <string.h>

#include    "report.h"

unsigned long errors = 0;

#ifndef     __PDOS__
#if     defined (_WIN32)
# include   <windows.h>
static int OriginalConsoleColor = -1;
#endif

static void reset_console_color (void) {

#if     defined (_WIN32)

    HANDLE hStdError = GetStdHandle (STD_ERROR_HANDLE);
    
    if (OriginalConsoleColor == -1) { return; }
    
    SetConsoleTextAttribute (hStdError, OriginalConsoleColor);
    OriginalConsoleColor = -1;

#else

    fprintf (stderr, "\033[0m");

#endif

}

static void set_console_color (int color) {

#if     defined (_WIN32)

    HANDLE hStdError = GetStdHandle (STD_ERROR_HANDLE);
    WORD wColor;
    
    if (OriginalConsoleColor == -1) {
    
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        
        if (!GetConsoleScreenBufferInfo (hStdError, &csbi)) {
            return;
        }
        
        OriginalConsoleColor = csbi.wAttributes;
    
    }
    
    wColor = (OriginalConsoleColor & 0xF0) + (color & 0xF);
    SetConsoleTextAttribute (hStdError, wColor);

#else

    fprintf (stderr, "\033[%dm", color);

#endif

}
#endif

static void output_message (const char *filename, unsigned long lineno, enum report_type type, const char *fmt, va_list ap) {

    if (filename) {
    
        if (lineno == 0) {
            fprintf (stderr, "%s: ", filename);
        } else {
            fprintf (stderr, "%s:", filename);
        }
    
    }
    
    if (lineno > 0) {
        fprintf (stderr, "%lu: ", lineno);
    }
    
    if (type == REPORT_ERROR || type == REPORT_FATAL_ERROR) {
    
#ifndef     __PDOS__
        set_console_color (COLOR_ERROR);
#endif
        
        if (type == REPORT_ERROR) {
            fprintf (stderr, "error:");
        } else {
            fprintf (stderr, "fatal error:");
        }
    
    } else if (type == REPORT_INTERNAL_ERROR) {
    
#ifndef     __PDOS__
        set_console_color (COLOR_INTERNAL_ERROR);
#endif
        
        fprintf (stderr, "internal error:");
    
    } else if (type == REPORT_WARNING) {
    
#ifndef     __PDOS__
        set_console_color (COLOR_WARNING);
#endif
        
        fprintf (stderr, "warning:");
    
    }
    
#ifndef     __PDOS__
    reset_console_color ();
#endif
    
    fprintf (stderr, " ");
    
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
    
    if (type != REPORT_WARNING) {
        ++errors;
    }

}

unsigned long get_error_count (void) {
    return errors;
}

void report_at (const char *filename, unsigned long lineno, enum report_type type, const char *fmt, ...) {

    va_list ap;
    
    va_start (ap, fmt);
    output_message (filename, lineno, type, fmt, ap);
    va_end (ap);

}

void report_line_at (const char *filename, unsigned long lineno, enum report_type type, const char *start, const char *end, const char *fmt, ...) {

    int indent = 1;
    va_list ap;
    
    va_start (ap, fmt);
    output_message (filename, lineno, type, fmt, ap);
    va_end (ap);
    
    if (start != NULL && end != NULL) {
    
        if (lineno > 0) {
            indent = fprintf (stderr, " %8lu | ", lineno);
        } else {
            indent = fprintf (stderr, "%*s", 12, "");
        }
        
        fprintf (stderr, "%s", start);
        
        if (start[strlen (start) - 1] != '\n') {
            fprintf (stderr, "\n");
        }
        
        fprintf (stderr, "%*s^\n", (int) (end - start) + indent, "");
    
    }

}
