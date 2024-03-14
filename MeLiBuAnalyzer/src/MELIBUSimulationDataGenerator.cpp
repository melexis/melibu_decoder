#include "MELIBUSimulationDataGenerator.h"
#include "MELIBUAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

MELIBUSimulationDataGenerator::MELIBUSimulationDataGenerator() {}

MELIBUSimulationDataGenerator::~MELIBUSimulationDataGenerator() {}

void MELIBUSimulationDataGenerator::Initialize( U32 simulation_sample_rate, MELIBUAnalyzerSettings* settings ) {
    mSimulationSampleRateHz = simulation_sample_rate;
    mSettings = settings;

    mSerialSimulationData.SetChannel( mSettings->mInputChannel );
    mSerialSimulationData.SetSampleRate( simulation_sample_rate );
    mSerialSimulationData.SetInitialBitState( BIT_HIGH );
}

U32 MELIBUSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested,
                                                           U32 sample_rate,
                                                           SimulationChannelDescriptor** simulation_channel ) {
    U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested,
                                                                                           sample_rate,
                                                                                           mSimulationSampleRateHz );

    while( mSerialSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested ) {
        CreateFrame();
    }

    *simulation_channel = &mSerialSimulationData;
    return 1;
}

void MELIBUSimulationDataGenerator::CreateFrame() {
    U32 samples_per_bit = mSimulationSampleRateHz / mSettings->mBitRate;
    mSerialSimulationData.Advance( samples_per_bit * Random( 1, 4 ) ); // simulate jitter

    // master to slave: ReqIcStatus
    CreateBreakField();
    CreateSerialByte( 0x10 );
    CreateSerialByte( 0x22 );

    CreateSerialByte( 0x1a );
    CreateSerialByte( 0x5c );
    CreateSerialByte( 0x7e ); // ack byte

    // slave to master: IcStatus
    CreateBreakField();
    CreateSerialByte( 0x12 );
    CreateSerialByte( 0x1c );

    CreateSerialByte( 0x2a );
    CreateSerialByte( 0xc8 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x2c );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x02 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x0d );
    CreateSerialByte( 0x01 );
    CreateSerialByte( 0x0b );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0xb5 );
    CreateSerialByte( 0x29 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );

    CreateSerialByte( 0x1c );
    CreateSerialByte( 0x2a );

    // master to slave: ReqIcStatus; wrong crc byte; ack byte received 7E
    CreateBreakField();
    CreateSerialByte( 0x10 );
    CreateSerialByte( 0x22 );

    CreateSerialByte( 0x1a );
    CreateSerialByte( 0x55 );
    CreateSerialByte( 0x7e ); // ack byte

    // slave to master: IcStatus; wrong crc
    CreateBreakField();
    CreateSerialByte( 0x12 );
    CreateSerialByte( 0x1c );

    CreateSerialByte( 0x2a );
    CreateSerialByte( 0xc8 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x2c );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x02 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x0d );
    CreateSerialByte( 0x01 );
    CreateSerialByte( 0x0b );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0xb5 );
    CreateSerialByte( 0x29 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );

    CreateSerialByte( 0x11 );
    CreateSerialByte( 0x2a );

    // master to slave: ReqIcStatus; expected ack byte but no is received
    CreateBreakField();
    CreateSerialByte( 0x10 );
    CreateSerialByte( 0x22 );

    CreateSerialByte( 0x1a );
    CreateSerialByte( 0x5c );
    //CreateSerialByte( 0x7e ); // ack byte

    // master to slave: ReqIcStatus; received ack byte but value is not 7E
    CreateBreakField();
    CreateSerialByte( 0x10 );
    CreateSerialByte( 0x22 );

    CreateSerialByte( 0x1a );
    CreateSerialByte( 0x5c );
    CreateSerialByte( 0x00 ); // ack byte

    // slave to master: IcStatus; not nough bytes received
    CreateBreakField();
    CreateSerialByte( 0x12 );
    CreateSerialByte( 0x1c );

    CreateSerialByte( 0x2a );
    CreateSerialByte( 0xc8 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x2c );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x02 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x0d );
    CreateSerialByte( 0x01 );
    CreateSerialByte( 0x0b );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0xb5 );
    CreateSerialByte( 0x29 );
    //CreateSerialByte( 0x00 );
    //CreateSerialByte( 0x00 );

    CreateSerialByte( 0x1c );
    CreateSerialByte( 0x2a );

    // master to slave: ReqIcStatus
    CreateBreakField();
    CreateSerialByte( 0x10 );
    CreateSerialByte( 0x22 );

    CreateSerialByte( 0x1a );
    CreateSerialByte( 0x5c );
    CreateSerialByte( 0x7e ); // ack byte

    // slave to master: IcStatus; more bytes than needed
    CreateBreakField();
    CreateSerialByte( 0x12 );
    CreateSerialByte( 0x1c );

    CreateSerialByte( 0x2a );
    CreateSerialByte( 0xc8 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x2c );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x02 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x0d );
    CreateSerialByte( 0x01 );
    CreateSerialByte( 0x0b );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0xb5 );
    CreateSerialByte( 0x29 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );
    CreateSerialByte( 0x00 );

    CreateSerialByte( 0x1c );
    CreateSerialByte( 0x2a );
}

