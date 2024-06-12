#include "MELIBUSimulationDataGenerator.h"
#include "MELIBUAnalyzerSettings.h"

#include <AnalyzerHelpers.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>

#ifdef BUILD_WIN32
#include <Windows.h>
#endif

MELIBUSimulationDataGenerator::MELIBUSimulationDataGenerator() {}

MELIBUSimulationDataGenerator::~MELIBUSimulationDataGenerator() {}

void MELIBUSimulationDataGenerator::Initialize( U32 simulation_sample_rate, MELIBUAnalyzerSettings* settings ) {
    mSimulationSampleRateHz = simulation_sample_rate;
    mSettings = settings;

    mSerialSimulationData.SetChannel( mSettings->mInputChannel );
    mSerialSimulationData.SetSampleRate( simulation_sample_rate );
    mSerialSimulationData.SetInitialBitState( BIT_HIGH );

    ReadSimulationBytes(); // read messages from simulated_data.csv and save to simulation_bytes vector
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

    // iterate through simulaton bytes - vector of vectors of string
    // each vector is one message with more data to generate
    for( std::vector < std::vector < std::string >> ::iterator frame = simulation_bytes.begin();
         frame != simulation_bytes.end();
         ++frame ) {
        CreateBreakField(); // create break field before every message
        for( std::vector < std::string > ::iterator byte = ( *frame ).begin(); byte != ( *frame ).end(); ++byte ) {
            std::regex reg( "^[0-9a-fA-F]{1,2}$" );
            if( std::regex_match( *byte, reg ) )
                CreateSerialByte( stoi( *byte, 0, 16 ) );

            reg = "^-[0-9a-fA-F]{1,2}$"; // this is for creating byte with wrong stop bit
            if( std::regex_match( *byte, reg ) )
                CreateSerialByteWrongStop( stoi( ( *byte ).erase( 0, 1 ), 0, 16 ) );
        }
    }
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

    if(mSettings->mMELIBUVersion == 2.0)
        SwapEnds( byte );

    // inter-byte space.....
    //mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
    //mSerialSimulationData.Advance( samples_per_bit );

    // start bit...
    mSerialSimulationData.TransitionIfNeeded( BIT_LOW );
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

U32 MELIBUSimulationDataGenerator::Random( U32 min, U32 max ) {
    return min + ( rand() % ( max - min + 1 ) );
}

void MELIBUSimulationDataGenerator::CreateSerialByteWrongStop( U8 byte ) {
    U32 samples_per_bit = mSimulationSampleRateHz / mSettings->mBitRate;

    // mChecksum.add( byte );
    if( mSettings->mMELIBUVersion == 2.0 )
        SwapEnds( byte );

    // inter-byte space.....
    // mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
    // mSerialSimulationData.Advance( samples_per_bit );

    // start bit...
    mSerialSimulationData.TransitionIfNeeded( BIT_LOW );
    // mSerialSimulationData.Transition();               // low-going edge for start bit
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
    mSerialSimulationData.TransitionIfNeeded( BIT_LOW );
    mSerialSimulationData.Advance( samples_per_bit );
}

void helper() {}; // this is only for finding path of dll; this function can't be class member function

void MELIBUSimulationDataGenerator::ReadSimulationBytes() {
#ifdef BUILD_WIN32
    char path[ MAX_PATH ];
    HMODULE hm = NULL;
    GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       ( LPCSTR )&helper,
                       &hm );
    GetModuleFileName( hm, path, sizeof( path ) );
    std::string filepath = path;
    for( int i = 0; i < 4; i++ ) {
        std::size_t found = filepath.find_last_of( "/\\" );
        filepath = filepath.substr( 0, found );
    }
    filepath += "\\simulated_data.csv";
#else
    std::string filepath = "simulated_data.csv";
#endif

    std::fstream fin;
    fin.open( filepath, std::ios::in );
    std::string line;
    while( std::getline( fin, line ) ) {
        // one line has more hex numbers in message separated with comma
        std::vector < std::string > bytes_in_line;
        std::string byte;
        std::stringstream ss(line);
        // read element from line and save to byte
        while( std::getline( ss, byte, ',' ) )
            bytes_in_line.push_back( byte ); // add to vector of bytes in message

        simulation_bytes.push_back( bytes_in_line ); // add message vector to vector of messages
    }
}
