/******************************************************************************
 * @file            ll.h
 *****************************************************************************/
#ifndef     _LL__H
#define     _LL__H

#include    <stdio.h>

int load_line (char **line_p, char **line_end_p, unsigned long *newlines_p, FILE *ifp, void **load_line_internal_data_p);

void load_line_destory_internal_data (void *load_line_internal_data);
void *load_line_create_internal_data (unsigned long *new_line_number_p);

#endif      /* _LL__H */
