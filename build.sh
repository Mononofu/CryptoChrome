#!/bin/bash

if [ ! -d "firebreath-1.6" ]; then
  # need to download and extract FireBreath firstcd
	git clone git://github.com/firebreath/FireBreath.git -b firebreath-1.6 firebreath-1.6
fi

cd firebreath-1.6/build
make -j 4
cd ../..
cp firebreath-1.6/build/bin/CryptoChrome/npCryptoChrome.so .