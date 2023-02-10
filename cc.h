/******************************************************************************
 * @file            cc.h
 *****************************************************************************/
#ifndef     _CC_H
#define     _CC_H

#include    <stdio.h>

struct cc_state {

    char **files, **defs, **inc_paths;
    unsigned long nb_files, nb_defs, nb_inc_paths;
    
    FILE *ofp;
    int mode;

};

extern struct cc_state *state;
extern const char *program_name;

#endif      /* _CC_H */
