
#include "Tool.h"
#include "Util.h"

#if 0
static void compareFiles()
{
#define BUFSIZE 4096

    QFile   fa( "D:/CatGTtest/xxx/catgt_SC035_010820_g0/SC035_010820_g0_tcat.exported.imec0.ap.bin" ),
            fb( "D:/CatGTtest/yyy/catgt_SC035_010820_g0/SC035_010820_g0_tcat.exported.imec0.ap.bin" );

    fa.open( QIODevice::ReadOnly );
    fb.open( QIODevice::ReadOnly );

    quint64 asize = fa.size(),
            bsize = fb.size();

    QVector<char>   abuf( BUFSIZE ),
                    bbuf( BUFSIZE );
    char            *pa = &abuf[0],
                    *pb = &bbuf[0];

    if( asize != bsize ) {
        Log()<<"dif sizes";
        goto close;
    }

    while( bsize ) {

        int n = qMin( (quint64)BUFSIZE, bsize );

        fa.read( pa, n );
        fb.read( pb, n );

        if( memcmp( pa, pb, n ) ) {
            Log()<<"diff";
            goto close;
        }

        bsize -= n;
    }

    Log()<<"same";

close:
    fb.close();
    fa.close();
}
#endif

#if 0
static void compareFiles()
{
    QFile   fa( "C:/SGLDataMS/TEST/catgt_SC024_092319_NP1.0_Midbrain_g0/SC024_092319_NP1.0_Midbrain_g0_imec0/SC024_092319_NP1.0_Midbrain_g0_tcat.imec0.ap.bin" ),
            fb( "C:/SGLDataMS/REFOUT/catgt_SC024_092319_NP1.0_Midbrain_g0/SC024_092319_NP1.0_Midbrain_g0_imec0/SC024_092319_NP1.0_Midbrain_g0_tcat.imec0.ap.bin" );

    fa.open( QIODevice::ReadOnly );
    fb.open( QIODevice::ReadOnly );

    quint64 asize = fa.size(),
            bsize = fb.size();

    uchar   *pa = fa.map( 0, asize ),
            *pb = fb.map( 0, bsize );

    if( asize != bsize ) {
        Log()<<"dif sizes";
        goto close;
    }

    if( memcmp( pa, pb, asize ) )
        Log()<<"diff";
    else
        Log()<<"same";

close:
    fb.unmap( pb );
    fa.unmap( pa );

    fb.close();
    fa.close();
}
#endif


#if 0
static void compareFiles1()
{
#define BUFSIZE 4096

    QFile   fa( "D:\\catgt_test_data_2\\OUT\\catgt_SC024_092319_NP1.0_Midbrain_g0\\SC024_092319_NP1.0_Midbrain_g0_tcat.imec0.ap.bin" ),
            fb( "D:\\catgt_test_data_2\\ORIG\\OUT\\catgt_SC024_092319_NP1.0_Midbrain_g0\\SC024_092319_NP1.0_Midbrain_g0_tcat.imec0.ap.bin" );

    fa.open( QIODevice::ReadOnly );
    fb.open( QIODevice::ReadOnly );

    quint64 asize = fa.size(),
            bsize = fb.size();

    QVector<char>   abuf( BUFSIZE ),
                    bbuf( BUFSIZE );
    char            *pa = &abuf[0],
                    *pb = &bbuf[0];

    if( asize != bsize ) {
        Log()<<"dif sizes";
        goto close;
    }

    while( bsize ) {

        int n = qMin( (quint64)BUFSIZE, bsize );

        fa.read( pa, n );
        fb.read( pb, n );

        if( memcmp( pa, pb, n ) ) {
            Log()<<"diff";
            goto close;
        }

        bsize -= n;
    }

    Log()<<"same";

close:
    fb.close();
    fa.close();
}
static void compareFiles2()
{
#define BUFSIZE 4096

    QFile   fa( "D:\\catgt_test_data_2\\OUT\\catgt_SC024_092319_NP1.0_Midbrain_g0\\SC024_092319_NP1.0_Midbrain_g0_tcat.imec0.lf.bin" ),
            fb( "D:\\catgt_test_data_2\\ORIG\\OUT\\catgt_SC024_092319_NP1.0_Midbrain_g0\\SC024_092319_NP1.0_Midbrain_g0_tcat.imec0.lf.bin" );

    fa.open( QIODevice::ReadOnly );
    fb.open( QIODevice::ReadOnly );

    quint64 asize = fa.size(),
            bsize = fb.size();

    QVector<char>   abuf( BUFSIZE ),
                    bbuf( BUFSIZE );
    char            *pa = &abuf[0],
                    *pb = &bbuf[0];

    if( asize != bsize ) {
        Log()<<"dif sizes";
        goto close;
    }

    while( bsize ) {

        int n = qMin( (quint64)BUFSIZE, bsize );

        fa.read( pa, n );
        fb.read( pb, n );

        if( memcmp( pa, pb, n ) ) {
            Log()<<"diff";
            goto close;
        }

        bsize -= n;
    }

    Log()<<"same";

close:
    fb.close();
    fa.close();
}
#endif
int main( int argc, char *argv[] )
{
    setLogFileName( "CatGT.log" );

//compareFiles1();
//compareFiles2();return 0;

//double qq=getTime();

//compareFiles();
//Log()<<"comp secs "<<getTime()-qq;
//return 0;

    if( !GBL.SetCmdLine( argc, argv ) ) {
        Log();
        return 42;
    }

    if( GBL.pass() == 1 )
        pass1entrypoint();
    else
        supercatentrypoint();

    for( int i = 0, n = GBL.vX.size(); i < n; ++i ) {
        GBL.vX[i]->close();
        delete GBL.vX[i];
    }

//Log()<<"tot secs "<<getTime()-qq;
    Log();
    return 0;
}


