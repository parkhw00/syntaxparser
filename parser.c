
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <getopt.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "parser.h"
#include "parser_input.h"
#include "parser_desc.h"

#define element_type(n)	#n,
const char *element_type_name[] =
{
#include "parser_element_types.h"
};
#undef element_type

#define POSITION_LENGTH		(7+8+6+get_display_input_name_len()+1)


int debug_level;

FILE *null_out;

enum return_status return_status = return_internal_error;

static struct return_value *parse_function (const char *function_name,
		struct element *args, int indent, FILE *out);
static struct return_value *parse_element (struct element *e,
		struct symbol *symbol_local, int indent, FILE *out);

const char *desc_return_value (struct return_value *r)
{
	if (!r)
		return g_strdup ("<null>");

	if (r->type == return_type_nothing)
		return g_strdup ("nothing");
	if (r->type == return_type_boolean)
		return g_strdup ("boolean");
	if (r->type == return_type_stream_offset)
		return g_strdup ("stream_offset");
	if (r->type == return_type_string)
		return g_strdup ("string");
	if (r->type == return_type_integer)
		return g_strdup ("integer");
	if (r->type == return_type_symbol)
		return g_strdup ("symbol");
	if (r->type == return_type_bitlength)
		return g_strdup ("bitlength");
	if (r->type == return_type_function_return)
		return g_strdup ("function_return");

	return g_strdup_printf ("unknown type(%d)", r->type);
}

void _error_element (FILE *o, const char *func, int line,
		struct element *e, const char *f, ...)
{
	const char *t;
	va_list ap;

	va_start (ap, f);

	if (e)
		t = description (e, 0);
	else
		t = g_strdup ("<null>");
	fprintf (o, "function:%s, line:%d\n", func, line);
	vfprintf (o, f, ap);
	fprintf (o, "error element:\n%s\n", t);
	g_free ((void*)t);

	va_end (ap);
}

struct symbol symbol_parser;
struct symbol *symbol_get (struct symbol *root,
		const char *name)
{
	struct symbol *s, *new;

	s = root;
	while (s->next)
	{
		s = s->next;

		if (!strcmp (s->str, name))
			return s;
	}

	new = calloc (1, sizeof (*new));
	if (!new)
		return NULL;

	new->str = g_strdup (name);

	s->next = new;

	return new;
}

struct symbol *symbol_check (struct symbol *root,
		const char *name)
{
	struct symbol *s;

	s = root;
	while (s->next)
	{
		s = s->next;

		if (!strcmp (s->str, name))
			return s;
	}

	return NULL;
}

void symbol_free (struct symbol *root)
{
	struct symbol *s, *t;

	s = root->next;
	while (s)
	{
		t = s;
		s = s->next;

		g_free (t->str);
		free (t);
	}

	root->next = NULL;
}

int symbol_number (struct symbol *s, int *num)
{
	if (s->predefined)
	{
		struct element *e;

		e = s->predefined;
		do
		{
			if (e->type == element_type_num)
			{
				*num = (unsigned long)e->data[0];

				return 0;
			}

			e = e->next;
		}
		while (e != s->predefined);
	}

	return -1;
}

int symbol_range (struct symbol *s, int *from, int *to)
{
	if (s->predefined)
	{
		struct element *e;

		e = s->predefined;
		do
		{
			if (e->type == element_type_num_range)
			{
				*from = (unsigned long)e->data[0];
				*to   = (unsigned long)e->data[1];

				return 0;
			}

			e = e->next;
		}
		while (e != s->predefined);
	}

	return -1;
}

struct element *predefined;
struct symbol *search_predefined (const char *symb)
{
	struct element *e;

	if (!predefined)
	{
		error ("bug??\n");
		return NULL;
	}

	e = predefined;
	do
	{
		struct symbol *s;

		if (e->type != element_type_pred_def)
		{
			error ("bug??\n");
			goto do_next;
		}
		s = e->data[0];

		if (!strcmp (s->str, symb))
			return s;

do_next:
		e = e->next;
	}
	while (e != predefined);

	return NULL;
}

struct element *search_attribute_symbol (struct symbol *s, const char *attr)
{
	struct element *e;

	if (!s || !s->predefined)
	{
		debug ("no symbol for %s, %s\n", s->str, attr);
		return NULL;
	}

	e = s->predefined;
	do
	{
		struct symbol *v;

		v = e->data[0];
		if (e->type == element_type_variable &&
				!strcmp (v->str, attr))
		{
			debug ("found var for %s, %s\n", s->str, attr);
			return e;
		}

		if (e->type == element_type_functioncall &&
				!strcmp (v->str, attr))
		{
			debug ("found attr for %s, %s\n", s->str, attr);
			return e;
		}

		e = e->next;
	}
	while (e != s->predefined);

	return NULL;
}

struct element *search_attribute (const char *symb, const char *attr)
{
	return search_attribute_symbol (search_predefined (symb), attr);
}

struct function
{
	/* symbol of function name */
	struct symbol *symbol;

	/* function body */
	struct element *element;

	/* native function body */
	void *(*builtin) (struct element *);

	/* local symbol table */
	struct symbol symbol_local;

	unsigned int flags;
#define FUNCTION_FLAG_NO_PRINT	1

	struct function *next;
};

static struct function *function_root;
struct function *function_get (const char *name,
		struct element *e, void *(*builtin)(struct element *))
{
	struct function *f;

	f = function_root;
	while (f)
	{
		if (!strcmp (f->symbol->str, name))
			break;

		f = f->next;
	}

	if (f == NULL && (e || builtin))
	{
		f = calloc (1, sizeof (*f));
		if (!f)
			return NULL;

		f->symbol = symbol_get (&symbol_parser, name);
		f->element = e;
		f->builtin = builtin;

		f->next = function_root;
		function_root = f;
	}

	return f;
}

struct symbol *symbol_search (struct symbol *root,
		const char *name)
{
	struct symbol *ret;
	struct function *f;

	/* first, check in local symbol table */
	if (root)
	{
		ret = symbol_check (root, name);
		if (ret)
			return ret;
	}

	/* search in function local table */
	f = function_root;
	while (f)
	{
		ret = symbol_check (&f->symbol_local, name);
		if (ret)
		{
			debug ("found symbol, %s, in %s function table\n",
					name, f->symbol->str);
			return ret;
		}

		f = f->next;
	}

	/* if not found, search in predefined table */
	ret = search_predefined (name);
	if (ret && ret->predefined)
	{
		struct element *e;

		e = ret->predefined;

		do
		{
			if (e->type == element_type_num)
			{
				long num;

				num = (long)e->data[0];
				debug ("predefined symbol %s, number %d\n", name, (int)num);

				ret->value = num;
			}
			else if (e->type == element_type_functioncall && (

				/* attributes that looks like function call */
				!strcmp (((struct symbol *)e->data[0])->str, "__array_size")
					)
				)
			{
				debug ("skip function call, %s, for %s\n",
						((struct symbol *)e->data[0])->str,
						name);
			}
			else if (e->type == element_type_functioncall)
			{
				struct return_value *r;
				struct symbol *s = e->data[0];

				debug ("call function %s for %s\n", s->str, name);
				r = parse_function (s->str, e->data[1], 0, null_out);
				if (!r)
				{
					error ("function, %s, failed for %s\n", s->str, name);
					return NULL;
				}

				if (r->type == return_type_bitlength)
				{
					ret->bit_length = r->v.bitlength;
					debug ("bit length %d for %s\n", ret->bit_length, name);
				}
				else if (r->type == return_type_integer)
				{
					ret->value = r->v.integer;
					debug ("value %ld for %s\n", ret->value, name);
				}
				else
				{
					const char *tmp;

					tmp = desc_return_value (r);
					error ("unknown return type, %s, to get %s\n",
							tmp, name);
					g_free ((void*)tmp);
				}
			}

			e = e->next;
		}
		while (e != ret->predefined);

		return ret;
	}

	error ("unknown symbol for %s\n", name);

	return NULL;
}

