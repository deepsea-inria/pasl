all: pctl.html5 pctl.pdf

%.pdf : %.md
	pandoc $< -s -o $@

%.html5 : %.md
	pandoc $< -s --toc --number-sections --mathjax -c pctl.css -o $@

clean:
	rm -f pctl.pdf pctl.html5
