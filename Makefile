all:
	gcc -Wall wrapwebp.c -lpng -ljpeg -lwebp

install:
	python setup.py install

.PHONY: test
test:
	cd test && python test_encode.py ../test.jpg test.webp

clean:
	rm -rf temp build dist *.egg-info