struct element *root_element;

struct element *element_new (enum element_type type, int args, ...)
{
	struct element *e;
	va_list ap;
	void *arg;

	debug ("type %s, %d args\n",
			element_type_name[type], args);

	e = calloc (1, sizeof (*e));
	if (!e)
		return NULL;

	if (args > 0)
	{
		int a;

		e->data = malloc (sizeof(void*)*args);
		if (!e->data)
		{
			free (e);
			return NULL;
		}

		va_start (ap, args);
		for (a=0; a<args; a++)
		{
			arg = va_arg (ap, void*);
			e->data[a] = arg;
			debug ("data[%d] = %p\n", a, arg);
		}
		va_end (ap);
	}

	e->type = type;
	e->data_num = args;
	e->next = e->prev = e;

	if (e->type == element_type_functiondef)
	{
		struct symbol *s;

		s = e->data[0];
		function_get (s->str, e, NULL);
	}

#if 0
	{
		const char *desc;
		desc = description (e, 0);
		debug ("%s\n", desc);
		g_free ((void*)desc);
	}
#endif

	return e;
}

const char *append (const char *a, const char *b)
{
	const char *str;

	if (!a && !b)
		return g_strdup ("");

	if (!a)
		return b;

	if (!b)
		return a;

	str = g_strdup_printf ("%s%s", a, b);
	g_free ((void*)a);
	g_free ((void*)b);

	return str;
}

const char *dot (struct element *e);

//#define VERTICAL_LIST
const char *dot_list (struct element *e)
{
	struct element *now;
	const char *ret = NULL;
	const char *sub = NULL;

	debug ("element %p\n", e);
	if (e->next != e)
	{
		const char *t;

		debug ("listing\n");

		now = e;
		do
		{
			if (now->next != e)
			{
#ifdef VERTICAL_LIST
				t = g_strdup_printf ("%selement_%p -> element_%p [dir=back];\n",
						sub?sub:"",
						now->next, now);
#else
				t = g_strdup_printf ("%selement_%p -> element_%p;\n",
						sub?sub:"",
						now, now->next);
#endif
				g_free ((void*)sub);
				sub = t;
			}

			now = now->next;
		}
		while (now != e);

		t = g_strdup_printf ("subgraph cluster_%p {\n"
				"label=\"listing\";\n"
				"subgraph subgraph_%p {\n"
#ifdef VERTICAL_LIST
				"rank=same;\n"
#endif
				"%s\n"
				"}\n"
				"}\n", e, e, sub);
		g_free ((void*)sub);
		sub = t;
	}

	now = e;
	do
	{
		ret = append (ret, dot (now));

		now = now->next;
	}
	while (now != e);

	return append (sub, ret);
}

const char *dot (struct element *e)
{
	void **data = e->data;
	struct symbol *s, *s2;
	const char *tmp, *ret, *label = "";
	void *es[4] = {NULL,};
	int es_num = 0;
	int a;

	switch (e->type)
	{
	case element_type_num_or:
	case element_type_num_range:
		label = g_strdup_printf ("%d, %d",
				(int)(long)data[0], (int)(long)data[1]);
		break;

	case element_type_num:
		label = g_strdup_printf ("%d",
				(int)(long)data[0]);
		break;

	case element_type_string:
		label = g_strdup ((char*)data[0]);
		break;

	case element_type_variable_array:
		s = data[0];
		label = g_strdup_printf ("%s", s->str);
		es[0] = data[1];
		es_num = 1;
		break;

	case element_type_variable:
		s = data[0];
		label = g_strdup_printf ("%s", s->str);
		break;

	case element_type_variable_stream:
		s = data[0];
		s2 = data[1];
		label = g_strdup_printf ("%s.%s", s->str, s2->str);
		break;

	case element_type_bitsdesc_mpeg2_string:
		label = g_strdup ((char*)data[2]);
		es[0] = data[0];
		es[1] = data[1];
		es_num = 2;
		break;

	case element_type_bitsdesc_mpeg2_mnemonic:
		switch ((enum mnemonic)data[2])
		{
		default: tmp = "unknown"; break;
		case mnemonic_bslbf : tmp = "bslbf";  break;
		case mnemonic_uimsbf: tmp = "uimsbf"; break;
		case mnemonic_simsbf: tmp = "simsbf"; break;
		case mnemonic_vlclbf: tmp = "vlclbf"; break;
		}
		label = g_strdup (tmp);
		es[0] = data[0];
		es[1] = data[1];
		es_num = 2;
		break;

	case element_type_functiondef:
		s = data[0];
		label = g_strdup (s->str);
		es[0] = data[1];
		es[1] = data[2];
		es_num = 2;
		break;

	case element_type_functioncall:
		s = data[0];
		label = g_strdup (s->str);
		es[0] = data[1];
		es_num = 1;
		break;

	case element_type_pred_def:
		s = data[0];
		label = g_strdup (s->str);
		es[0] = data[1];
		es_num = 1;
		break;

	default:
		for (a=0; a<e->data_num; a++)
			es[a] = e->data[a];
		es_num = e->data_num;
		break;
	}

	ret = g_strdup_printf ("element_%p [label=\"%s%s%s\"];\n",
			e, element_type_name[e->type],
			*label!=0?": ": "",
			label);

	for (a=0; a<es_num; a++)
	{
		const char *t, *d;

		d = dot_list (es[a]);
		t = g_strdup_printf ("%selement_%p -> element_%p;\n%s",
				ret, e, es[a], d);
		if (ret)
			g_free ((void*)ret);
		g_free ((void*)d);
		ret = t;
	}

	if (*label)
		g_free ((void*)label);

	return ret;
}

int input_line = 1, input_column;
static char *filename;

void yyerror (char const *s)
{
	fprintf (stderr, "%s:%d:%d: %s\n", filename,
			input_line, input_column, s);
}

int flex_debug;

static int list_functions (struct element *e)
{
	struct element *now;

	now = e;
	do
	{
		const char *name;

		if (now->type == element_type_functiondef)
		{
			struct symbol *s;

			s = now->data[0];

			name = s->str;
		}
		else if (now->type == element_type_predefined)
		{
			name = element_type_name[now->type];
		}
		else
		{
			error_element (now, "unknown defs\n");
			return -1;
		}

		printf ("%s\n", name);

		now = now->next;
	}
	while (now != e);

	return 0;
}

static int dot_defs (struct element *e, const char *function)
{
	struct element *now;

	now = e;
	do
	{
		const char *str, *name;

		if (now->type == element_type_functiondef)
		{
			struct symbol *s;

			s = now->data[0];

			str = dot (now);
			name = s->str;
		}
		else if (now->type == element_type_predefined)
		{
			str = dot (now);
			name = element_type_name[now->type];
		}
		else
		{
			error_element (now, "unknown defs\n");
			return -1;
		}

		if (!function ||
				!strcmp(function, name))
		{
			printf ("digraph %s {\n"
					"graph [rankdir=\"LR\",size=\"11,8\",rotate=90,center=1];\n"
					"%s}\n",
					name, str);
		}

		g_free ((void*)str);

		now = now->next;
	}
	while (now != e);

	return 0;
}


