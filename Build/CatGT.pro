
TEMPLATE = app
TARGET   = CatGT

win32 {
    DESTDIR = C:/Users/labadmin/Desktop/SGLBUILD/FIXU/CatGT/CatGT-win
#    DESTDIR = C:/Users/labadmin/Desktop/SGLBUILD/FIXU/CatGT/Debug
}

unix {
    DESTDIR = /home/billkarsh/Code/CatGT/CatGT-linux
}

QT += widgets

HEADERS +=              \
    Biquad.h            \
    CGBL.h              \
    Cmdline.h           \
    fftw3.h             \
    IMROTbl.h           \
    IMROTbl_T0.h        \
    IMROTbl_T0base.h    \
    IMROTbl_T1020.h     \
    IMROTbl_T1030.h     \
    IMROTbl_T1100.h     \
    IMROTbl_T1110.h     \
    IMROTbl_T1200.h     \
    IMROTbl_T1300.h     \
    IMROTbl_T21.h       \
    IMROTbl_T24.h       \
    IMROTbl_T3A.h       \
    KVParams.h          \
    Pass1.h             \
    Pass1AP.h           \
    Pass1AP2LF.h        \
    Pass1LF.h           \
    Pass1NI.h           \
    Pass1OB.h           \
    Pass2.h             \
    SGLTypes.h          \
    ShankMap.h          \
    Subset.h            \
    Tool.h              \
    Util.h

SOURCES +=              \
    Biquad.cpp          \
    CGBL.cpp            \
    Cmdline.cpp         \
    IMROTbl.cpp         \
    IMROTbl_T0base.cpp  \
    IMROTbl_T1100.cpp   \
    IMROTbl_T1110.cpp   \
    IMROTbl_T1200.cpp   \
    IMROTbl_T21.cpp     \
    IMROTbl_T24.cpp     \
    IMROTbl_T3A.cpp     \
    KVParams.cpp        \
    main.cpp            \
    Pass1.cpp           \
    Pass1AP.cpp         \
    Pass1AP2LF.cpp      \
    Pass1LF.cpp         \
    Pass1NI.cpp         \
    Pass1OB.cpp         \
    Pass2.cpp           \
    ShankMap.cpp        \
    Subset.cpp          \
    Tool.cpp            \
    Util.cpp            \
    Util_osdep.cpp

win32 {
    QMAKE_LIBDIR    += $${_PRO_FILE_PWD_}
    LIBS            += -lWS2_32 -lUser32 -lwinmm
    LIBS            += -llibfftw3-3
    DEFINES         += _CRT_SECURE_NO_WARNINGS WIN32
}

unix {
    LIBS            += -lfftw3
}

QMAKE_TARGET_COMPANY = Bill Karsh
QMAKE_TARGET_PRODUCT = CatGT
QMAKE_TARGET_DESCRIPTION = Joins, filters, edits, extracts events
QMAKE_TARGET_COPYRIGHT = (c) 2020, Bill Karsh, All rights reserved
VERSION = 3.2

