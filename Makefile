
apps += parser
objs += parser.o parser_input.o parser_desc.o parser.yacc.o parser.lex.o

CFLAGS += -Wall
CFLAGS += -g0 -ggdb
CFLAGS += -DYYERROR_VERBOSE -DYYDEBUG
CFLAGS += `pkg-config --cflags glib-2.0`
LDFLAGS += `pkg-config --libs glib-2.0` -lm

.SECONDARY:
.DELETE_ON_ERROR:

all: mpeg2 h264 #mpeg4

clean:
	rm -f $(apps) $(objs) $(objs:.o=.d)
	rm -f parser.yacc.[ch] parser.yacc.output parser.yacc.dot parser.lex.c
	for a in mpeg2 h264 mpeg4; do \
		rm -f $$a.funcs $$a.syntax $$a.txt $${a}_desc.txt; \
		rm -f $${a}_dot.pdf $${a}_dot.pdf.tmp.ps $${a}_dot.pdf.tmp.dot; \
		rm -f $${a}_desc.txt; \
	done;

parser: $(objs)
	$(CC) $(LDFLAGS) -o $@ $^

parser.yacc.o: parser.h
parser.lex.o: parser.h parser.yacc.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.d: %.c
	$(CC) $(CFLAGS) -MM -MP -o $@ $<

%.yacc.c %.yacc.h: %.yacc.y
	bison -g -v -o ${<:.y=.c} --defines=${<:.y=.h} $<

%.lex.c: %.lex.l %.yacc.h
	flex -o $@ $<



mpeg2: mpeg2.syntax mpeg2_dot.pdf mpeg2_desc.txt

mpeg2.syntax: mpeg2.funcs mpeg2.predefined mpeg2.some_tricky.sed
	cat mpeg2.predefined > $@
	sed -f mpeg2.some_tricky.sed $< >> $@

mpeg2.funcs: mpeg2.txt mpeg2.awk
	./mpeg2.awk $< > $@

mpeg2.txt: is138182.pdf
	pdftotext -raw $< $@

mpeg2_dot.pdf: parser mpeg2.syntax
	rm -f $@.tmp.ps
	for f in `./parser --list-functions mpeg2.syntax`; do \
		./parser -d $$f mpeg2.syntax | dot -Tps >> $@.tmp.ps; \
	done
	ps2pdf -sPAPERSIZE=a4 $@.tmp.ps $@

mpeg2_desc.txt: parser mpeg2.syntax
	./parser -D mpeg2.syntax > $@



mpeg4: mpeg4.syntax mpeg4_desc.txt

mpeg4.syntax: mpeg4.predefined mpeg4.txt mpeg4.awk
	./mpeg4.awk mpeg4.txt > mpeg4.funcs
	cat mpeg4.predefined > $@
	cat mpeg4.funcs >> $@

mpeg4.txt: MPEG4Part2.pdf
	pdftotext -raw $< $@

mpeg4_desc.txt: parser mpeg4.syntax
	./parser -D mpeg4.syntax > $@



h264: h264_desc.txt

h264_desc.txt: parser h264.syntax
	./parser -D h264.syntax > $@

h264.funcs: h264.txt h264.awk
	./h264.awk $< > $@

h264.syntax: h264.funcs h264.predefined
	cat h264.predefined > $@
	cat h264.funcs >> $@

h264.txt: h.264_standard_document.pdf
	pdftotext -layout $< $@



-include $(objs:.o=.d)

