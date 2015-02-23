
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "parser.h"
#include "parser_input.h"

//static int debug_level = 1;
extern int debug_level;

/* memory input */
static const unsigned char *mem_data;
static int mem_size;
static int mem_byte_count, mem_bit_count;
static const char *mem_name;

/* file input */
static const char *es_filename;
static char *es_filename_cur;
static int es_filename_len;
static int file_count = -1;
static FILE *in;
static unsigned char buf[16];
static int buf_filled;
static int byte_count;
static int bit_count;
static int stream_readed;

static void dump (const unsigned char *data, int size)
{
	int a;

	printf ("dump %d bytes:", size);
	for (a=0; a<size; a++)
		printf (" %02x", data[a]);
	printf ("\n");
}

int set_input (const char *name)
{
	if (!mem_data)
		es_filename = name;
	else
		mem_name = name;

	return 0;
}

int set_input_number (int num)
{
	file_count = num;
	return 0;
}

const char *get_display_input_name (void)
{
	if (!mem_data)
		return es_filename_cur;
	else
		return mem_name;
}

int get_display_input_name_len (void)
{
	if (!mem_data)
		return es_filename_len;
	else
		return strlen (mem_name);
}

static int read_more_data (unsigned char *data, int size)
{
	int ret;

	if (!in)
	{
next_file:
		if (es_filename_cur)
			g_free ((void*)es_filename_cur);
		es_filename_cur = g_strdup_printf (es_filename, file_count);

		debug ("parse stream, %s\n", es_filename_cur);
		in = fopen (es_filename_cur, "rb");
		if (!in)
		{
			error ("fopen failed, %s, %s\n",
					es_filename_cur, strerror (errno));
			return_status = return_end_of_stream;
			return -1;
		}

		/* remove directory name */
		{
			char *t;

			t = strrchr (es_filename_cur, '/');
			if (t)
			{
				t = g_strdup (t+1);
				g_free ((void*)es_filename_cur);
				es_filename_cur = t;
			}
		}
		es_filename_len = strlen (es_filename_cur);
	}

	if (size == 0)
		return 0;

	ret = fread (data, 1, size, in);

	if (ret == 0)
	{
		debug ("no more data\n");
		fclose (in);
		in = NULL;

		if (file_count < 0)
			return -1;

		file_count ++;
		stream_readed = 0;

		goto next_file;
	}
	if (debug_level > 0)
		dump (data, ret);

	stream_readed += ret;

	return ret;
}

static int _get_bits (int bits, unsigned int *_ret,
		unsigned char *buf, int buf_size,
		int *byte_count, int *bit_count, int *buf_filled,
		int (*read_more_data)(unsigned char *data, int size))
{
	unsigned int ret = 0;

	/* read bits */
	debug ("read %d bits from %d.%d\n", bits, *byte_count, *bit_count);
	if (debug_level > 0)
		dump (buf + *byte_count, (*bit_count + 7) / 8);

	while (bits > 0)
	{
		int bit;
		unsigned char tmp;
		static const unsigned char mask[8] =
		{ 0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, };

		if (*byte_count >= *buf_filled)
		{
			int got;

			debug ("read %d bytes to %d, \n",
					(int)buf_size-*buf_filled, *buf_filled);
			if (!read_more_data)
				return -1;

			got = read_more_data (buf+*buf_filled, buf_size-*buf_filled);
			if (got < 0)
				return -1;

			*buf_filled += got;
		}

		bit = bits;
		if (*bit_count + bit > 8)
			bit = 8 - *bit_count;
		debug ("get %d bits from %d.%d\n",
				bit,
				*byte_count, *bit_count);

		tmp = (buf[*byte_count] & mask[*bit_count]) >> (8-*bit_count-bit);

		ret <<= bit;
		ret |= tmp;

		bits -= bit;

		/* update counts */
		*bit_count += bit;
		if (*bit_count >= 8)
		{
			(*byte_count) ++;
			*bit_count -= 8;
		}
	}

	if (_ret)
	{
		debug ("got 0x%x\n", ret);
		*_ret = ret;
	}

	return 0;
}

int get_bits (int bits, unsigned int *_ret)
{
	if (!mem_data)
	{
		/* flush previously used data */
		if (
				byte_count > 0 &&
				byte_count < buf_filled
		   )
		{
			debug ("move %d bytes to the front\n",
					buf_filled-byte_count);

			memmove (buf, buf+byte_count, buf_filled-byte_count);

			buf_filled -= byte_count;
			byte_count = 0;
		}

		return _get_bits (bits, _ret, buf, sizeof(buf),
				&byte_count, &bit_count, &buf_filled,
				read_more_data);
	}
	else
	{
		return _get_bits (bits, _ret, (unsigned char*)mem_data, mem_size,
				&mem_byte_count, &mem_bit_count, &mem_size,
				NULL);
	}
}

int byte_position (void)
{
	if (!mem_data)
		return stream_readed - buf_filled + byte_count;
	else
		return mem_byte_count;
}

int bit_position (void)
{
	if (!mem_data)
		return bit_count;
	else
		return mem_bit_count;
}

void get_position (int *byte, int *bit)
{
	if (!mem_data)
	{
		*byte = byte_count;
		*bit  = bit_count;
	}
	else
	{
		*byte = mem_byte_count;
		*bit  = mem_bit_count;
	}
}

void set_position (int byte, int bit)
{
	if (!mem_data)
	{
		byte_count = byte;
		bit_count  = bit;
	}
	else
	{
		mem_byte_count = byte;
		mem_bit_count  = bit;
	}
}

void *more_data (struct element *arg)
{
	if (!mem_data)
	{
		error ("not specified at file input\n");
		return NULL;
	}

	debug ("mem_byte_count %d.%d, mem_size %d\n",
			mem_byte_count, mem_bit_count, mem_size);
	return_value (boolean, mem_byte_count+1 < mem_size);
}

void *bytealigned (struct element *arg)
{
	if (!mem_data)
		return_value (boolean, bit_count==0);
	else
		return_value (boolean, mem_bit_count==0);
}

int set_memory_buffer (const unsigned char *data, int size)
{
	mem_data = data;
	mem_size = size;
	mem_byte_count = 0;
	mem_bit_count = 0;

	if (data)
		debug ("set memory input %p, %d\n", data, size);
	else
		debug ("use file input\n");

	return 0;
}
