pywebp
======

About
-----
pywebp is a yet another libwebp_ wrapper for Python.

.. _libwebp: http://code.google.com/intl/en/speed/webp/


Install
-------

    pip install git+http://github.com/hhatto/pywebp.git#egg=pywebp


Require
-------
* libwebp
* libjpeg
* libpng


Basic Usage
-----------
encode::

    >>> from webp.encode import encode
    >>> encode('input.jpg', 'output.webp')
