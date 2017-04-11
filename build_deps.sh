set -e

sys=`uname`

# cd ../boost
#  ./bootstrap
#  ./b2 -j 24 define=BOOST_USE_WINAPI_VERSION=0x0601 link=shared runtime-link=shared --prefix=..\deps --layout=system address-model=64 release install

cd ../libed2k
rm -rf build/$sys
mkdir build/$sys
cd build/$sys
cmake ../..
make -j 9 install
cd ../..

# cd ../openssl
# perl Configure VC-WIN64A shared no-asm --prefix="$pwd/../deps"
# nmake install

cd ../libemulex
rm -rf build/$sys
mkdir build/$sys
cd build/$sys
cmake ../..
make -j 9 install
cd ../..

# cp ../deps/lib/*.dll ../deps/bin/

cd ../libemulex-node
node-gyp rebuild