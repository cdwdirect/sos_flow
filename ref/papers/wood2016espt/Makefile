# Errors, mistakes, ommissions in this file will prevent 
# latex from running correctly...

# In the next declaration, "uname" is a standard Unix command which prints
# the name of the current system.
os:= $(shell uname -s)

ifeq ($(os),Linux)
	ACROBAT:=evince
	DETEX:=cat
	SPELL:=ispell -t
endif

ifeq ($(os),Darwin)
	ACROBAT:=open
	DETEX:=detex
	SPELL:=echo 'ispell not instaled' ; ls 
endif

MAIN=bare_conf

TEXFILES=${wildcard *.tex}
LBLFILES=${wildcard LBL/*.tex} 
FIGURES=
STYLE=${wildcard *.sty} 
CLASS=${wildcard *.cls} 
SOURCES=${wildcard *.bib} 
MAKEFILE=Makefile
INFILES=$(TEXFILES) $(FIGURES) $(STYLE) $(CLASS) $(SOURCES) $(MAKEFILE)
LATEX=pdflatex
BIBTEX=bibtex
FONTS=pdffonts


# default: $(MAIN).pdf fonts wordcount
default: $(MAIN).pdf fonts

$(MAIN).pdf: $(INFILES)
	$(LATEX) $(MAIN)
	$(BIBTEX) $(MAIN)
	$(LATEX) $(MAIN)
	$(LATEX) $(MAIN)


fonts: $(MAIN).pdf
	$(FONTS) $<

view: $(MAIN).pdf
	$(ACROBAT) $< &

clean:
	rm -fv *.bbl *.dvi *.ps *.aux *.blg *.log *.pdf *.lof *.lot *.toc *.out

remake: clean default

wordcount: $(TEXFILES)
	$(DETEX) $(TEXFILES) | wc

spell: $(TEXFILES)
	$(SPELL) $(TEXFILES)