static void *nextbits (struct element *arg)
{
	struct stream_offset so;

	if (arg->type == element_type_empty)
	{
		/* flush any bytes in input buffer */
		get_bits (0, NULL);

		get_position (&so.bytes, &so.bits);

		return_value (stream_offset, so);
	}

	if (arg->type == element_type_num)
	{
		int byte_count_org, bit_count_org;
		int bits;
		unsigned int value;

		bits = (long)arg->data[0];

		get_bits (0, NULL);
		get_position (&byte_count_org, &bit_count_org);
		if (get_bits (bits, &value) < 0)
			return NULL;
		set_position (byte_count_org, bit_count_org);

		debug ("got %d bits, %x at %d.%d\n", bits, value,
				byte_count_org, bit_count_org);

		return_value (integer, value);
	}

	error_element (arg, "unknown type of argument\n");
	return NULL;
}

static void *stream_defined (struct element *arg)
{
	struct function *f;

	if (arg->type != element_type_string)
	{
		error_element (arg, "unknown argument\n");
		return NULL;
	}

	f = function_get (arg->data[0], NULL, NULL);

	return_value (boolean, !!f->symbol_local.next);
}

static void *bit_length (struct element *arg)
{
	if (arg->type != element_type_num)
	{
		error_element (arg, "unknown type of argument\n");
		return NULL;
	}

	return_value (bitlength, (long)arg->data[0]);
}

static void *set_input_buffer (struct element *arg)
{
	struct element *e;
	struct symbol *s1, *s2;
	int args;

	e = arg;
	args = 0;
	if (e)
	{
		do
		{
			args ++;
			e = e->next;
		}
		while (arg != e);
	}

	if (args != 2)
	{
		debug ("use file input. args, %d\n", args);
		set_memory_buffer (NULL, 0);
		return_value (nothing, 0);
	}

	if (arg->type != element_type_variable ||
			arg->next->type != element_type_variable)
	{
		error_element (arg, "expecting symbol, 1st\n");
		error_element (arg->next, "expecting symbol, 2ed\n");
		return NULL;
	}

	s1 = arg->data[0];
	s2 = arg->next->data[0];

	s1 = symbol_search (NULL, s1->str);
	s2 = symbol_search (NULL, s2->str);
	if (!s1 || !s2)
	{
		error ("symbol not defined. %p, %p\n", s1, s2);
		return NULL;
	}

	if (s1->array == NULL ||
			s1->array_depth != 1)
	{
		error ("%s is not one dimensional array, %p, %d\n",
				s1->str, s1->array, s1->array_depth);
		return NULL;
	}

	if (s1->array_size[0] < s2->value)
	{
		error ("too small array size, %d, %d\n",
				s1->array_size[0], (int)s2->value);
		return NULL;
	}

	debug ("setting %s, size %d as input buffer\n",
			s1->str, (int)s2->value);
	set_memory_buffer (s1->array, s2->value);
	set_input (s1->str);

	return_value (nothing, 0);
}

/* return the number of bits that was converted */
static int string_to_bits (const char *str,
		unsigned int *_ret)
{
	unsigned int ret = 0;
	int count = 0;
	const char *p = str;

	while (*p)
	{
		if (*p == ' ')
		{
			p ++;
			continue;
		}

		if (*p != '0' && *p != '1')
		{
			error ("cannot convert to bit stream. %s\n",
					p);
			return -1;
		}

		ret <<= 1;
		ret |= *p - '0';
		p ++;

		count ++;
	}

	*_ret = ret;
	debug ("string %s converted to 0x%x\n",
			str, ret);

	return count;
}

/*
 * return values
 *  > 0 : true
 * == 0 : false
 *  < 0 : error, cannot determin
 */
static int rv_test_true (struct return_value *r)
{
	const char *t;

	if (!r)
		return 0;

	if (r->type == return_type_boolean)
		return r->v.boolean;
	if (r->type == return_type_integer)
		return !!r->v.integer;
	if (r->type == return_type_symbol)
		return !!r->v.symbol->value;

	t = desc_return_value (r);
	error ("unknown type to test true. type:%s\n", t);
	g_free ((void *)t);

	return -1;
}

static int rv_sub (struct return_value *r1, struct return_value *r2, int *ret)
{
	const char *t1, *t2;

	if (!r1 || !r2)
		return -1;

	if (r1->type == return_type_stream_offset &&
			r2->type == return_type_string)
	{
		unsigned int bits1, bits2;
		int byte_count_org, bit_count_org;
		int bits;

		bits = string_to_bits (r2->v.string, &bits2);
		if (bits < 0)
			return -1;

		get_bits (0, NULL);
		get_position (&byte_count_org, &bit_count_org);
		if (get_bits (bits, &bits1) < 0)
			return -1;
		set_position (byte_count_org, bit_count_org);

		debug ("check %d bits, 0x%x, 0x%x\n", bits,
				bits1, bits2);
		*ret = bits1 - bits2;

		return 0;
	}

	if (r1->type == return_type_stream_offset &&
			r2->type == return_type_symbol)
	{
		struct symbol *s;
		unsigned int bits1;
		int byte_count_org, bit_count_org;
		int from, to;

		s = r2->v.symbol;

		if (s->bit_length == 0)
		{
			error ("no bit length for %s.\n", s->str);
			goto failed;
		}

		/* get bits */
		get_bits (0, NULL);
		get_position (&byte_count_org, &bit_count_org);
		if (get_bits (s->bit_length, &bits1) < 0)
			return -1;
		set_position (byte_count_org, bit_count_org);

		from = to = s->value;

		/* check number range */
		symbol_range (s, &from, &to);

		debug ("check %d bits, from 0x%x, bit 0x%x, to 0x%x\n",
				s->bit_length, from, bits1, to);
		*ret = !((from <= bits1) && (bits1 <= to));
		return 0;
	}

	if (r1->type == return_type_integer &&
			r2->type == return_type_integer)
	{
		*ret = r1->v.integer - r2->v.integer;
		return 0;
	}

	if (r1->type == return_type_symbol &&
			r2->type == return_type_integer)
	{
		*ret = r1->v.symbol->value - r2->v.integer;
		return 0;
	}

	if (r1->type == return_type_symbol &&
			r2->type == return_type_symbol)
	{
		struct symbol *s = r2->v.symbol;
		struct element *e;

		if (s->predefined)
		{
			int check = 0;

			debug ("compare %s(%d), %s\n",
					r1->v.symbol->str,
					(int)r1->v.symbol->value,
					r2->v.symbol->str);
			e = s->predefined;
			do
			{
				if (e->type == element_type_num)
				{
					debug ("number %d\n", (int)(long)e->data[0]);
					if (r1->v.symbol->value == (long)e->data[0])
					{
						*ret = 0;
						return 0;
					}

					check ++;
				}
				else if (e->type == element_type_num_range)
				{
					debug ("range %d - %d\n",
							(int)(long)e->data[0],
							(int)(long)e->data[1]);
					if ((long)e->data[0] <= r1->v.symbol->value &&
							r1->v.symbol->value <= (long)e->data[0])
					{
						*ret = 0;
						return 0;
					}

					check ++;
				}

				e = e->next;
			}
			while (e != s->predefined);

			if (check > 0)
			{
				*ret = 1;
				return 0;
			}
		}

		*ret = r1->v.symbol->value - r2->v.symbol->value;
		return 0;
	}

failed:
	t1 = desc_return_value (r1);
	t2 = desc_return_value (r2);
	error ("failed to test equal. type: %s, %s\n",
			t1, t2);
	g_free ((void *)t1);
	g_free ((void *)t2);

	return -1;
}

