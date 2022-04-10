#!/bin/bash

VERSION=$1
VERSION=${VERSION:-'0.5.0'}
BUILDDIR="vnetpc-${VERSION}"
TARFILE="vnetpc_${VERSION}.orig.tar.gz"

#Preparation
echo "Preparing ${BUILDDIR}"
cp -ra vnetpc ${BUILDDIR}
tar czf ${TARFILE} ${BUILDDIR}

#build
echo "Building ${BUILDDIR}"
cd ${BUILDDIR}
debuild -us -uc
cd ..

#clean up
echo "Cleaning up"
rm -rf ${BUILDDIR}
rm -f ${TARFILE}
