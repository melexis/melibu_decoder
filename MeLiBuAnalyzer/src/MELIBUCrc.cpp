#include "MELIBUCrc.h"

MELIBUCrc::MELIBUCrc() : mCRC( 0xFFFF ) {}

MELIBUCrc::~MELIBUCrc() {}

void MELIBUCrc::clear() {
    mCRC = 0xFFFF;
}

U16 MELIBUCrc::add( U8 byte ) {
    for( int i = 0; i < 8; i++ ) {
        U16 firstBit16 = mCRC & 0x8000;
        U16 firstBit8 = ( U16 )byte & 0x80;
        byte <<= 1;
        mCRC <<= 1;

        firstBit8 <<= 8;
        if( ( firstBit16 ^ firstBit8 ) != 0 ) {
            mCRC ^= 0x1021;
        }
    }
    return mCRC;
}

U16 MELIBUCrc::result() {
    return mCRC;
}
