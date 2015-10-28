from distutils.core import setup, Extension
import numpy as np

module1 = Extension('readarc',
                    sources = ['pyc_readarc.c'],
                    library_dirs = ['../src'],
                    libraries = ['readarc'])

setup (name = 'readarc',
       version = '0.1',
       description = 'Arc file reader package',
       include_dirs = [np.get_include()],
       ext_modules = [module1])
