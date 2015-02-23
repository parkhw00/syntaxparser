
#ifndef _PARSER_INPUT_H
#define _PARSER_INPUT_H

#include "parser.h"

int set_input (const char *name);
int set_input_number (int num);

const char *get_display_input_name (void);
int get_display_input_name_len (void);

int get_bits (int bits, unsigned int *_ret);
int byte_position (void);
int bit_position (void);
void get_position (int *byte, int *bit);
void set_position (int byte, int bit);

void *bytealigned (struct element *arg);
void *more_data (struct element *arg);

int set_memory_buffer (const unsigned char *data, int size);

#endif

