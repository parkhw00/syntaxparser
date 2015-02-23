
%top{
#include <glib.h>
#include <glib/gprintf.h>

#include "parser.h"
#include "parser.yacc.h"

}

%option noyywrap

%%

%{
#define got(r,f,a...)	do{ \
	extern int flex_debug, input_column; \
	input_column += strlen(yytext); \
	if (flex_debug > 0) \
		fprintf(stderr,"got "#r", %s, result:"f"\n", yytext, ##a); \
	return r; \
}while(0)
%}

 /* space or new line */
[ \t]+			{/* space */ extern int input_column; input_column += strlen (yytext); }
\n			{ extern int input_line, input_column; input_line ++; input_column = 0; }

 /* a number */
[1-9][0-9]*		{ yylval.num = atoi (yytext); got (NUM, "%d", yylval.num); }
0			{ yylval.num = 0; got (NUM, "0"); }
0x[0-9a-fA-F]+		{ yylval.num = strtoul (yytext, NULL, 16); got (NUM, "0x%x", yylval.num); }
0X[0-9a-fA-F]+		{ yylval.num = strtoul (yytext, NULL, 16); got (NUM, "0x%x", yylval.num); }
[0124]:[0124]:[0124]	{ yylval.str = g_strdup (yytext); got (STRING, "%s", yylval.str); }

 /* some predefined keywords */
if			{ got (IF, ""); }
else			{ got (ELSE, ""); }
while			{ got (WHILE, ""); }
for			{ got (FOR, ""); }
do			{ got (DO, ""); }
__predefined		{ got (PREDEFINED, ""); }
through			{ got (THROUGH, ""); }
mpeg2_bitsdesc		{ got (MPEG2_BITSDESC, ""); }
h264_bitsdesc		{ got (H264_BITSDESC, ""); }
h264_func_cat		{ got (H264_FUNC_CAT, ""); }

 /* mnemonic */
bslbf			{ yylval.mnemonic = mnemonic_bslbf; got (MNEMONIC, "bslbf"); }
uimsbf			{ yylval.mnemonic = mnemonic_uimsbf; got (MNEMONIC, "uimsbf"); }
simsbf			{ yylval.mnemonic = mnemonic_simsbf; got (MNEMONIC, "simsbf"); }
vlclbf			{ yylval.mnemonic = mnemonic_vlclbf; got (MNEMONIC, "vlclbf"); }

or			{ got (OR, ""); }

 /* symbol */
[a-zA-Z_][a-zA-Z_0-9]*	{ yylval.sym = symbol_get (&symbol_parser, yytext); got (SYMBOL, ""); }

 /* comment */
"/*"			{
	register int c;
	extern int input_line, input_column;

	for ( ; ; )
	{
		while ( (c = input()) != '*' && c != EOF )
		{
			input_column ++;
			if (c == '\n')
			{
				input_line ++;
				input_column = 0;
			}
			/* eat up text of comment */
		}

		if ( c == '*' )
		{
			while ( (c = input()) == '*' )
				input_line ++;
			if ( c == '/' )
				break;    /* found the end */
		}

		if ( c == EOF )
		{
			printf( "EOF in comment" );
			break;
		}
	}
}
.			{
	extern int flex_debug, input_column;

	if (flex_debug > 0)
		fprintf (stderr, "something other %s\n", yytext);
	input_column ++;

	return yytext[0];
}

 /* bit string */
‘[^’]*’			{
	yylval.str = g_strdup (yytext + strlen("‘"));
	yylval.str[strlen (yylval.str)-strlen("’")] = 0;

	got (STRING, "");
};

 /* character string */
“[^”]*”			{
	yylval.str = g_strdup (yytext + strlen("“"));
	yylval.str[strlen (yylval.str)-strlen("”")] = 0;

	got (STRING, "");
};

 /* string */
\"[^"]*\"			{
	yylval.str = g_strdup (yytext+1);
	yylval.str[strlen (yylval.str)-1] = 0;

	got (STRING, "");
};


">="			{ got (GE, ""); }
"<="			{ got (LE, ""); }
"=="			{ got (EQUAL, ""); }
"!="			{ got (NOTEQUAL, ""); }
"&&"			{ got (LOGICALAND, ""); }
"||"			{ got (LOGICALOR, ""); }
"++"			{ got (PLUSPLUS, ""); }
"--"			{ got (MINUSMINUS, ""); }
"+="			{ got (PLUSASSIGN, ""); }
"<<"			{ got (LSHIFT, ""); }
">>"			{ got (RSHIFT, ""); }

%%

