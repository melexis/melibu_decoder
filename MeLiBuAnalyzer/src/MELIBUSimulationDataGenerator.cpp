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
    CreateHeader();
    CreateSerialByte( 0xff );
    CreateSerialByte( 0xe4 );
    CreateSerialByte( 0x1c );
    //CreateSerialByte( 0x01 );   // comment this to test missing byte
    //CreateSerialByte( 0x01 ); // uncomment this when adding additional byte to test errors
    CreateSerialByte( 0x13 );
    CreateSerialByte( 0x56 );
    CreateSerialByte( 0x77 );
    /*
     * for( U8 i = 0; i < 8; i++ )
     * {
     *  CreateSerialByte( ( U8 )Random( 0, 225 ) );
     * }
     */
    //CreateReponse( Random( 1, 8 ) );
}

void MELIBUSimulationDataGenerator::CreateHeader() {
    CreateBreakField();
    CreateSerialByte( 0xf0 );
    CreateSerialByte( 0x04 );
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
U8 MELIBUSimulationDataGenerator::SwapEnds2( U8 byte ) {
    U8 t = 0;
    for( int n = 7; n >= 0; n-- ) {
        t |= ( ( byte >> n ) & 1 ) << 7;
        if( n )
            t >>= 1;
    }
    return t;
}

U32 MELIBUSimulationDataGenerator::Random( U32 min, U32 max ) {
    return min + ( rand() % ( max - min + 1 ) );
}
