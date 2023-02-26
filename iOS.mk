NICE_PRINT = "\033[1;32m Building $(1)\033[0m\n"

XCODE_TOOLCHAIN = $(shell xcode-select --print-path)/Toolchains/XcodeDefault.xctoolchain
IOS_PLATFORM ?= iphoneos

IOS_SDK = $(shell xcrun -sdk ${IOS_PLATFORM} -show-sdk-path)

export PATH := ${CURDIR}/build/macOS/x86_64/bin:${PATH}

export PREFIX = ${CURDIR}/build/iOS/${ARCH}
LIBDIR = ${PREFIX}/lib

export CXX = ${XCODE_TOOLCHAIN}/usr/bin/clang++
export CC = ${XCODE_TOOLCHAIN}/usr/bin/clang
CROSSFLAGS = -arch ${ARCH} -isysroot ${IOS_SDK} -miphoneos-version-min=8.0 -maes
export CFLAGS = ${CROSSFLAGS} -O3 -fembed-bitcode -fvisibility=hidden
export CPPFLAGS = ${CROSSFLAGS} -I${IOS_SDK}/usr/include -I${PREFIX}/include
export CXXFLAGS = ${CFLAGS} -std=c++14 -stdlib=libc++ -fno-aligned-allocation
export LDFLAGS = ${CROSSFLAGS} -e _main -stdlib=libc++ -L${LIBDIR} -L${IOS_SDK}/usr/lib
HOST = arm-apple-darwin
LIBTOOLFLAGS = -arch_only ${ARCH}

# Build separate architectures
all: config_file
	@${MAKE} -f iOS.mk ios_arch ARCH=x86_64 IOS_PLATFORM=iphonesimulator
	@${MAKE} -f iOS.mk ios_arch ARCH=arm64 IOS_PLATFORM=iphoneos >/dev/null
	#@${MAKE} -f iOS.mk ios_arch ARCH=armv7 IOS_PLATFORM=iphoneos >/dev/null
	#@${MAKE} -f iOS.mk ios_arch ARCH=i386 IOS_PLATFORM=iphonesimulator >/dev/null

config_file:
	autoreconf

ios_arch: status | ${LIBDIR}/libsqlite3.a

status:
	@printf $(call NICE_PRINT,$(ARCH)) 1>&2;

${LIBDIR}/libsqlite3.a:
	@printf $(call NICE_PRINT,$@) 1>&2;
	env CXXFLAGS="${CXXFLAGS} -DSQLITE_ENABLE_GEOPOLY -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_FTS5" \
	./configure --host=${HOST} --prefix=${PREFIX} --disable-shared --with-chacha20 --with-sqlcipher && \
	${MAKE} clean && \
	${MAKE} -j8 install-libLTLIBRARIES install-includemcHEADERS