static void *parse_arrayindex (struct element *e,
		struct symbol *s, struct symbol *symbol_local,
		int data_size,
		int indent, FILE *out)
{
	struct element *e_tmp;
	void *array = s->array;
	int array_depth;
	int a;

	debug ("array, %s, data size %d\n", s->str, data_size);

	/* get array dimension */
	e_tmp = e;
	array_depth = 0;
	do
	{
		array_depth ++;
		e_tmp = e_tmp->next;
	}
	while (e_tmp != e);

	e_tmp = e;
	for (a=0; a<array_depth; a++)
	{
		struct return_value *r;
		int index;

		/* get index */
		r = parse_element (e_tmp->data[0], symbol_local, indent, out);
		if (!r)
			return NULL;
		if (r->type != return_type_integer)
		{
			const char *t;

			t = desc_return_value (r);
			error_element (e_tmp->data[0], "unknown type of array index. %s\n", t);
			g_free ((void*)t);
			g_free ((void*)r);
			return NULL;
		}

		index = r->v.integer;
		g_free ((void*)r);

		debug ("%s, dimension %dth, index %d\n", s->str, a, index);

		if (a == array_depth-1)
		{
			/* 실제 데이터 타입에 따른 주소 계산을 한다 */
			array = ((char*)array) + data_size*index;
		}
		else
			array = ((void**)array) + index;

		e_tmp = e_tmp->next;
	}

	return array;
}

static struct return_value *parse_element (struct element *e,
		struct symbol *symbol_local, int indent, FILE *out);

static int init_array (struct symbol *new,
		struct symbol *symbol_local, int indent, FILE *out)
{
	struct element *e, *p;
	int a;
	int depth;
	int *array_size;
	void *array;

	e = search_attribute (new->str, "__array_size");
	if (!e || e->type != element_type_functioncall)
	{
		error_element (e, "no predefined for %s\n", new->str);
		return -1;
	}

	p = e->data[1];
	depth = 0;
	do
	{
		depth ++;
		p = p->next;
	}
	while (p != e->data[1]);
	debug ("dimension %d for array %s\n", depth, new->str);

	if (depth > 2)
	{
		error ("depth %d is not yet implemented\n", depth);
		return -1;
	}

	array_size = calloc (sizeof (int), depth);
	p = e->data[1];
	for (a=0; a<depth; a++, p=p->next)
	{
		struct return_value *r;

		r = parse_element (p, symbol_local, indent, out);
		if (!r)
			return -1;
		if (r->type != return_type_integer)
		{
			error_element (p, "unknown type of array size\n");
			/* FIXME: free all array */
			free (array_size);
			return -1;
		}

		array_size[a] = r->v.integer;
		g_free ((void*)r);
		debug ("%dth dimension: size %d\n", a, array_size[a]);

		if (a == 0)
			array = calloc (sizeof (void *), array_size[a]);
		else
		{
			int b;
			void **arr;

			arr = array;
			for (b=0; b<array_size[a-1]; b++)
			{
				arr[b] = calloc (sizeof (void *), array_size[a]);
			}
		}
	}

	new->array = array;
	new->array_size = array_size;
	new->array_depth = depth;
	debug ("initialize array, %p, %s, %d dimension\n",
			new, new->str, new->array_depth);

	return 0;
}

static int set_value (struct element *variable, struct symbol *new, long value,
		struct symbol *symbol_local, int indent, FILE *out)
{
	/* find out returning value */
	if (variable->type == element_type_variable_array)
	{
		/* initialize array if not inited */
		if (!new->array && init_array (new, symbol_local, indent, out) < 0)
			return -1;


		if (search_attribute (new->str, "__array_data_type_byte"))
		{
			unsigned char *rvalue;

			rvalue = parse_arrayindex (variable->data[1],
					new, symbol_local, 1,
					indent, out);
			if (!rvalue)
				return -1;

			*rvalue = value;
			debug ("value %02x\n", *rvalue);
		}
		else
		{
			long *rvalue;

			rvalue = parse_arrayindex (variable->data[1],
					new, symbol_local, sizeof(*rvalue),
					indent, out);
			if (!rvalue)
				return -1;

			*rvalue = value;
		}
	}
	else
	{
		new->value = value;
	}

	return 0;
}

static void *parse_assign (struct element *var, struct element *val,
		struct symbol *symbol_local, int indent, FILE *out)
{
	struct return_value *val_ret;
	struct symbol *var_sym, *new;

	if (var->type != element_type_variable)
	{
		error_element (var, "not a variable\n");
		return NULL;
	}

	/* get value */
	val_ret = parse_element (val, symbol_local, indent, out);
	if (!val_ret)
		return NULL;

	if (val_ret->type != return_type_integer)
	{
		error_element (val, "not an integer\n");
		g_free ((void*)val_ret);
		return NULL;
	}

	/* set the value */
	var_sym = var->data[0];
	new = symbol_get (symbol_local, var_sym->str);
	if (!new)
	{
		g_free ((void*)val_ret);
		return NULL;
	}

	debug ("set %d on %s(%p)\n", (int)val_ret->v.integer, new->str, new);
	if (set_value (var, new, val_ret->v.integer,
			symbol_local, indent, out) < 0)
	{
		g_free ((void*)val_ret);
		return NULL;
	}

	g_free ((void*)val_ret);
	return_value (nothing, 0);
}

int error_count;
int allowed_errors;

static void *parse_bitsdesc_mpeg2 (struct element *e,
		struct symbol *symbol_local, int indent, FILE *out)
{
	void **data = e->data;
	struct element *variable = data[0];
	struct element *numberofbits = data[1];
	struct symbol *variable_s, *new;
	unsigned int value;
	int byte_pos, bit_pos;
	int byte_count_org, bit_count_org;
	int constant = 0;

	if (variable->type == element_type_variable ||
			variable->type == element_type_variable_array)
	{
		void **data = variable->data;

		variable_s = data[0];
	}
	else
	{
		error_element (e, "unknown variable type.\n");

		return NULL;
	}

	if (
			e->type != element_type_bitsdesc_mpeg2_string &&
			variable->type != element_type_variable_array &&
			symbol_check (symbol_local, variable_s->str)
	   )
	{
		error_element (e, "symbol %s, already defined\n",
				variable_s->str);

		return NULL;
	}

	constant = search_attribute (variable_s->str, "__const") != NULL;

	/* make new symbol */
	if (constant)
		new = search_predefined (variable_s->str);
	else
		new = symbol_get (symbol_local, variable_s->str);

