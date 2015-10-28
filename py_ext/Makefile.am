all-local:
	CFLAGS="$(CFLAGS) -I../src/lib -fno-strict-aliasing" $(PYTHON) setup.py build

install:
	$(PYTHON) setup.py install

clean:
	$(PYTHON) setup.py clean --all
