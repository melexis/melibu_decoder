#include "MELIBUAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "MELIBUAnalyzer.h"
#include "MELIBUAnalyzerSettings.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

MELIBUAnalyzerResults::MELIBUAnalyzerResults( MELIBUAnalyzer* analyzer, MELIBUAnalyzerSettings* settings )
    :   AnalyzerResults(),
    mSettings( settings ),
    mAnalyzer( analyzer ) {}

MELIBUAnalyzerResults::~MELIBUAnalyzerResults() {}

// bubble text is shown on the bar above bits
void MELIBUAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base ) {
    ClearResultStrings();
    Frame frame = GetFrame( frame_index );

    char number_str[128];
    std::string fault_str;
    std::string str[ 3 ];

    if( frame.mFlags & byteFramingError )
        fault_str += "!FRAME";
    if( frame.mFlags & headerBreakExpected )
        fault_str += "!BREAK";
    if( frame.mFlags & crcMismatch )
        fault_str += "!CRC";
    if( frame.mFlags & receptionFailed )
        fault_str += "!ACK";
    if( fault_str.length() ) {
        fault_str += "!";
        AddResultString( fault_str.c_str() );

        // display the error checksum if and only if the frame was a checksum and the only error was a checksum mismatch.
        if( ( frame.mType == ( U8 )responseCRC2 ) && ( frame.mFlags == crcMismatch ) ) {
            AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
            str[ 0 ] = "!CRC ERR: ";
            str[ 0 ] += number_str;
            str[ 1 ] = "!CRC mismatch: ";
            str[ 1 ] += number_str;
            AddResultString( str[ 0 ].c_str() );
            AddResultString( str[ 1 ].c_str() );
        }
    } else {
        // depending on size of bar different strings are shown
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
        switch( ( MELIBUAnalyzerResults::tMELIBUFrameState )frame.mType ) {
            default:
            case NoFrame:
                str[ 0 ] += "IBS";
                str[ 1 ] += "IB Space";
                str[ 2 ] += "Inter-Byte Space";
                break;
            case headerBreak:
                str[ 0 ] += "BRK";
                str[ 1 ] += "Break";
                str[ 2 ] += "Breakfield";
                break;
            case headerID1:
                AnalyzerHelpers::GetNumberString( frame.mData1 & 0xFF, display_base, 8, number_str, 128 );
                str[ 0 ] += number_str;

                str[ 1 ] += "ID1: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "Header ID 1: ";
                str[ 2 ] += number_str;
                break;
            case headerID2:
                AnalyzerHelpers::GetNumberString( frame.mData1 & 0xFF, display_base, 8, number_str, 128 );
                str[ 0 ] += number_str;

                str[ 1 ] += "ID2: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "Header ID 2: ";
                str[ 2 ] += number_str;
                break;
            case instruction1:
                AnalyzerHelpers::GetNumberString( frame.mData1 & 0xFF, display_base, 8, number_str, 128 );
                str[ 0 ] += number_str;

                str[ 1 ] += "INST1: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "Instruction 1: ";
                str[ 2 ] += number_str;
                break;
            case instruction2:
                AnalyzerHelpers::GetNumberString( frame.mData1 & 0xFF, display_base, 8, number_str, 128 );
                str[ 0 ] += number_str;

                str[ 1 ] += "INST2: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "Instruction 2: ";
                str[ 2 ] += number_str;
                break;
            case responseDataZero:
            case responseData:
                char seq_str[ 128 ];
                AnalyzerHelpers::GetNumberString( frame.mData2 - 1, Decimal, 8, seq_str, 128 );
                str[ 0 ] += number_str;
                str[ 1 ] += "D";
                str[ 1 ] += seq_str;
                str[ 1 ] += ": ";
                str[ 1 ] += number_str;
                str[ 2 ] += "Data ";
                str[ 2 ] += seq_str;
                str[ 2 ] += ": ";
                str[ 2 ] += number_str;
                break;
            case responseCRC1:
                str[ 0 ] += number_str;

                str[ 1 ] += "CRC1: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "CRC 1: ";
                str[ 2 ] += number_str;
                break;
            case responseCRC2:
                str[ 0 ] += number_str;

                str[ 1 ] += "CRC2: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "CRC 2: ";
                str[ 2 ] += number_str;
                break;
            case responseACK:
                str[ 0 ] += number_str;

                str[ 1 ] += "ACK: ";
                str[ 1 ] += number_str;

                str[ 2 ] += "ACK: ";
                str[ 2 ] += number_str;
                break;
        }
        AddResultString( str[ 0 ].c_str() );
        AddResultString( str[ 1 ].c_str() );
        AddResultString( str[ 2 ].c_str() );
    }
}

// txt and csv extension are supported
void MELIBUAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id ) {
    std::ofstream file_stream( file, std::ios::out );

    U64 trigger_sample = this->mAnalyzer->GetTriggerSample();
    U32 sample_rate = this->mAnalyzer->GetSampleRate();

    file_stream << "Type,Time [s],Value,Error" << std::endl;

    U64 num_frames = GetNumFrames();
    for( U32 i = 0; i < num_frames; i++ ) {
        Frame frame = GetFrame( i );
        if( frame.mType != MELIBUAnalyzerResults::NoFrame ) {
            std::string frame_type =
                FrameTypeToString( static_cast < MELIBUAnalyzerResults::tMELIBUFrameState > ( frame.mType ) ).c_str();

            char time_str[ 128 ];
            AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str,
                                            128 );

            char number_str[ 128 ];
            AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

            file_stream << frame_type << "," << time_str << "," << number_str << ",";

            auto flag_strings = FrameFlagsToString( frame.mFlags );
            for( const auto& flag_string : flag_strings ) {
                file_stream << flag_string << " ";
            }
            file_stream << std::endl;
        }
        if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true ) {
            file_stream.close();
            return;
        }
    }

    file_stream.close();
}

void MELIBUAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base ) {
#ifdef SUPPORTS_PROTOCOL_SEARCH
    /*Frame frame = GetFrame( frame_index );
     * ClearTabularText();
     *
     * char number_str[128];
     * AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
     * AddTabularText( number_str );*/
    ClearTabularText();
#endif
}

void MELIBUAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base ) {
    //not supported

}

void MELIBUAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base ) {
    //not supported
}