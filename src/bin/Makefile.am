AM_CFLAGS = -I$(top_srcdir)/src/lib -L$(top_srcdir)/src/lib
AM_LIBS = -lreadarc -lz

bin_PROGRAMS = \
	dumparc arc2txt arc2dir arcfile

dumparc_SOURCES = \
	dumparc.c output_hex.c

arc2txt_SOURCES = \
	arc2txt.c output_txt.c

arc2dir_SOURCES = \
	arc2dir.c output_dirball.c tarfile.c

arcfile_SOURCES = \
	arcfile.c output_hex.c output_txt.c output_dirball.c tarfile.c