	/* get value */
	if (numberofbits->type == element_type_num)
	{
		void **nob_data = numberofbits->data;
		int nob;

		nob = (int)(long)nob_data[0];
		debug ("get %d bits\n", nob);

		/* get current bit position */
		get_bits (0, NULL);
		get_position (&byte_count_org, &bit_count_org);

		byte_pos = byte_position ();
		bit_pos = bit_position ();

		/* get bits */
		if (get_bits (nob, &value) < 0)
			return NULL;
	}
	else if (
			e->type == element_type_bitsdesc_mpeg2_mnemonic &&
			numberofbits->type == element_type_num_range &&
			(enum mnemonic)data[2] == mnemonic_vlclbf
		)
	{
		error ("vlc not yet implemented\n");
		return NULL;
	}
	else if (
			e->type == element_type_bitsdesc_mpeg2_mnemonic &&
			variable->type == element_type_variable_array &&
			((struct element*)variable->data[1])->type == element_type_arrayindex &&
			numberofbits->type == element_type_multiply &&
			((struct element*)numberofbits->data[0])->type == element_type_num &&
			((struct element*)numberofbits->data[1])->type == element_type_num
		)
	{
		struct element *arrayindex, *index, *num1, *num2;
		int a;

		/* parse following syntax, no store on the symbol table yet
		 *
		 * intra_quantiser_matrix[64]          8*64 uimsbf
		 */

		arrayindex = variable->data[1];
		if (arrayindex->next != arrayindex)
		{
			return_status = return_internal_error;
			error_element (e, "not one demensional array.\n");
			return NULL;
		}

		index = arrayindex->data[0];
		if (index->type != element_type_num)
		{
			return_status = return_internal_error;
			error_element (e, "not number index.\n");
			return NULL;
		}

		num1 = numberofbits->data[0];
		num2 = numberofbits->data[1];

		if ((long)index->data[0] != (long)num2->data[0])
		{
			return_status = return_internal_error;
			error_element (e, "do not know the array number.\n");
			return NULL;
		}

		for (a=0; a<(long)index->data[0]; a++)
		{
			struct symbol *name = variable->data[0];
			unsigned int val;

			byte_pos = byte_position ();
			bit_pos = bit_position ();

			if (get_bits ((long)num1->data[0], &val) < 0)
				return NULL;

			fprintf (out, "%s (0x%06x)%8d.%d:%*s%-*s[%d] : 0x%x(%d)\n",
					get_display_input_name(),
					byte_pos, byte_pos, bit_pos,
					indent, "",
					50-indent-(a>9?4:3), name->str, a,
					val, val);
		}

		return_value (nothing, 0);
	}
	else
	{
		error_element (e, "unknown type of \"numberofbits\". type %s\n",
				element_type_name[numberofbits->type]);

		return NULL;
	}
	debug ("value, 0x%x\n", value);

	/* store the value */
	if (e->type == element_type_bitsdesc_mpeg2_string)
	{
		unsigned int bit_string;
		const char *str = data[2];
		const char *bitname;

		debug ("check 0x%x with %s\n", value, str);
		if (string_to_bits (str, &bit_string) < 0)
		{
			ferror_element (out, e, "unknown string, %s, in the element\n",
					str);
			return NULL;
		}

		if (value != bit_string)
		{
			//set_position (byte_count_org, bit_count_org);

			//return_status = return_bitstream_error;
			ferror_element (out, e, "unknown bit stream, %x != %x\n",
					value, bit_string);
			//return NULL;
		}

		/* set value */
		if (!constant)
			if (set_value (variable, new, value, symbol_local, indent, out) < 0)
				return NULL;

		bitname = description (variable, 0);
		fprintf (out, "%s (0x%06x)%8d.%d:%*s%-*s : 0x%x(%d)\n",
				get_display_input_name(),
				byte_pos, byte_pos, bit_pos,
				indent, "",
				50-indent, bitname,
				value, value);

		g_free ((void*)bitname);
	}
	else if (e->type == element_type_bitsdesc_mpeg2_mnemonic)
	{
		const char *bitname;
		struct symbol *pred;

		pred = search_predefined (new->str);

		bitname = description (variable, 0);

		if (pred && search_attribute_symbol (pred, "__const"))
		{
			int from, to;

			symbol_number (pred, &to);
			from = to;
			symbol_range (pred, &from, &to);

			if (from > value || to < value)
			{
				set_position (byte_count_org, bit_count_org);

				return_status = return_bitstream_error;
				fprintf (out, "%s (0x%06x)%8d.%d:%*s%-*s : 0x%x(%d) "
						"/* ERROR: changing constant variable. expected from %x to %x\n",
						get_display_input_name(),
						byte_pos, byte_pos, bit_pos,
						indent, "",
						50-indent, bitname,
						value, value, from, to);
				g_free ((void*)bitname);
				return NULL;
			}
		}

		/* set value */
		if (!constant)
			if (set_value (variable, new, value, symbol_local, indent, out) < 0)
				return NULL;

		fprintf (out, "%s (0x%06x)%8d.%d:%*s%-*s : 0x%x(%d)\n",
				get_display_input_name(),
				byte_pos, byte_pos, bit_pos,
				indent, "",
				50-indent, bitname,
				value, value);

		g_free ((void*)bitname);
	}
	else
	{
		error_element (e, "unknown type\n");

		return NULL;
	}

	return_value (nothing, 0);
}

static int h264_desc_notyet (struct element *var, struct element *desc, void *ret)
{
	error_element (desc, "net yet implemented\n");
	return -1;
}

static int h264_nob_frame_num (void)
{
	struct symbol *s;

	s = symbol_search (NULL, "log2_max_frame_num_minus4");
	if (!s)
	{
		error ("no log2_max_frame_num_minus4\n");
		return -1;
	}

	return s->value+4;
}

static int h264_desc_u (struct element *var, struct element *desc, void *ret)
{
	struct element *param = desc->data[1];
	int nob;
	unsigned int value;

	if (param->type == element_type_num)
		nob = (long)param->data[0];
	else if (param->type == element_type_variable &&
			!strcmp (((struct symbol*)param->data[0])->str, "v"))
	{
		struct
		{
			const char *name;
			int (*func)(void);
		} funcs[] =
		{
			{ "frame_num", h264_nob_frame_num, },
		};
#define funcs_num	(sizeof(funcs)/sizeof(funcs[0]))
		int a;
		const char *bitname = ((struct symbol*)var->data[0])->str;

		debug ("get bit length for %s\n", bitname);

		for (a=0; a<funcs_num; a++)
		{
			if (!strcmp (bitname, funcs[a].name))
			{
				nob = funcs[a].func ();
				if (nob < 0)
				{
					error_element (desc, "failed to get bit length for %s\n",
							bitname);

					return -1;
				}

				break;
			}
		}

		if (a == funcs_num)
		{
			error_element (desc, "unknown element for u(v)\n");
			return -1;
		}

		debug ("got %d\n", nob);
	}
	else
	{
		error_element (desc, "unknown type of param\n");
		return -1;
	}

	if (get_bits (nob, &value) < 0)
	{
		info ("EOS?\n");
		return -1;
	}

	*(unsigned int*)ret = value;

	return 0;
}

static int h264_exp_golomb (void)
{
	int leading_zero_bits;
	unsigned int b;
	int ret;

	leading_zero_bits = -1;
	ret = 1;
	b = 0;
	while (1)
	{
		if (get_bits (1, &b) < 0)
		{
			info ("EOS?\n");
			return -1;
		}

		leading_zero_bits ++;

		if (!b)
			ret *= 2;
		else
			break;
	}

	b = 0;
	if (leading_zero_bits)
	{
		if (get_bits (leading_zero_bits, &b) < 0)
		{
			info ("EOS?\n");
			return -1;
		}
	}

	ret = ret - 1 + b;
	debug ("%d zero bits, %d\n", leading_zero_bits, ret);

	return ret;
}

