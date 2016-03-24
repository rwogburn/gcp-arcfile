import numpy as np
from datetime import datetime
from arcfile import readarc


"""
load_arc wrapper for readarc

"""


# Use for time conversions.
SECONDS_PER_DAY = 86400.
MJD_EPOCH = datetime.strptime('1858-Nov-17:00:00:00', '%Y-%b-%d:%H:%M:%S')
# Supported time string formats:
#   2009-Apr-29:14:32:01 (Walt format)
#   29-Apr-2009:14:32:01 (gcp "show" command format)
#   090429 14:32:01      (log file format)
TIME_FORMAT = ('%Y-%b-%d:%H:%M:%S', '%d-%b-%Y:%H:%M:%S', '%y%m%d %H:%M:%S')


def load_arc(arcdir, trange=None, reglist=None):
    """
    Read data from gcp arcfiles.

    This function is a wrapper for arcfile.readarc and provides the following 
    features:
      - Function interface is more pythonic.
      - Unpacks time registers into floating-point MJD and seconds.
      - Select only data within the requested time range, if specified.

    Parameters
    ----------
    arcdir : str
        Path to arcfile directory. If trange is not specified, then this 
        argument should be the path to a specific file to read.
    trange : tuple, optional
        Two-element tuple containing start and stop time for data. Times are
        specified in UTC, with the following supported formats:
          1. '2009-Apr-29:14:32:01' (Walt format)
          2. '29-Apr-2009:14:32:01' (gcp "show" command format)
          3. '090429 14:32:01'      (log file format)
        If trange is not specified, then the arcdir argument must point to a 
        specific arcfile to read.
    reglist : list, optional
        List of registers to read. Items in this list can specify an entire
        module (i.e. 'antenna0'), a board (i.e. 'mce0.data'), or a specific 
        register (i.e. 'array.frame.features'). The union of the register list
        is returned, so ['antenna0'] and ['antenna0', 'antenna0.frame'] will 
        give the same results. The 'antenna0.time' board is always appended to
        the list, because it is used to select samples by time. If no register
        list is specified, then all registers are returned.

    Returns
    -------
    data : dict
        Data is stored in numpy arrays nested inside three levels of dict 
        structures -- module, board, and register. To access mce0.data.fb, for
        example, use `data['mce0']['data']['fb']`.

    Examples
    --------
    >>> from arcfile import load_arc

    Read all registers from a specific arcfile.
    
    >>> data = load_arc('arc/20160301_000300.dat.gz')

    Read data from 2016-Mar-01 UTC 00:00:00 to 01:00:00 (all registers).

    >>> t0 = '2016-Mar-01:00:00:00'
    >>> t1 = '2016-Mar-01:01:00:00'
    >>> data = load_arc('arc/', (t0, t1))

    Read only the antenna0 registers.

    >>> data = load_arc('arc/', (t0, t1), ['antenna0'])

    Read only the antenna0 and hk0 registers from a specific file.

    >>> data = load_arc('arc/20160301_000300.dat.gz', 
                         reglist=['antenna0', 'hk0'])

    """

    # If trange or reglist are unspecified, set them to empty strings.
    if trange is None: 
        trange = ('', '')
    if reglist is None: 
        reglist = ''
    else:
        # Make sure that antenna0.time is included in reglist.
        if type(reglist) is str: 
            reglist = [reglist]
        if type(reglist) is tuple: 
            reglist = list(reglist)
        reglist.append('antenna0.time')
    # Load data from arcfiles using readarc.
    data = readarc(arcdir, trange[0], trange[1], reglist)
    # Unpack timestamps.
    data = unpack_utc(data)
    # Select only data in time range.
    if data['antenna0']['time']['utcslow'].size > 0:
        data = select_data(data, trange)
    # Done.
    return data


def unpack_utc(data):
    """Convert utc registers from uint64 to floating-point (mjd, sec)."""
    
    # Loop over register maps.
    for mp in data:
        # Loop over boards.
        for brd in data[mp]:
            # Loop over registers.
            for reg in data[mp][brd]:
                # UTC values are the only thing stored as UINT64.
                if data[mp][brd][reg].dtype == np.uint64:
                    # MJD = UTC mod 2^32
                    mjd = np.mod(data[mp][brd][reg], 2**32).astype(np.float)
                    # sec = (UTC / 2^32) / 1000.
                    sec = (data[mp][brd][reg] / 2**32).astype(np.float) / 1000.
                    # Combine into [2,N] array.
                    data[mp][brd][reg] = np.array([mjd, sec]).squeeze()
    return data


def select_data(data, trange):
    """Select only data that falls within the specified time range."""

    # If either element of trange is an empty string, then just return the data
    # structure.
    if (len(trange[0]) == 0) or (len(trange[1]) == 0):
        return data
    # Determine length of slow and fast registers.
    utcslow = data['antenna0']['time']['utcslow']
    Nslow = utcslow.shape[-1]
    utcfast = data['antenna0']['time']['utcfast']
    Nfast = utcfast.shape[-1]
    sampratio = Nfast / Nslow # should be integer
    # Convert time range into (mjd, sec) pairs.
    t0 = tstring_to_mjd(trange[0])
    t1 = tstring_to_mjd(trange[1])
    # Select slow register data that falls within the requested range.
    utcslow = (utcslow[0,:] - t0[0]) * SECONDS_PER_DAY + (utcslow[1,:] - t0[1])
    t1 = (t1[0] - t0[0]) * SECONDS_PER_DAY + (t1[1] - t0[1])
    islow = np.all([utcslow >= 0., utcslow < t1], axis=0)
    # Select fast registers corresponding to integer number of frames 
    # (is this what we want?)
    ifast = islow.repeat(sampratio)
    # Loop over maps->boards->registers and downselect.
    for mp in data:
        for brd in data[mp]:
            for reg in data[mp][brd]:
                # Get size of register.
                shape = data[mp][brd][reg].shape
                # Ignore registers with no size (i.e. scalars)
                if len(shape) > 0:
                    if shape[-1] == Nslow:
                        # Slow register.
                        data[mp][brd][reg] = data[mp][brd][reg][:, islow]
                    elif shape[-1] == Nfast:
                        # Fast register.
                        data[mp][brd][reg] = data[mp][brd][reg][:, ifast]
                    else:
                        raise ValueError(
                            '{}.{}.{} does not match slow or fast sample'.
                            format(mp, brd, reg))
    # Done.
    return data


def tstring_to_mjd(tstring):
    """Convert time string to floating-point (mjd, sec)."""

    # Try all time string formats.
    for fmt in TIME_FORMAT:
        try:
            # Convert string to datetime struct.
            t = datetime.strptime(tstring, fmt)
            # Extract (mjd, sec).
            mjd = float((t - MJD_EPOCH).days)
            sec = float((t - MJD_EPOCH).seconds)
            return (mjd, sec)
        except ValueError:
            pass
    # None of the time string formats worked. Raise error.
    raise ValueError('Did not recognize time string format')

