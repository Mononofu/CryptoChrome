#!/bin/bash

if [ ! -d "firebreath-1.6/build" ]; then
  # need to download and extract FireBreath first
  mv firebreath-1.6 tmp
	git clone git://github.com/firebreath/FireBreath.git -b firebreath-1.6 firebreath-1.6
	mv tmp/* firebreath-1.6/
	rmdir tmp
	cd firebreath-1.6
	./prepmake.sh
	cd ..
fi

cd firebreath-1.6/build
make -j 4
cd ../..
cp firebreath-1.6/build/bin/CryptoChrome/npCryptoChrome.so .