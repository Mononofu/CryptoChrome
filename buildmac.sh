#!/bin/bash

if [ ! -d "firebreath-1.6/build" ]; then
  # need to download and extract FireBreath first
  mv firebreath-1.6 tmp
	git clone git://github.com/firebreath/FireBreath.git -b firebreath-1.6 firebreath-1.6
	mv tmp/* firebreath-1.6/
	rmdir tmp
	cd firebreath-1.6
	./prepmake.sh -D CMAKE_OSX_ARCHITECTURES=i386
	cd ..
fi

cd firebreath-1.6/build
make -j 4
cd ../..
cp -r firebreath-1.6/build/projects/CryptoChrome/CryptoChrome.plugin .
if [ ! -f npCryptoChrome.so ];
then
	touch npCryptoChrome.so
fi
