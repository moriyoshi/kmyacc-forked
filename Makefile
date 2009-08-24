ARCHNAME = $(notdir $(shell /bin/pwd))

all:
	cd src; $(MAKE) all

install:
	cd src; $(MAKE) install

clean:
	rm -f *~
	cd src; $(MAKE) clean
	cd doc; $(MAKE) clean
	cd sample; $(MAKE) clean

dist: clean
	cd ..; tar cvzf $(ARCHNAME).tar.gz --exclude=test $(ARCHNAME)
