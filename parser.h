#ifndef _PARSER_H
#define _PARSER_H

#include <stdio.h>
#include <stdlib.h>

struct symbol
{
	/* symbol name */
	char *str;

	/* single bit value */
	int bit_length;
	long value;

	/* array */
	void *array;
	int *array_size;
	int array_depth;

	/* some complex form of symbol like number range */
	struct element *predefined;

	struct symbol *next;
};

enum mnemonic
{
	mnemonic_bslbf,
	mnemonic_uimsbf,
	mnemonic_simsbf,
	mnemonic_vlclbf,
};

#define element_type(n)	element_type_##n,
enum element_type
{
#include "parser_element_types.h"
};
#undef element_type

extern const char *element_type_name[];

struct element
{
	enum element_type type;

	int data_num;
	void **data;

	/* used when listing same element types. NULL if its end */
	struct element *next, *prev;
};

struct element *element_new (enum element_type type, int args, ...);

extern struct symbol symbol_parser;
struct symbol *symbol_get (struct symbol *root, const char *name);


extern void _error_element (FILE *o, const char *func, int line,
		struct element *e, const char *f, ...) __attribute__((format (printf, 5, 6)));

#define LOG_FORMAT_PREFIX	"%-24s %4d: "
#if 1
#define debug(f,a...)		do{ \
	if(debug_level>0) \
		printf (LOG_FORMAT_PREFIX f,__func__,__LINE__,##a); \
}while(0)
#else
#define debug(f,a...)		do{}while(0)
#endif
#define info(f,a...)		printf (LOG_FORMAT_PREFIX f,__func__,__LINE__,##a)
#define error(f,a...)		printf (LOG_FORMAT_PREFIX f,__func__,__LINE__,##a)
#define ferror_element(o,e,f,a...)	_error_element (o, __func__,__LINE__,e,f,##a)
#define error_element(e,f,a...)	_error_element (stdout, __func__,__LINE__,e,f,##a)


enum return_status
{
	return_internal_error,
	return_bitstream_error,		// try again if possible
	return_end_of_stream,
};
extern enum return_status return_status;

enum return_type
{
	return_type_nothing,
	return_type_boolean,
	return_type_stream_offset,
	return_type_string,
	return_type_integer,
	return_type_symbol,
	return_type_bitlength,
	return_type_function_return,
};

struct stream_offset
{
	int bytes;
	int bits;
};

struct return_value
{
	enum return_type type;

	union
	{
		int nothing;
		int boolean;
		struct stream_offset stream_offset;
		const char *string;
		long long integer;
		struct symbol *symbol;
		int bitlength;
		struct return_value *function_return;
	} v;
};

#define return_value_init(r,t,val) \
	do { \
		r = calloc (1, sizeof(*r)); \
		if (r) \
		{ \
			r->type = return_type_##t; \
			r->v.t = val; \
		} \
	} while(0)

#define return_value(t,val) \
	do { \
		struct return_value *r; \
		return_value_init(r,t,val); \
		return r; \
	} while (0)

extern const char *desc_return_value (struct return_value *r);


#endif
