About
=====
pywebp is a yet another libwebp_ wrapper for Python.

.. _libwebp: http://code.google.com/intl/en/speed/webp/


Install
=======
+ download from github
+ *cd pywebp*
+ python setup.py install


Require
=======
* libwebp
* libjpeg
* libpng


Basic Usage
===========
encode::

    >>> from webp.encode import encode
    >>> encode('input.jpg', 'output.webp')
