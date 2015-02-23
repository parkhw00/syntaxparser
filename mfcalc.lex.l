
%top{
#include "calc.h"
#include "mfcalc.yacc.h"

#if 0
#define debug(f,a...)	printf (f,##a)
#else
#define debug(f,a...)	do{}while(0)
#endif

extern symrec * putsym (char const *sym_name, int sym_type);
extern symrec * getsym (char const *sym_name);

}

%option noyywrap

%%

[ \t]+			/* space */
0			{yylval.val = 0; debug ("got 0\n"); return NUM;}
[1-9][0-9]*		{yylval.val = atoi (yytext); debug ("got %f\n", yylval.val); return NUM;}
[1-9][0-9]*"."[0-9]*	{sscanf (yytext, "%lf", &yylval.val); debug ("got %f\n", yylval.val); return NUM;}
<<EOF>>			{debug ("got EOF\n"); return 0;}
[a-zA-Z_][a-zA-Z_0-9]*	{
/* Initially make the buffer long enough
 *               for a 40-character symbol name.  */
static size_t length = 40;
static char *symbuf = 0;
symrec *s;

if (!symbuf)
	symbuf = (char *) malloc (length + 1);

if (length < strlen (yytext))
{
	length = strlen (yytext);
	symbuf = (char *) realloc (symbuf, length + 1);
}
strcpy (symbuf, yytext);

s = getsym (symbuf);
if (s == 0)
	s = putsym (symbuf, VAR);
yylval.tptr = s;
debug ("got val %s\n", symbuf); return s->type;
}
[\n]			{debug ("got newline\n"); return yytext[0];}
.			{debug ("got %c\n", yytext[0]); return yytext[0];}

%%

