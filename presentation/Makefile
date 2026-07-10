# Makefile for the Auto Flight presentation
# Uses latexmk + xelatex (required by the metropolis theme / Fira fonts).

MAIN    := main
OUTDIR  := out
PDF     := $(OUTDIR)/$(MAIN).pdf
SLIDES  := $(wildcard slides/*.tex)
DEPS    := $(MAIN).tex preamble.sty $(SLIDES)
LATEXMK := latexmk
FLAGS   := -xelatex -interaction=nonstopmode -halt-on-error -file-line-error \
           -outdir=$(OUTDIR)

.PHONY: all watch clean distclean view

all: $(PDF)

$(PDF): $(DEPS)
	$(LATEXMK) $(FLAGS) $(MAIN).tex

# Rebuild continuously on file changes.
watch:
	$(LATEXMK) $(FLAGS) -pvc $(MAIN).tex

# Open the compiled PDF (macOS).
view: $(PDF)
	open $(PDF)

# Remove build artefacts but keep the PDF.
clean:
	$(LATEXMK) -c -outdir=$(OUTDIR) $(MAIN).tex

# Remove everything, including the PDF and the out/ directory.
distclean:
	$(LATEXMK) -C -outdir=$(OUTDIR) $(MAIN).tex
	rm -rf $(OUTDIR)
