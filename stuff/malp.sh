#!/bin/sh
cd ../
git archive --format tar --prefix midnighttpd/ master | gzip > midnighttpd-0.0.0.tar.gz
cp midnighttpd-0.0.0.tar.gz pkg/arch
cd pkg/arch
updpkgsums
makepkg -c -f