static int h264_desc_ue (struct element *var, struct element *desc, void *ret)
{
	int value;

	value = h264_exp_golomb ();
	if (value < 0)
		return -1;

	*(unsigned int*)ret = value;

	return 0;
}

static int h264_desc_se (struct element *var, struct element *desc, void *ret)
{
	int value;

	value = h264_exp_golomb ();
	if (value < 0)
		return -1;

	/* FIXME
	 * not yet implemented
	 */
	*(unsigned int*)ret = value;

	return 0;
}

static void *parse_bitsdesc_h264 (struct element *e,
		struct symbol *symbol_local, int indent, FILE *out)
{
	struct element *variable = e->data[0];
	//struct element *category = e->data[1];
	struct element *desc = e->data[2];
	struct symbol *var_sym, *new;

	if (variable->type == element_type_variable ||
			variable->type == element_type_variable_array)
	{
		var_sym = variable->data[0];
		new = symbol_get (symbol_local, var_sym->str);
	}
	else
	{
		error_element (e, "not supported variable type\n");

		return NULL;
	}

	if (desc->type == element_type_functioncall)
	{
		struct
		{
			const char *name;
			int (*func) (struct element *var, struct element *desc, void *ret);
		} descs[] =
		{
			{"ae"	, h264_desc_notyet, },
			{"b"	, h264_desc_u, },
			{"ce"	, h264_desc_notyet, },
			{"f"	, h264_desc_u, },
			{"i"	, h264_desc_notyet, },
			{"me"	, h264_desc_notyet, },
			{"se"	, h264_desc_se, },
			{"te"	, h264_desc_notyet, },
			{"u"	, h264_desc_u, },
			{"ue"	, h264_desc_ue, },
		};
#define sizeof_descs	(sizeof(descs)/sizeof(descs[0]))
		struct symbol *desc_type;
		int a;

		desc_type  = desc->data[0];

		for (a=0; a<sizeof_descs; a++)
		{
			if (!strcmp (desc_type->str, descs[a].name))
			{
				unsigned int value;
				const char *bitname, *descstr;
				int byte_pos, bit_pos;

				byte_pos = byte_position ();
				bit_pos = bit_position ();

				if (descs[a].func (variable, desc, &value) < 0)
				{
					error_element (e, "failed to get bit value\n");
					return NULL;
				}

				if (set_value (variable, new, value,
						symbol_local, indent, out) < 0)
					return NULL;

				bitname = description (variable, 0);
				descstr = description (desc, 0);

				fprintf (out, "%s (0x%06x)%8d.%d:%*s%-*s : 0x%x(%d) - %s\n",
						get_display_input_name(),
						byte_pos, byte_pos, bit_pos,
						indent, "",
						50-indent, bitname,
						value, value, descstr);

				g_free ((void*)bitname);
				g_free ((void*)descstr);

				return_value (nothing, 0);
			}
		}

		error_element (desc, "unknown bits description\n");
		return NULL;
	}
	else
	{
		error_element (e, "unknown bits description\n");

		return NULL;
	}

	return NULL;
}

#define rv_free(r) \
do{ \
	if (r->type == return_type_function_return) \
		g_free ((void*)r->v.function_return); \
	g_free ((void*)r); \
}while(0)

#define rv_check(r) \
do{ \
	if (!r) \
		return NULL; \
	if (r->type == return_type_function_return) \
		return r; \
}while(0)

void *parse_element_list (struct element *e, struct symbol *symbol_local, int indent, FILE *out)
{
	struct element *t;
	struct return_value *r;

	t = e;
	r = NULL;
	do
	{
		if (r)
			rv_free (r);
		r = parse_element (t, symbol_local, indent, out);
		rv_check (r);

		t = t->next;
	}
	while (t != e);

	return r;
}

#define single_element(e)	(((struct element*)e)->next == ((struct element*)e)->prev)

/*
 * return NULL for error
 */
static struct return_value *parse_element (struct element *e,
		struct symbol *symbol_local, int indent, FILE *out)
{
	void **data = e->data;
	struct symbol *s, *s1;
	struct return_value *r, *r1, *r2;
	int a;
	const char *str;

