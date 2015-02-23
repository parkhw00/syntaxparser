
%option noyywrap

%%
a        |
ab       |
abc      |
abcd     ECHO; REJECT;
.|\n     /* eat up any unmatched character */

%%

int main (int argc, char **argv)
{
	yylex ();

	return 0;
}
