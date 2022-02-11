#!/bin/sh

rm -rf links
mkdir -p links/plugins/platforms

ln -s $PWD/libs/libqxcb.so links/plugins/platforms/libqxcb.so

ln -s $PWD/libs/libfftw3.so.3.6.9 links/libfftw3.so.3
ln -s $PWD/libs/libQt5Gui.so.5.9.9 links/libQt5Gui.so.5
ln -s $PWD/libs/libQt5Core.so.5.9.9 links/libQt5Core.so.5
ln -s $PWD/libs/libicui18n.so.56.1 links/libicui18n.so.56
ln -s $PWD/libs/libicuuc.so.56.1 links/libicuuc.so.56
ln -s $PWD/libs/libicudata.so.56.1 links/libicudata.so.56

ln -s $PWD/libs/libpthread-2.23.so links/libpthread.so.0
ln -s $PWD/libs/libstdc++.so.6.0.21 links/libstdc++.so.6
ln -s $PWD/libs/libgcc_s.so.1 links/libgcc_s.so.1
ln -s $PWD/libs/libc-2.23.so links/libc.so.6
ln -s $PWD/libs/libGL.so.1.2.0 links/libGL.so.1
ln -s $PWD/libs/libz.so.1.2.8 links/libz.so.1
ln -s $PWD/libs/libm-2.23.so links/libm.so.6
ln -s $PWD/libs/libdl-2.23.so links/libdl.so.2
ln -s $PWD/libs/libgthread-2.0.so.0.4800.2 links/libgthread-2.0.so.0
ln -s $PWD/libs/libglib-2.0.so.0.4800.2 links/libglib-2.0.so.0
ln -s $PWD/libs/ld-2.23.so links/ld-linux-x86-64.so.2
ln -s $PWD/libs/libexpat.so.1.6.0 links/libexpat.so.1
ln -s $PWD/libs/libxcb-dri3.so.0.0.0 links/libxcb-dri3.so.0
ln -s $PWD/libs/libxcb-present.so.0.0.0 links/libxcb-present.so.0
ln -s $PWD/libs/libxcb-sync.so.1.0.0 links/libxcb-sync.so.1
ln -s $PWD/libs/libxshmfence.so.1.0.0 links/libxshmfence.so.1
ln -s $PWD/libs/libglapi.so.0.0.0 links/libglapi.so.0
ln -s $PWD/libs/libXext.so.6.4.0 links/libXext.so.6
ln -s $PWD/libs/libXdamage.so.1.1.0 links/libXdamage.so.1
ln -s $PWD/libs/libXfixes.so.3.1.0 links/libXfixes.so.3
ln -s $PWD/libs/libpcre.so.3.13.2 links/libpcre.so.3
ln -s $PWD/libs/libX11-xcb.so.1.0.0 links/libX11-xcb.so.1
ln -s $PWD/libs/libX11.so.6.3.0 links/libX11.so.6
ln -s $PWD/libs/libxcb-glx.so.0.0.0 links/libxcb-glx.so.0
ln -s $PWD/libs/libxcb-dri2.so.0.0.0 links/libxcb-dri2.so.0
ln -s $PWD/libs/libxcb.so.1.1.0 links/libxcb.so.1
ln -s $PWD/libs/libXxf86vm.so.1.0.0 links/libXxf86vm.so.1
ln -s $PWD/libs/libdrm.so.2.4.0 links/libdrm.so.2
ln -s $PWD/libs/libXau.so.6.0.0 links/libXau.so.6
ln -s $PWD/libs/libXdmcp.so.6.0.0 links/libXdmcp.so.6

chmod +x runit.sh
chmod +x CatGT
chmod +x links/ld-linux-x86-64.so.2

