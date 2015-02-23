
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "calc.h"
#include "mfcalc.yacc.h"
     
symrec * putsym (char const *sym_name, int sym_type)
{
	symrec *ptr = (symrec *) malloc (sizeof (symrec));
	ptr->name = (char *) malloc (strlen (sym_name) + 1);
	strcpy (ptr->name,sym_name);
	ptr->type = sym_type;
	ptr->value.var = 0; /* Set value to 0 even if fctn.  */
	ptr->next = (struct symrec *)sym_table;
	sym_table = ptr;
	return ptr;
}
     
symrec * getsym (char const *sym_name)
{
	symrec *ptr;
	for (ptr = sym_table; ptr != (symrec *) 0;
			ptr = (symrec *)ptr->next)
		if (strcmp (ptr->name,sym_name) == 0)
			return ptr;
	return 0;
}

#if 0
int yylex (void)
{
	int c;

	/* Ignore white space, get first nonwhite character.  */
	while ((c = getchar ()) == ' ' || c == '\t')
		continue;

	if (c == EOF)
		return 0;

	/* Char starts a number => parse the number.         */
	if (c == '.' || isdigit (c))
	{
		ungetc (c, stdin);
		scanf ("%lf", &yylval.val);
		return NUM;
	}

	/* Char starts an identifier => read the name.       */
	if (isalpha (c))
	{
		/* Initially make the buffer long enough
		 *               for a 40-character symbol name.  */
		static size_t length = 40;
		static char *symbuf = 0;
		symrec *s;
		int i;

		if (!symbuf)
			symbuf = (char *) malloc (length + 1);

		i = 0;
		do
		{
			/* If buffer is full, make it bigger.        */
			if (i == length)
			{
				length *= 2;
				symbuf = (char *) realloc (symbuf, length + 1);
			}
			/* Add this character to the buffer.         */
			symbuf[i++] = c;
			/* Get another character.                    */
			c = getchar ();
		}
		while (isalnum (c));

		ungetc (c, stdin);
		symbuf[i] = '\0';

		s = getsym (symbuf);
		if (s == 0)
			s = putsym (symbuf, VAR);
		yylval.tptr = s;
		return s->type;
	}

	/* Any other character is a token by itself.        */
	return c;
}
#endif

/* Called by yyparse on error.  */
void yyerror (char const *s)
{
	fprintf (stderr, "%s\n", s);
}

struct init
{
	char const *fname;
	double (*fnct) (double);
};

struct init const arith_fncts[] =
{
	"sin",  sin,
	"cos",  cos,
	"atan", atan,
	"ln",   log,
	"exp",  exp,
	"sqrt", sqrt,
	0, 0
};

/* The symbol table: a chain of `struct symrec'.  */
symrec *sym_table;

/* Put arithmetic functions in table.  */
void init_table (void)
{
	int i;
	for (i = 0; arith_fncts[i].fname != 0; i++)
	{
		symrec *ptr = putsym (arith_fncts[i].fname, FNCT);
		ptr->value.fnctptr = arith_fncts[i].fnct;
	}
}

int main (void)
{
	init_table ();
	return yyparse ();
}
