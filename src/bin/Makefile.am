AM_CFLAGS = -I$(top_srcdir)/src/lib -L$(top_srcdir)/src/lib

bin_PROGRAMS = \
	dumparc arc2txt arc2dir arcfile

dumparc_SOURCES = \
	dumparc.c output_hex.c
dumparc_LDADD = -lreadarc -lz

arc2txt_SOURCES = \
	arc2txt.c output_txt.c
arc2txt_LDADD = -lreadarc -lz

arc2dir_SOURCES = \
	arc2dir.c output_dirball.c tarfile.c
arc2dir_LDADD = -lreadarc -lz

arcfile_SOURCES = \
	arcfile.c output_hex.c output_txt.c output_dirball.c tarfile.c
arcfile_LDADD = -lreadarc -lz


