%{
#include <glib.h>
#include <glib/gprintf.h>

#include "parser.h"

static struct element *append (struct element *e1, struct element *e2)
{
	e1->prev->next = e2;

	e2->prev = e1->prev;
	e2->next = e1;

	e1->prev = e2;

	return e1;
}

extern struct element *root_element;
extern struct element *predefined;

extern int yylex (void);
extern void yyerror (char const *s);

%}

%union {
	/* token */
	struct symbol *sym;
	int num;
	char *str;
	enum mnemonic mnemonic;

	struct element *element;
}

%token PREDEFINED THROUGH
%token IF ELSE WHILE FOR DO
%token MPEG2_BITSDESC
%token H264_BITSDESC H264_FUNC_CAT
%token <sym> SYMBOL
%token <mnemonic> MNEMONIC
%token <num> NUM
%token <str> STRING

%type <element> functiondef predefined defs
%type <element> pred_def pred_defs pred_value pred_values
%type <element> statement statements
%type <element> ifstat whilestat forstat dowhilestat
%type <element> params
%type <element> functioncall
%type <element> exp
%type <element> arrayindexes
%type <element> arrayindex
%type <element> variable
%type <element> bitsdesc
%type <element> numberofbits

%right PLUSASSIGN
%left LOGICALAND LOGICALOR
%left EQUAL NOTEQUAL
%left '>' GE
%left '<' LE
%left LSHIFT RSHIFT
%left '*'
%left OR
%left '|'
%left '&'
%left LOGICALNOT
%left MINUS
%right PLUSPLUS MINUSMINUS

%%

defs:
	predefined					{ root_element = $$ = $1; }
|	functiondef					{ root_element = $$ = $1; }
|	defs predefined					{ $$ = append ($1, $2); }
|	defs functiondef				{ $$ = append ($1, $2); }

predefined:
	PREDEFINED '{' pred_defs '}'		{
	$$ = element_new (element_type_predefined, 1, $3);
	predefined = $3;
}

pred_defs:
	pred_def					{ $$ = $1; }
|	pred_defs pred_def				{ $$ = append ($1, $2); }

pred_def:
	SYMBOL '=' pred_values ';'			{
	$$ = element_new (element_type_pred_def,	2, $1, $3);
	if ($1->predefined)
		printf ("symbol(%s) already initialized\n", $1->str);
	$1->predefined = $3;
}

pred_values:
	pred_value					{ $$ = $1; }
|	pred_values ',' pred_value			{ $$ = append ($1, $3); }

pred_value:
	functioncall					{ $$ = $1; }
|	SYMBOL						{ $$ = element_new (element_type_variable,	1, $1); }
|	NUM						{ $$ = element_new (element_type_num,		1, (void*)(long)$1); }
|	NUM THROUGH NUM					{ $$ = element_new (element_type_num_range, 2, (long)$1, (long)$3); }

functiondef:
	SYMBOL '(' params ')' statement			{ $$ = element_new (element_type_functiondef,	3, $1, $3, $5); }

statement:
	bitsdesc					{ $$ = $1; }
|	functioncall					{ $$ = $1; }
|	ifstat						{ $$ = $1; }
|	whilestat					{ $$ = $1; }
|	dowhilestat					{ $$ = $1; }
|	forstat						{ $$ = $1; }
|	'{' statements '}'				{ $$ = $2; }
|	'{' '}'						{ $$ = element_new (element_type_empty,		0); }
|	exp						{ $$ = $1; }
|	H264_FUNC_CAT '(' functioncall ',' exp ')'		{ $$ = element_new (element_type_h264_func_cat, 2, $3, $5); }

ifstat:
	IF '(' exp ')' statement			{ $$ = element_new (element_type_if,		2, $3, $5); }
|	IF '(' exp ')' statement ELSE statement		{ $$ = element_new (element_type_if_else,	3, $3, $5, $7); }

whilestat:
	WHILE '(' exp ')' statement			{ $$ = element_new (element_type_while,		2, $3, $5); }

dowhilestat:
	DO statement WHILE '(' exp ')'			{ $$ = element_new (element_type_dowhile,	2, $2, $5); }

forstat:
	FOR '(' exp ';' exp ';' exp ')' statement	{ $$ = element_new (element_type_for,		4, $3, $5, $7, $9); }

statements:
	statement					{ $$ = $1; }
|	statements statement				{ $$ = append ($1, $2); }

bitsdesc:
	MPEG2_BITSDESC '(' variable ',' numberofbits ',' MNEMONIC ')'	{ $$ = element_new (element_type_bitsdesc_mpeg2_mnemonic, 3, $3, $5, (void*)(long)$7); }
