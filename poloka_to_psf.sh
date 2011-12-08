#!/bin/bash 

git rm autogen.sh ChangeLog config.h.in configure.ac Makefile.am README stamp-h.in 
git rm -r bindings
git rm -r cern_stuff cern_utils
git rm -r cmt
git rm -r dao
git rm -r flat
git rm -r imagemagick_utils
git rm -r m4
git rm -r mc
git rm -r lc
git rm -r simphot
git rm -r src
git rm -r src_base
git rm -r src_utils
git rm -r lapack_stuff
git rm -r telinst 

mkdir tools

# psf 
git rm psf/sparsevect.h 
git mv psf src

# utils
git mv utils/fitlin.cc tools/
git mv utils/makepsf.cc tools/
git rm -r utils



