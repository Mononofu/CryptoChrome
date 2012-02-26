#!/bin/bash

cd firebreath-1.6/build
make -j 4
cd ../..
cp firebreath-1.6/build/bin/CryptoChrome/npCryptoChrome.so .