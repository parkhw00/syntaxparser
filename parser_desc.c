
#include <glib.h>
#include <glib/gprintf.h>

#include "parser_desc.h"
#include "parser.h"

#define COLUMN_NOB 50
#define WIDTH_NOB "4"

const char *description_list (struct element *e, const char *gap, int indent)
{
	struct element *now;
	const char *ret;

	ret = description (e, indent);

	now = e->next;
	while (e != now)
	{
		const char *new, *tmp;

		new = description (now, indent);
		tmp = g_strdup_printf ("%s%s%s", ret, gap, new);
		g_free ((void*)ret);
		g_free ((void*)new);
		ret = tmp;

		now = now->next;
	}

	return ret;
}

static const char *description_statement (struct element *e, int indent)
{
	const char *str, *ret;

	ret = description_list (e, "", indent+2);

	if (e->next != e)
	{
		str = g_strdup_printf ("%*s{\n%s%*s}\n",
				indent, "", ret, indent, "");
		g_free ((void*)ret);
		ret = str;
	}

	return ret;
}

const char *description (struct element *e, int indent)
{
	void **data;
	struct symbol *s;
	const char *ret;
	const char *tmp, *str1, *str2, *str3, *str4;
	struct element *e_tmp;

	if (!e)
		return g_strdup ("<null>");

	data = e->data;

	switch (e->type)
	{
	case element_type_num_or:
		return g_strdup_printf ("%d or %d",
				(int)(long)data[0], (int)(long)data[1]);

	case element_type_num_range:
		return g_strdup_printf ("%d-%d",
				(int)(long)data[0], (int)(long)data[1]);

	case element_type_num:
		return g_strdup_printf ("%d", (int)(long)data[0]);

	case element_type_string:
		return g_strdup_printf ("\"%s\"", (char*)(long)data[0]);

	case element_type_empty:
		if (indent == 0)
		{
			//return g_strdup ("/* empty */");
			return g_strdup ("");
		}
		else
			return g_strdup_printf ("%*s{ /* empty */ }\n", indent, "");

	case element_type_arrayindex:
		ret = NULL;
		e_tmp = e;
		do
		{
			str1 = description (e_tmp->data[0], 0);
			tmp = g_strdup_printf ("%s[%s]", ret?ret:"", str1);
			g_free ((void*)str1);

			if (ret)
				g_free ((void*)ret);
			ret = tmp;

			e_tmp = e_tmp->next;
		}
		while (e_tmp != e);

		return ret;

	case element_type_variable_array:
		s = data[0];
		str1 = description (data[1], 0);
		ret = g_strdup_printf ("%s%s", s->str, str1);
		g_free ((void*)str1);
		return ret;

	case element_type_variable:
		s = data[0];
		return g_strdup (s->str);

	case element_type_logicalnot:
		str1 = description (data[0], 0);
		ret = g_strdup_printf ("!%s", str1);
		g_free ((void*)str1);
		return ret;

	case element_type_plusplus_pre:
		str1 = description (data[0], 0);
		if (indent == 0)
			ret = g_strdup_printf ("++%s", str1);
		else
			ret = g_strdup_printf ("%*s++%s\n", indent, "",
					str1);
		g_free ((void*)str1);
		return ret;
	case element_type_plusplus_post:
		str1 = description (data[0], 0);
		if (indent == 0)
			ret = g_strdup_printf ("%s++", str1);
		else
			ret = g_strdup_printf ("%*s%s++\n", indent, "",
					str1);
		g_free ((void*)str1);
		return ret;
	case element_type_minusminus_pre:
		str1 = description (data[0], 0);
		if (indent == 0)
			ret = g_strdup_printf ("--%s", str1);
		else
			ret = g_strdup_printf ("%*s--%s\n", indent, "",
					str1);
		g_free ((void*)str1);
		return ret;
	case element_type_minusminus_post:
		str1 = description (data[0], 0);
		if (indent == 0)
			ret = g_strdup_printf ("%s--", str1);
		else
			ret = g_strdup_printf ("%*s%s--\n", indent, "",
					str1);
		g_free ((void*)str1);
		return ret;

		/* something between two element */
	case element_type_multiply:
	case element_type_modulo:
	case element_type_gt:
	case element_type_ge:
	case element_type_lt:
	case element_type_le:
	case element_type_leftshift:
	case element_type_rightshift:
	case element_type_plus:
	case element_type_minus:
	case element_type_assign:
	case element_type_plusassign:
	case element_type_logicaland:
	case element_type_logicalor:
	case element_type_bitwiseand:
	case element_type_bitwiseor:
	case element_type_equal:
	case element_type_notequal:
		if (e->type == element_type_multiply)		tmp = "*";
		else if (e->type == element_type_modulo)	tmp = "%";
		else if (e->type == element_type_gt)		tmp = ">";
		else if (e->type == element_type_ge)		tmp = ">=";
		else if (e->type == element_type_lt)		tmp = "<";
		else if (e->type == element_type_le)		tmp = "<=";
		else if (e->type == element_type_leftshift)	tmp = "<<";
		else if (e->type == element_type_rightshift)	tmp = ">>";
		else if (e->type == element_type_plus)		tmp = "+";
		else if (e->type == element_type_minus)		tmp = "-";
		else if (e->type == element_type_assign)	tmp = "=";
		else if (e->type == element_type_plusassign)	tmp = "+=";
		else if (e->type == element_type_logicaland)	tmp = "&&";
		else if (e->type == element_type_logicalor)	tmp = "||";
		else if (e->type == element_type_bitwiseand)	tmp = "&";
		else if (e->type == element_type_bitwiseor)	tmp = "|";
		else if (e->type == element_type_equal)		tmp = "==";
		else if (e->type == element_type_notequal)	tmp = "!=";
		else						tmp = "???";

		str1 = description (data[0], 0);
		str2 = description (data[1], 0);
		if (indent == 0)
			ret = g_strdup_printf ("%s%s%s", str1, tmp, str2);
		else
			ret = g_strdup_printf ("%*s%s%s%s\n", indent, "",
					str1, tmp, str2);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_bitsdesc_mpeg2_string:
		str1 = description (data[0], 0);
		str2 = description (data[1], 0);
		ret = g_strdup_printf ("%*s%-*s %"WIDTH_NOB"s %s\n", indent, "",
				COLUMN_NOB-indent, str1, str2, (const char*)data[2]);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_bitsdesc_mpeg2_mnemonic:
		str1 = description (data[0], 0);
		str2 = description (data[1], 0);
		switch ((enum mnemonic)data[2])
		{
		default: tmp = "unknown"; break;
		case mnemonic_bslbf : tmp = "bslbf";  break;
		case mnemonic_uimsbf: tmp = "uimsbf"; break;
		case mnemonic_simsbf: tmp = "simsbf"; break;
		case mnemonic_vlclbf: tmp = "vlclbf"; break;
		}
		ret = g_strdup_printf ("%*s%-*s %"WIDTH_NOB"s %6s\n", indent, "",
				COLUMN_NOB-indent, str1, str2, tmp);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_bitsdesc_h264:
		str1 = description (e->data[0], 0);
		str2 = description (e->data[1], 0);
		str3 = description (e->data[2], 0);
		ret = g_strdup_printf ("%*s%-*s %"WIDTH_NOB"s %6s\n", indent, "",
				COLUMN_NOB-indent, str1, str2, str3);
		g_free ((void*)str1);
		g_free ((void*)str2);
		g_free ((void*)str3);
		return ret;

	case element_type_h264_func_cat:
		str1 = description (e->data[0], 0);
		str2 = description (e->data[1], 0);
		ret = g_strdup_printf ("%*s%-*s %"WIDTH_NOB"s\n", indent, "",
				COLUMN_NOB-indent, str1, str2);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_for:
		str1 = description (data[0], 0);
		str2 = description (data[1], 0);
		str3 = description (data[2], 0);
		str4 = description_statement (data[3], indent);
		ret = g_strdup_printf ("%*sfor (%s; %s; %s)\n%s", indent, "",
				str1, str2, str3, str4);
		g_free ((void*)str1);
		g_free ((void*)str2);
		g_free ((void*)str3);
		g_free ((void*)str4);
		return ret;

	case element_type_dowhile:
		str1 = description_statement (data[0], indent);
		str2 = description (data[1], 0);
		ret = g_strdup_printf ("%*sdo\n%s%*swhile (%s)\n",
				indent, "", str1, indent, "", str2);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_while:
	case element_type_if:
		if (e->type == element_type_while)	tmp = "while";
		else if (e->type == element_type_if)	tmp = "if";
		else					tmp = "???";

		str1 = description (data[0], 0);
		str2 = description_statement (data[1], indent);
		ret = g_strdup_printf ("%*s%s (%s)\n%s",
				indent, "", tmp, str1, str2);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_if_else:
		str1 = description (data[0], 0);
		str2 = description_statement (data[1], indent);
		if (((struct element*)data[2])->type == element_type_if_else)
		{
			str3 = description (data[2], indent);
			tmp = str3;
			while (*tmp == ' ')
				tmp ++;

			ret = g_strdup_printf ("%*sif (%s)\n%s%*selse %s",
					indent, "", str1, str2, indent, "", tmp);
		}
		else
		{
			str3 = description_statement (data[2], indent);

			ret = g_strdup_printf ("%*sif (%s)\n%s%*selse\n%s",
					indent, "", str1, str2, indent, "", str3);
		}
		g_free ((void*)str1);
		g_free ((void*)str2);
		g_free ((void*)str3);
		return ret;

	case element_type_functiondef:
		s = data[0];
		str1 = description_list (data[1], ", ", 0);
		str2 = description_statement (data[2], indent);
		ret = g_strdup_printf ("%*s%s (%s)\n%s\n", indent, "", s->str, str1, str2);
		g_free ((void*)str1);
		g_free ((void*)str2);
		return ret;

	case element_type_functioncall:
		s = data[0];
		str1 = description_list (data[1], ", ", 0);
		if (indent == 0)
			ret = g_strdup_printf ("%s(%s)", s->str, str1);
		else
			ret = g_strdup_printf ("%*s%s (%s)\n", indent, "", s->str, str1);
		g_free ((void*)str1);
		return ret;

	case element_type_predefined:
		str1 = description_statement (data[0], indent);
		ret = g_strdup_printf ("%*s__predefined\n%s\n", indent, "", str1);
		return ret;

	default:
		return g_strdup_printf ("(not yet, type %s)", element_type_name[e->type]);
	}
}

