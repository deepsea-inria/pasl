##################################################
# RULES FOR COMPILING TOOLS

OCAMLC=ocamlc
OCAMLOPT=ocamlopt
OCAML_INC=shared.ml
OCAML_LIBS=unix.cmxa str.cmxa 
OCAML_LIBSA=graphics.cma unix.cma str.cma 

all: view.out

view: view.out 

view.out: view.ml visualize.ml $(OCAML_INC)
	$(OCAMLOPT) graphics.cmxa $(OCAML_LIBS) -I . $(OCAML_INC) visualize.ml -o $@ $<

doc:
	pandoc README.md -s -o README.pdf


##################################################
# RULES FOR DOCUMENTATION


clean:
	rm -rf doc
	bash -c "rm -f *.{d,o,exe,dbg,opt,log,sta,glo,gch,cmi,cmo,cmx,out}"



