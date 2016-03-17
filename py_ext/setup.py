from distutils.core import setup, Extension
import numpy as np

module1 = Extension('arcfile',
                    sources = ['pyc_readarc.c'],
                    library_dirs = ['../src/lib'],
                    libraries = ['readarc','z'])

setup (name = 'arcfile',
       version = '0.1',
       description = 'Arc file reader package',
       include_dirs = [np.get_include()],
       ext_modules = [module1],
       ext_package = 'arcfile',
       packages = ['arcfile'],
       package_dir = {'arcfile': '.'}
       )
