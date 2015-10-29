additional_dist_subdirs =

if BUILD_PYTHON_EXTENSION
PYSUBDIR = py_ext
else
additional_dist_subdirs += py_ext
endif

if BUILD_OCTAVE_EXTENSION
OCTSUBDIR = oct_ext
else
additional_dist_subdirs += oct_ext
endif

if BUILD_MATLAB_EXTENSION
MEXSUBDIR = mex_ext
else
additional_dist_subdirs += mex_ext
endif

if BUILD_BIN
BINDIR = src/bin
else
additional_dist_subdirs += src/bin
endif

SUBDIRS = \
	src/lib \
	$(BINDIR) \
	$(OCTSUBDIR) \
        $(PYSUBDIR) \
        $(MEXSUBDIR)
