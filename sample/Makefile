KMYACC = ../src/kmyacc
JAVAC = javac
JAVA = java
PERL = perl

all: calc Calc.class calc.pl calc.html

test: all
	./calc <calc-input >out-c
	$(JAVA) Calc <calc-input >out-java
	perl calc.pl <calc-input >out-perl
	-diff out-c out-java
	-diff out-c out-perl

calc: calc.c
	$(CC) -o calc calc.c

calc.c: calc.y
	$(KMYACC) -m ../src/kmyacc.c.parser calc.y
	mv -f y.tab.c calc.c

Calc.class: Calc.java
	$(JAVAC) Calc.java

Calc.java: Calc.jy ../src/kmyacc.java.parser
	$(KMYACC) -m ../src/kmyacc.java.parser Calc.jy

calc.pl: calc.ply
	$(KMYACC) -m ../src/kmyacc.pl.parser calc.ply

calc.html: calc.jsy
	$(KMYACC) -m ../src/kmyacc.js.parser calc.jsy
	mv -f calc.js calc.html

clean:
	rm -f *~ calc.c Calc.java calc.pl *.class calc calc.html out-*