|	MPEG2_BITSDESC '(' variable ',' numberofbits ',' STRING ')'	{ $$ = element_new (element_type_bitsdesc_mpeg2_string, 3, $3, $5, $7); }
|	H264_BITSDESC '(' variable ',' exp ',' exp ')'	{ $$ = element_new (element_type_bitsdesc_h264, 3, $3, $5, $7); }

numberofbits:
	exp						{ $$ = $1; }
|	NUM '-' NUM					{ $$ = element_new (element_type_num_range,	2, (void*)(long)$1, (void*)(long)$3); }
|	NUM OR NUM					{ $$ = element_new (element_type_num_or,	2, (void*)(long)$1, (void*)(long)$3); }

variable:
	SYMBOL						{ $$ = element_new (element_type_variable,	1, $1); }
|	SYMBOL arrayindexes				{ $$ = element_new (element_type_variable_array,2, $1, $2); }
|	SYMBOL '.' SYMBOL				{ $$ = element_new (element_type_variable_stream,2, $1, $3); }

arrayindexes:
	arrayindex					{ $$ = $1; }
|	arrayindexes arrayindex				{ $$ = append ($1, $2); }

arrayindex:
	'[' exp ']'					{ $$ = element_new (element_type_arrayindex,	1, $2); }

params:
	/* empty */					{ $$ = element_new (element_type_empty,		0); }
|	exp						{ $$ = $1; }
|	params ',' exp					{ $$ = append ($1, $3); }

exp:
	NUM						{ $$ = element_new (element_type_num,		1, (void*)(long)$1); }
|	STRING						{ $$ = element_new (element_type_string,	1, $1); }
|	functioncall					{ $$ = $1; }
|	variable					{ $$ = $1; }
|	exp '=' exp					{ $$ = element_new (element_type_assign,	2, $1, $3); }
|	exp PLUSASSIGN exp				{ $$ = element_new (element_type_plusassign,	2, $1, $3); }
|	exp LOGICALAND exp				{ $$ = element_new (element_type_logicaland,	2, $1, $3); }
|	exp LOGICALOR exp				{ $$ = element_new (element_type_logicalor,	2, $1, $3); }
|	exp '|' exp					{ $$ = element_new (element_type_bitwiseor,	2, $1, $3); }
|	exp '&' exp					{ $$ = element_new (element_type_bitwiseand,	2, $1, $3); }
|	exp EQUAL exp					{ $$ = element_new (element_type_equal,		2, $1, $3); }
|	exp NOTEQUAL exp				{ $$ = element_new (element_type_notequal,	2, $1, $3); }
|	exp '>' exp					{ $$ = element_new (element_type_gt,		2, $1, $3); }
|	exp GE exp					{ $$ = element_new (element_type_ge,		2, $1, $3); }
|	exp '<' exp					{ $$ = element_new (element_type_lt,		2, $1, $3); }
|	exp LE exp					{ $$ = element_new (element_type_le,		2, $1, $3); }
|	exp LSHIFT exp					{ $$ = element_new (element_type_leftshift,	2, $1, $3); }
|	exp RSHIFT exp					{ $$ = element_new (element_type_rightshift,	2, $1, $3); }
|	exp '+' exp					{ $$ = element_new (element_type_plus,		2, $1, $3); }
|	exp '-' exp					{ $$ = element_new (element_type_minus,		2, $1, $3); }
|	exp '/' exp					{ $$ = element_new (element_type_divide,	2, $1, $3); }
|	exp '*' exp					{ $$ = element_new (element_type_multiply,	2, $1, $3); }
|	exp '%' exp					{ $$ = element_new (element_type_modulo,	2, $1, $3); }
|	'!' exp %prec LOGICALNOT			{ $$ = element_new (element_type_logicalnot,	1, $2); }
|	'-' exp %prec MINUS				{ $$ = element_new (element_type_minus,		2, element_new (element_type_num, 1, (void*)(long)0), $2); }
|	PLUSPLUS exp					{ $$ = element_new (element_type_plusplus_pre,	1, $2); }
|	exp PLUSPLUS					{ $$ = element_new (element_type_plusplus_post,	1, $1); }
|	MINUSMINUS exp					{ $$ = element_new (element_type_minusminus_pre,	1, $2); }
|	exp MINUSMINUS					{ $$ = element_new (element_type_minusminus_post,	1, $1); }
|	'(' exp ')'					{ $$ = $2; }

functioncall:
	SYMBOL '(' params ')'				{ $$ = element_new (element_type_functioncall,	2, $1, $3); }


%%

