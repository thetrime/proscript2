#!/bin/bash
for ARCH in armv7 armv7s arm64; do
    PLATFORM=iphoneos
    XCODE_BASE=/Applications/Xcode.app/Contents
    SDKROOT=${XCODE_BASE}/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk
    ETCFLAGS="-fembed-bitcode -arch ${ARCH} --sysroot=${SDKROOT}"
    echo "Compiling for ${ARCH}"
    make distclean >& /dev/null  || true
    ./configure CC="$(xcrun --sdk $PLATFORM -f clang) ${ETCFLAGS}" CPPFLAGS="${ETCFLAGS} -mios-version-min=6.1" --disable-shared --enable-static --prefix=/tmp/gmp-${ARCH} --host=arm-apple-darwin --disable-assembly > /dev/null && make > /dev/null && make install > /dev/null && echo "... OK!"
done

for ARCH in i386 x86_64; do
    PLATFORM=iphonesimulator
    XCODE_BASE=/Applications/Xcode.app/Contents
    SDKROOT=${XCODE_BASE}/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator.sdk
    ETCFLAGS="-fembed-bitcode -arch ${ARCH} --sysroot=${SDKROOT}"
    make distclean >& /dev/null  || true
    ./configure CC="$(xcrun --sdk $PLATFORM -f clang) ${ETCFLAGS}" CPPFLAGS="${ETCFLAGS} -mios-simulator-version-min=6.1" --disable-shared --enable-static --prefix=/tmp/gmp-${ARCH} --host=arm-apple-darwin --disable-assembly > /dev/null && make > /dev/null && make install > /dev/null && echo "... OK!"
done

lipo \
	"/tmp/gmp-armv7/lib/libgmp.a" \
	"/tmp/gmp-armv7s/lib/libgmp.a" \
	"/tmp/gmp-arm64/lib/libgmp.a" \
	"/tmp/gmp-i386/lib/libgmp.a" \
	"/tmp/gmp-x86_64/lib/libgmp.a" \
	-create -output lib/libgmp.a
