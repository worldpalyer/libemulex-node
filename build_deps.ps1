$ErrorActionPreference="Stop"

cd ../boost
 ./bootstrap
 ./b2 -j 24 define=BOOST_USE_WINAPI_VERSION=0x0601 link=shared runtime-link=shared --prefix=..\deps --layout=system address-model=64 release install

cd ../libed2k
rm -r -Force build64
mkdir build64
cd build64
cmake -G "Visual Studio 14 2015 Win64" -DBOOST_ROOT="$pwd/../../deps" -DCMAKE_INSTALL_PREFIX="$pwd/../../deps/" ..
msbuild INSTALL.vcxproj /p:Configuration=Release
cd ../

cd ../openssl
perl Configure VC-WIN64A shared no-asm --prefix="$pwd/../deps"
nmake install


cd ../libemulex
rm -r -Force build64
mkdir build64
cd build64
cmake -G "Visual Studio 14 2015 Win64" -DBOOST_ROOT="$pwd/../../deps" -DCMAKE_INSTALL_PREFIX="$pwd/../../deps" ..
msbuild INSTALL.vcxproj /p:Configuration=Release
cd ../

cp ../deps/lib/*.dll ../deps/bin/

cd ../libemulex-node
node-gyp -DDEPS_DIR="$pwd/../../deps" rebuild