void MELIBUSimulationDataGenerator::CreateHeader() {
    // break field + id fields (1 and 2)
    CreateBreakField();
    CreateSerialByte( 0x12 );
    CreateSerialByte( 0x1c );
}

void MELIBUSimulationDataGenerator::CreateBreakField() {
    // The break field.
    U32 samples_per_bit = ( mSimulationSampleRateHz / mSettings->mBitRate ) + 1; // to fix round off error
    U8 byte1 = 0x0; // 0000 0000
    U8 byte2 = 0xE0;// 1110 0000

    SwapEnds( byte1 );
    SwapEnds( byte2 );

    // inter-byte space.....
    mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
    mSerialSimulationData.Advance( samples_per_bit * 2 );

    // there is no start bit on the break field.
    //// start bit...
    // mSerialSimulationData.Transition( );                 //low-going edge for start bit
    // mSerialSimulationData.Advance( samples_per_bit );	//add start bit time

    U16 mask_byte = byte1; // 0000 0000 0000 0000
    mask_byte |= ( ( U16 )byte2 << 8 ); // 1110 0000 0000 0000
    U16 mask = 0x1 << 7; // 0000 0000 1000 0000
    U32 accumulator = 0;
    for( U32 i = 0; i < 13; i++ ) {
        // mask_byte & mask = 0000 0000 0000 0000
        if( ( mask_byte & mask ) != 0 )
            mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
        else
            mSerialSimulationData.TransitionIfNeeded( BIT_LOW );

        mSerialSimulationData.Advance( samples_per_bit );
        mask = mask >> 1;
    }

    // stop bit...
    mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
    mSerialSimulationData.Advance( samples_per_bit );
}

void MELIBUSimulationDataGenerator::CreateSerialByte( U8 byte ) {
    U32 samples_per_bit = mSimulationSampleRateHz / mSettings->mBitRate;

    //mChecksum.add( byte );
    if(mSettings->mMELIBUVersion == 2.0)
        SwapEnds( byte );

    // inter-byte space.....
    //mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
    //mSerialSimulationData.Advance( samples_per_bit );

    // start bit...
    mSerialSimulationData.TransitionIfNeeded( BIT_LOW );
    //mSerialSimulationData.Transition();               // low-going edge for start bit
    mSerialSimulationData.Advance( samples_per_bit ); // add start bit time

    U8 mask = 0x1 << 7;
    for( U32 i = 0; i < 8; i++ ) {
        if( ( byte & mask ) != 0 )
            mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
        else
            mSerialSimulationData.TransitionIfNeeded( BIT_LOW );

        mSerialSimulationData.Advance( samples_per_bit );
        mask = mask >> 1;
    }

    // stop bit...
    mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
    mSerialSimulationData.Advance( samples_per_bit );
}

void MELIBUSimulationDataGenerator::SwapEnds( U8& byte ) {
    U8 t = 0;
    for( int n = 7; n >= 0; n-- ) {
        t |= ( ( byte >> n ) & 1 ) << 7;
        if( n )
            t >>= 1;
    }
    byte = t;
}

// not in use
U32 MELIBUSimulationDataGenerator::Random( U32 min, U32 max ) {
    return min + ( rand() % ( max - min + 1 ) );
}
