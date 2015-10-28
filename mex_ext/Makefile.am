.c.o:
	$(MATLABMEX) -c -I../src/lib/ -o $@ $<

bin_PROGRAMS=readarc listarc

readarc_SOURCES=mex_readarc.c
listarc_SOURCES=mex_listarc.c

readarc_LINK=$(MATLABMEX) -L../src/lib/ -lreadarc -lz -output $@ 
listarc_LINK=$(MATLABMEX) -L../src/lib/ -lreadarc -lz -output $@