	switch (e->type)
	{
	case element_type_functioncall:
		s = data[0];
		return parse_function (s->str, data[1], indent, out);

	case element_type_logicalnot:
		r = parse_element (data[0], symbol_local, indent, out);
		if (!r)
			return NULL;

		a = rv_test_true (r);
		g_free (r);

		if (a < 0)
			return NULL;

		return_value (boolean, !a);

	case element_type_while:
while_do_again:
		r = parse_element (data[0], symbol_local, indent, out);
		if (!r)
			return NULL;

		a = rv_test_true (r);
		g_free (r);
		if (a < 0)
			return NULL;

		str = description (data[0], 0);
		fprintf (out, "%*s%swhile (%s)\n", indent+POSITION_LENGTH, "",
				a>0?"":"/*", str);
		g_free ((void*)str);

		if (a > 0)
		{
			fprintf (out, "%*s{\n", indent+POSITION_LENGTH, "");
			r = parse_element_list (data[1], symbol_local, indent+2, out);
			fprintf (out, "%*s}\n", indent+POSITION_LENGTH, "");

			rv_check (r);
			rv_free (r);

			goto while_do_again;
		}

		str = description_list (data[1], "", indent+2+POSITION_LENGTH);
		fprintf (out, "%*s{\n%s%*s}*/\n", indent+POSITION_LENGTH, "",
				str, indent+POSITION_LENGTH, "");
		g_free ((void*)str);

		return_value (nothing, 0);

	case element_type_dowhile:
dowhile_do_again:
		/* do body */
		fprintf (out, "%*sdo\n%*s{\n", indent+POSITION_LENGTH, "",
				indent+POSITION_LENGTH, "");
		r = parse_element_list (data[0], symbol_local, indent+2, out);
		if (!r)
			return NULL;
		fprintf (out, "%*s}\n", indent+POSITION_LENGTH, "");

		if (r->type == return_type_function_return)
			return r;
		g_free (r);

		/* check condition */
		r = parse_element (data[1], symbol_local, indent, out);
		if (!r)
			return NULL;
		a = rv_test_true (r);
		g_free (r);

		str = description (data[1], 0);
		fprintf (out, "%*swhile (%s)\n", indent+POSITION_LENGTH, "", str);
		g_free ((void*)str);

		if (a > 0)
			goto dowhile_do_again;

		return_value (nothing, 0);

	case element_type_for:
		{
			const char *s1, *s2, *s3;

			s1 = description (data[0], 0);
			s2 = description (data[1], 0);
			s3 = description (data[2], 0);

			fprintf (out, "%*sfor (%s; %s; %s)\n", indent+POSITION_LENGTH, "",
					s1, s2, s3);

			g_free ((void*)s1);
			g_free ((void*)s2);
			g_free ((void*)s3);
		}

		r = parse_element (data[0], symbol_local, indent, out);
		if (!r)
			return NULL;
		g_free ((void*)r);

for_do_again:
		r = parse_element (data[1], symbol_local, indent, out);
		if (!r)
			return NULL;
		a = rv_test_true (r);
		g_free ((void*)r);
		if (a < 0)
			return NULL;

		if (!a)
		{
			fprintf (out, "%*s{ /* test failed */ }\n", indent+POSITION_LENGTH, "");

			return_value (nothing, 0);
		}

		if (!single_element(data[3]))
			fprintf (out, "%*s{\n", indent+POSITION_LENGTH, "");
		r = parse_element_list (data[3], symbol_local, indent+2, out);
		if (!single_element(data[3]))
			fprintf (out, "%*s}\n", indent+POSITION_LENGTH, "");

		rv_check (r);
		rv_free (r);

		r = parse_element (data[2], symbol_local, indent, out);
		if (!r)
			return NULL;
		g_free ((void*)r);

		goto for_do_again;

	case element_type_if:
	case element_type_if_else:
		r = parse_element (data[0], symbol_local, indent, out);
		if (!r)
			return NULL;

		a = rv_test_true (r);
		g_free (r);

		if (a < 0)
			return NULL;

		str = description (data[0], 0);
		fprintf (out, "%*s%sif (%s)\n", indent+POSITION_LENGTH, "", a>0?"":"/*", str);
		g_free ((void*)str);

		if (a > 0)
		{
			fprintf (out, "%*s{\n", indent+POSITION_LENGTH, "");
			r = parse_element_list (data[1], symbol_local, indent+2, out);
			if (!r)
				return NULL;
			fprintf (out, "%*s}\n", indent+POSITION_LENGTH, "");

			if (r->type == return_type_function_return)
				return r;
			g_free (r);
		}
		else
		{
			str = description_list (data[1], "", indent+2+POSITION_LENGTH);
			fprintf (out, "%*s{\n"
					"%s"
					"%*s}*/\n", indent+POSITION_LENGTH, "",
					str, indent+POSITION_LENGTH, "");
			g_free ((void*)str);
		}

		if (a == 0 && e->type == element_type_if_else)
		{
			int comming_if = 0;

			if ((((struct element *)data[2])->type == element_type_if ||
						((struct element *)data[2])->type == element_type_if_else) &&
					((struct element *)data[2])->next == ((struct element *)data[2])->prev)
				comming_if = 1;

			fprintf (out, "%*selse\n", indent+POSITION_LENGTH, "");
			if (!comming_if)
				fprintf (out, "%*s{\n", indent+POSITION_LENGTH, "");
			r = parse_element_list (data[2], symbol_local, indent+(comming_if?0:2), out);
			if (!r)
				return NULL;
			if (!comming_if)
				fprintf (out, "%*s}\n", indent+POSITION_LENGTH, "");

			if (r->type == return_type_function_return)
				return r;
			g_free (r);
		}
		else if (e->type == element_type_if_else)
		{
			str = description_list (data[2], "", indent+2+POSITION_LENGTH);
			fprintf (out, "%*s/*else\n%*s{\n%s%*s}*/\n",
					indent+POSITION_LENGTH, "",
					indent+POSITION_LENGTH, "", str,
					indent+POSITION_LENGTH, "");
			g_free ((void*)str);
		}

		return_value (nothing, 0);

	case element_type_assign:
		parse_assign (e->data[0], e->data[1], symbol_local, indent, out);
		return_value (nothing, 0);

	case element_type_notequal:
	case element_type_equal:
	case element_type_gt:
	case element_type_ge:
	case element_type_lt:
	case element_type_le:
		r1 = parse_element (data[0], symbol_local, indent, out);
		r2 = parse_element (data[1], symbol_local, indent, out);
		if (rv_sub (r1, r2, &a) < 0)
			goto sub_failed;

		debug ("compared:%d\n", a);

		if (!r1)
			g_free (r1);
		if (!r2)
			g_free (r2);

		switch (e->type)
		{
		case element_type_equal:
			a = !a;
			break;

		case element_type_gt:
			a = a > 0;
			break;

		case element_type_ge:
			a = a >= 0;
			break;

		case element_type_lt:
			a = a < 0;
			break;

		case element_type_le:
			a = a <= 0;
			break;

		default:
			a = !!a;
			break;
		}

		return_value (boolean, a);

sub_failed:
		if (!r1)
			g_free (r1);
		if (!r2)
			g_free (r2);

		return NULL;

	case element_type_logicalor:
	case element_type_logicaland:
		/* check first one */
		r1 = parse_element (data[0], symbol_local, indent, out);
		if (!r1)
			return NULL;
		a = rv_test_true (r1);
		g_free (r1);
		if (a < 0)
			return NULL;

		if (e->type == element_type_logicalor)
		{
			/* logical or */
			if (a > 0)
				return_value (boolean, a);
		}
		else
		{
			/* logical and */
			if (a == 0)
				return_value (boolean, a);
		}

		/* and second one */
		r2 = parse_element (data[1], symbol_local, indent, out);
		if (!r2)
			return NULL;
		a = rv_test_true (r2);
		g_free (r2);
		if (a < 0)
			return NULL;

		return_value (boolean, a);

	case element_type_multiply:
		/* check first one */
		r1 = parse_element (data[0], symbol_local, indent, out);
		if (!r1)
			return NULL;

		/* and second one */
		r2 = parse_element (data[1], symbol_local, indent, out);
		if (!r2)
		{
			g_free ((void*)r1);
			return NULL;
		}

		if (r1->type != return_type_integer || r2->type != return_type_integer)
		{
			error_element (e, "not supported return type\n");
			g_free ((void*)r1);
			g_free ((void*)r2);
			return NULL;
		}

		a = r1->v.integer * r2->v.integer;
		debug ("%d = %d * %d\n", a,
				(int)r1->v.integer, (int)r2->v.integer);
		g_free ((void*)r1);
		g_free ((void*)r2);
		return_value (integer, a);

	case element_type_plusplus_post:
		if (((struct element*)e->data[0])->type != element_type_variable)
		{
			error_element (e, "unknown type\n");
			return NULL;
		}

		s = ((struct element*)e->data[0])->data[0];
		s = symbol_search (symbol_local, s->str);
		if (!s)
			return NULL;

		a = s->value;
		s->value ++;

		return_value (integer, a);

	case element_type_plusassign:
		if (((struct element*)e->data[0])->type != element_type_variable)
		{
			error_element (e, "unknown type\n");
			return NULL;
		}

		s = ((struct element*)e->data[0])->data[0];
		s = symbol_search (symbol_local, s->str);
		if (!s)
			return NULL;

		if (((struct element*)e->data[1])->type == element_type_variable)
		{
			s1 = ((struct element*)e->data[1])->data[0];;
			s1 = symbol_search (symbol_local, s1->str);
			if (!s1)
				return NULL;

			s->value += s1->value;
			debug ("%s(%d) += %s(%d)\n", s->str, (int)s->value,
					s1->str, (int)s1->value);
		}
		else if (((struct element*)e->data[1])->type == element_type_num)
		{
			s->value += (long)((struct element*)e->data[1])->data[0];
			debug ("%s(%d) += %d\n", s->str, (int)s->value,
					(int)(long)((struct element*)e->data[1])->data[0]);
		}
		else
		{
			error_element (e, "unknown type of operand.\n");
			return NULL;
		}

		return_value (nothing, 0);

	case element_type_string:
		return_value (string, (const char *)data[0]);

	case element_type_num:
		return_value (integer, (long long)data[0]);

	case element_type_variable:
		s = data[0];
		s1 = symbol_search (symbol_local, s->str);
		if (!s1)
			return NULL;

		return_value (symbol, s1);

	case element_type_variable_array:
		s = data[0];
		s1 = symbol_search (symbol_local, s->str);
		if (!s1)
			return NULL;

		if (!s1->array)
		{
			error ("not an array. %s\n", s->str);
			return NULL;
		}

		if (search_attribute (s1->str, "__array_data_type_byte"))
		{
			unsigned char *array;

			array = parse_arrayindex (data[1],
					s1, symbol_local, sizeof(*array),
					indent, out);

			return_value (integer, *array);
		}
		else
		{
			long *array;

			array = parse_arrayindex (data[1],
					s1, symbol_local, sizeof(*array),
					indent, out);

			return_value (integer, *array);
		}

	case element_type_bitsdesc_mpeg2_string:
	case element_type_bitsdesc_mpeg2_mnemonic:
		return parse_bitsdesc_mpeg2 (e, symbol_local, indent, out);

	case element_type_bitsdesc_h264:
		return parse_bitsdesc_h264 (e, symbol_local, indent, out);

	case element_type_h264_func_cat:
		return parse_element (e->data[0], symbol_local, indent, out);

	case element_type_return:
		r1 = parse_element (data[0], symbol_local, indent, out);
		return_value (function_return, r1);

	case element_type_empty:
		return_value (nothing, 0);

	default:
		break;
	}

