"""
arcfile

Read data from gcp arcfiles.

Imports
-------
arcfile
    C extension containing low-level readarc function
load_arc
    Wrapper for readarc that does some simple unpacking and data selection for 
    the requested time range.

"""

from load_arc import load_arc
