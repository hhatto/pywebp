try:
    from setuptools import Extension, setup
except ImportError:
    from distutils.core import Extension, setup

import os
import sys
import os.path
import platform


library_dirs = ['/usr/local/lib']
include_dirs = ['/usr/local/lib']

setup(name='webp',
    version="0.0.1",
    description="Yet Another Python Webp(libwebp) Wrapper",
    long_description="This module is Alpha Release.",
    author='Hideo Hattori',
    author_email='hhatto.jp@gmail.com',
    #url='http://',
    license='New BSD License',
    platforms='Linux',
    packages=['webp'],
    ext_modules=[
        Extension('webp.encode',
            sources=['webp/encode.c'],
            include_dirs=include_dirs,
            library_dirs=library_dirs,
            libraries=['webp', 'png', 'jpeg'],
            )],

    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: C',
        'Programming Language :: Python',
        'Topic :: Multimedia :: Graphics'],
    keywords="webp jpeg png graphics image",
)
