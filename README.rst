PyBCY
=====

This is a Python implementation for the cosplay website bcy.net.

Daemon
------

A command line tool *PyBCYDownloader.py* will be installed and made
available on *$PATH*, it serves as a simple HTTPServer that accept POST
requests, extract and download corresponding BCY.net posts, it take
three arguments, example:

::

     --Email YOUREMAIL@example.com --Password=YOURPASSWORD  --SavePath=YOURDOWNLOADFOLDERPATH

or you can pass ``Daemon=True`` when creating BCYDownloadUtils to get a
Daemon process.Note that in the latter case you still have to keep your
process running by yourself

Chrome Extension
----------------

Companion Chrome Extension is packed within the distribution, underlying
powered by *PyBCYDownloader.py* as daemon.

Default copied to ``UserHomeDirectory/PyBCYChromeExtension/``

Refer to https://developer.chrome.com/extensions/getstarted#unpacked for
following steps