	error_element (e, "not implemented element. type:%s\n",
			element_type_name [e->type]);

	return NULL;
}

static struct return_value *parse_function (const char *function_name,
		struct element *args, int indent, FILE *out)
{
	struct function *func;
	const char *args_desc;
	void *ret;
	static struct symbol *donot_parse;
	static int donot_parse_inited;

	if (!donot_parse_inited)
	{
		donot_parse = search_predefined ("__donot_parse");
		donot_parse_inited = 1;
	}
	if (donot_parse)
	{
		struct element *e;

		e = donot_parse->predefined;
		do
		{
			if (e->type == element_type_variable)
			{
				if (!strcmp (((struct symbol *)e->data[0])->str, function_name))
				{
					debug ("skip function, %s\n", function_name);
					return_value (nothing, 0);
				}
			}

			e = e->next;
		} while (e != donot_parse->predefined);
	}

	func = function_get (function_name, NULL, NULL);
	if (func == NULL)
		goto unknown;

	if (func->builtin)
	{
		return func->builtin (args);
	}

	if (func->element)
	{
		void **data = func->element->data;
		FILE *o = out;

		fprintf (o, "%*s%s ()\n",
				indent+POSITION_LENGTH, "",
				function_name);

		if (search_attribute (function_name, "__no_print"))
			o = null_out;

		fprintf (o, "%*s{\n",
				indent+POSITION_LENGTH, "");

		if (func->element->type != element_type_functiondef)
		{
			error_element (func->element, "unknown element\n");
			return NULL;
		}

		/* free local symbol table */
		symbol_free (&func->symbol_local);

		/* parse the function */
		ret = parse_element_list (data[2], &func->symbol_local, indent+2, o);

		fprintf (o, "%*s}\n", indent+POSITION_LENGTH, "");

		return ret;
	}

	error ("BUG??\n");

	return NULL;

unknown:
	args_desc = description (args, 0);
	error ("call of unknown function, %s(%s)\n",
			function_name,
			args_desc
			);
	g_free ((void*)args_desc);

	return NULL;
}

int main (int argc, char **argv)
{
	int ret;
	int opt;
	char *do_dot;
	int do_desc;
	int do_list_functions;
	char *initial_function = "start_parser";
	char *input_filename;

	filename = NULL;
	do_dot = NULL;
	do_desc = 0;
	do_list_functions = 0;
	while (1)
	{
		int option_index = 0;
		const struct option longopts[] =
		{
			{"list-functions",	no_argument,	&do_list_functions,	1 },
			{0,		0,	0,	0 }
		};

		opt = getopt_long (argc, argv, "-d:Ds:le:n:f:",
				longopts, &option_index);
		if (opt == -1)
			break;

		switch (opt)
		{
		default:
		case '?':
printf (
"iso13818-2 stream parser\n"
"  # ./mpeg2 <arguments> ... <ES syntax file>\n"
"arguments:\n"
" -d <function name>   : print dot graph for a function\n"
" -D                   : print syntax description\n"
" -l                   : increase debug level\n"
" -s <input stream>    : input element stream\n"
" -n <file index>      : starting file index when input stream has %%d.\n"
" -e <number of error> : errors to allowed. stop parsing after the errors.\n"
" -f <function name>   : initial function name to parse\n"
" --list-functions     : list known functions\n"
);
			return 1;

		case 0:
			break;

		case 1:
			filename = optarg;
			break;

		case 'd':
			do_dot = optarg;
			break;

		case 'D':
			do_desc = 1;
			break;

		case 's':
			input_filename = optarg;
			break;

		case 'l':
			debug_level ++;
			break;

		case 'e':
			allowed_errors = atoi (optarg);
			break;

		case 'n':
			set_input_number (atoi (optarg));
			break;

		case 'f':
			initial_function = optarg;
			break;
		}
	}

	null_out = fopen ("/dev/null", "r");

	//extern int yydebug;
	//yydebug = 1;
	//flex_debug = 1;

	if (filename)
	{
		FILE *fd;
		void yyset_in (FILE *  in_str );

		fd = fopen (filename, "r");
		if (fd == NULL)
		{
			printf ("fopen failed. %s\n", strerror (errno));
			return 1;
		}

		yyset_in (fd);
	}
	else
	{
		printf ("no input file\n");
		return 1;
	}

	{
		extern int yyparse (void);

		ret = yyparse ();
		//printf ("ret %d\n", ret);
	}
	if (ret != 0)
	{
		printf ("parse failed %d\n", ret);
		return 1;
	}

	if (do_list_functions)
	{
		list_functions (root_element);
		return 0;
	}

	if (do_dot)
	{
		dot_defs (root_element, do_dot);
		return 0;
	}

	if (do_desc)
	{
		const char *str;

		str = description_list (root_element, "", 0);
		puts (str);
		g_free ((void*)str);

		return 0;
	}

	/* builtin functions */
	function_get ("bytealigned", NULL, bytealigned);
	function_get ("byte_aligned", NULL, bytealigned);
	function_get ("nextbits", NULL, nextbits);
	function_get ("next_bits", NULL, nextbits);
	function_get ("__stream_defined", NULL, stream_defined);
	function_get ("__bit_length", NULL, bit_length);
	function_get ("__set_input_buffer", NULL, set_input_buffer);
	function_get ("more_rbsp_data", NULL, more_data);

	if (input_filename)
	{
		struct return_value *r;
		const char *desc;

		set_input (input_filename);
		get_bits (0, NULL);

parse_again:
		r = parse_function (initial_function, NULL, 0, stdout);

		/* ignore errors if possible */
		if (r == NULL && return_status == return_bitstream_error &&
				(
				 allowed_errors < 0 ||
				 error_count < allowed_errors
				)
		   )
		{
			unsigned int tmp;

			error_count ++;
			printf ("%s (0x%06x)%8d.%d:%-*s\n",
					get_display_input_name(),
					byte_position(), byte_position(), bit_position (),
					50-0, "ERROR: stream errror, parse again...");

			if (bit_position ()>0)
				get_bits (8-bit_position (), &tmp);
			else
				get_bits (8, &tmp);

			return_status = return_internal_error;
			goto parse_again;
		}

		/* try again in normal case */
		if (r)
			goto parse_again;

		desc = desc_return_value (r);
		printf ("returned %s, return_status %d\n", desc, return_status);
		g_free ((void*)desc);
	}

#if 0
	{
		int token;
		while ((token = yylex()))
		{
			printf ("token %d\n", token);
		}
	}
#endif

	return 0;
}

