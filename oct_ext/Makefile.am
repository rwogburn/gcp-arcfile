.c.o:
	$(MKOCTFILE) -c -I../src/lib/ -o $@ $<

bin_PROGRAMS=readarc listarc

readarc_SOURCES=mex_readarc.c
listarc_SOURCES=mex_listarc.c

readarc_LINK=$(MKOCTFILE) --mex -L../src/lib/ -lreadarc -lz -o $@ 
listarc_LINK=$(MKOCTFILE) --mex -L../src/lib/ -lreadarc -lz -o $@
