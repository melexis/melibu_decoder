#include "MELIBUAnalyzer.h"
#include "MELIBUAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <math.h>
#include <bitset>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

MELIBUAnalyzer::MELIBUAnalyzer()
    :   Analyzer2(),
    mSettings( new MELIBUAnalyzerSettings() ),
    mSimulationInitilized( false ),
    mFrameState( MELIBUAnalyzerResults::NoFrame ) {
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

MELIBUAnalyzer::~MELIBUAnalyzer() {
    KillThread();
}

void MELIBUAnalyzer::SetupResults() {
    mResults.reset( new MELIBUAnalyzerResults( this, mSettings.get() ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings->mInputChannel );
}

void MELIBUAnalyzer::WorkerThread() {
    std::ofstream outfile;

    outfile.open( "C:\\Projects\\melibu_decoder\\MeLiBuAnalyzer\\build\\Analyzers\\Release\\filename.txt",
                  std::ios_base::app );                                                                                          // append instead of overwrite
    outfile << "Samples per bit " << SamplesPerBit() << "\n";
    outfile << "Sample rate " << GetSampleRate() << "\n";

    mFrameState = MELIBUAnalyzerResults::NoFrame;
    bool showIBS = false; // show inter byte space
    U8 nDataBytes = 0;    // number of data in message
    bool byteFramingError;
    Frame byteFrame; // byte frame from start to stop bit
    Frame prevByteFrame; // previous byte frame; used for calculating crc for MELIBU 2
    Frame ibsFrame;  // inter byte space frame
    bool is_data_really_break; // for break field found with ByteFrame function
    bool ready_to_save = false;
    bool is_start_of_packet = false;

    U8 id1 = 0;  // header id1 value
    U8 id2 = 0;  // header id2 value
    U16 crc = 0; // crc value read from crc byte fields
    bool ack = mSettings->mACK; // read ack byte? read when ack is enabled and when message is from master to slave (R/T bit)
    bool add_to_crc = false;    // add byte value to calculated crc? all types of bytes are added to crc except headerBreak, responseCRC1, responseCRC2 and responseACK
    U8 byte_order = 0;          // 0 or 1 values possible; used when calculating crc; for every two bytes second byte is first added and than first byte is added
    std::ostringstream ss;      // used for formating byte values to string

    ibsFrame.mData1 = 0;
    ibsFrame.mData2 = 0;
    ibsFrame.mFlags = 0;
    ibsFrame.mType = 0;
    mSerial = GetAnalyzerChannelData( mSettings->mInputChannel );

    if( mSerial->GetBitState() == BIT_LOW )
        mSerial->AdvanceToNextEdge();

    mResults->CancelPacketAndStartNewPacket();

    for( ; ; ) {
        ReadFrame( byteFrame, ibsFrame, is_data_really_break, byteFramingError ); // read byte frame or header break

        if( is_data_really_break ) // break field found insted of byte frame; this is not regular situation
            AddMissingByteFrame( showIBS, ibsFrame );
        //if( showIBS )
        //    mResults->AddFrame( ibsFrame );

        is_start_of_packet = false;
        ready_to_save = false;
        add_to_crc = false;

        // in each case set mFrameState for next iteration
        switch( mFrameState ) {
            case MELIBUAnalyzerResults::NoFrame:
                mFrameState = MELIBUAnalyzerResults::headerBreak;
            case MELIBUAnalyzerResults::headerBreak:
                showIBS = true;
                if( byteFrame.mData1 == 0x00 ) {
                    mFrameState = MELIBUAnalyzerResults::headerID1;
                    byteFrame.mType = MELIBUAnalyzerResults::headerBreak;
                    is_start_of_packet = true;
                    mCRC.clear(); // reset crc
                } else { // reset
                    byteFrame.mFlags |= MELIBUAnalyzerResults::headerBreakExpected;
                    mFrameState = MELIBUAnalyzerResults::NoFrame;
                }
                break;
            case MELIBUAnalyzerResults::headerID1:
                mFrameState = MELIBUAnalyzerResults::headerID2;
                id1 = byteFrame.mData1; // save byte value to id1
                add_to_crc = true;
                byte_order = 0; // first byte
                break;
            case MELIBUAnalyzerResults::headerID2:
                id2 = byteFrame.mData1; // save byte value to id2
                nDataBytes = NumberOfDataBytes( mSettings->mMELIBUVersion, id1, id2 );

                if( nDataBytes == 0 )
                    mFrameState = MELIBUAnalyzerResults::responseCRC1;
                else
                    mFrameState = MELIBUAnalyzerResults::responseDataZero;
                // if instruction bit is set read two bytes for instruction; only possible for MELIBU 2
                if( mSettings->mMELIBUVersion == 2.0 && ( id2 & 0x04 ) != 0 )
                    mFrameState = MELIBUAnalyzerResults::instruction1;
                ack = SendAckByte( mSettings->mMELIBUVersion, id1, id2 );
                add_to_crc = true;
                byte_order = 1; // second byte
                break;
            case MELIBUAnalyzerResults::instruction1:
                mFrameState = MELIBUAnalyzerResults::instruction2;
                add_to_crc = true;
                byte_order = 0; // first byte
                break;
            case MELIBUAnalyzerResults::instruction2:
                if(nDataBytes == 0)
                    mFrameState = MELIBUAnalyzerResults::responseCRC1;
                else
                    mFrameState = MELIBUAnalyzerResults::responseDataZero;
                add_to_crc = true;
                byte_order = 1; // second byte
                break;
            case MELIBUAnalyzerResults::responseDataZero:
                mFrameState = MELIBUAnalyzerResults::responseData;
                add_to_crc = true;
                byte_order = 0; // first byte
                nDataBytes--;
                break;
            case MELIBUAnalyzerResults::responseData:
                // if all data bytes are read, read response crc 1 field next
                if(nDataBytes == 1)
                    mFrameState = MELIBUAnalyzerResults::responseCRC1;
                nDataBytes--;
                add_to_crc = true;
                byte_order = ( byte_order + 1 ) % 2; // calculate byte order
                break;
            case MELIBUAnalyzerResults::responseCRC1:
                mFrameState = MELIBUAnalyzerResults::responseCRC2;
                CrcFrameValue( crc, byteFrame.mData1, 0 );
                break;
            case MELIBUAnalyzerResults::responseCRC2:
                mFrameState = ack ? MELIBUAnalyzerResults::responseACK : MELIBUAnalyzerResults::NoFrame;
                showIBS = ack ? true : false;
                ready_to_save = !ack; // if we need to read ack byte data is not ready for saving
                CrcFrameValue( crc, byteFrame.mData1, 1 );

                if( mCRC.result() != crc ) { // add flag if calculated crc is not the same as read crc
                    byteFrame.mFlags |= MELIBUAnalyzerResults::crcMismatch;
                    mResults->AddMarker( mSerial->GetSampleNumber(),
                                         AnalyzerResults::ErrorSquare,
                                         mSettings->mInputChannel );
                }
                break;
            case MELIBUAnalyzerResults::responseACK:
                mFrameState = MELIBUAnalyzerResults::NoFrame;
                if( byteFrame.mData1 != 0x7e ) { // add marker is ack value is not 0x7E (0x7E means that reception of the frame was OK)
                    mResults->AddMarker( mSerial->GetSampleNumber(),
                                         AnalyzerResults::ErrorSquare,
                                         mSettings->mInputChannel );
                    byteFrame.mFlags |= MELIBUAnalyzerResults::receptionFailed;
                }
                showIBS = false;
                nDataBytes = 0;
                ready_to_save = true;
                break;
        }

        AddToCrc( add_to_crc, byte_order, byteFrame, prevByteFrame );

        byteFrame.mData2 = NumberOfDataBytes( mSettings->mMELIBUVersion, id1, id2 ) - nDataBytes;
        prevByteFrame = byteFrame; // save frame to previous

        if( is_start_of_packet )
            mResults->CommitPacketAndStartNewPacket(); // commit previous packet

        mResults->AddFrame( byteFrame );
        AddFrameToTable( byteFrame, ss );

        if( ready_to_save )
            mResults->CommitPacketAndStartNewPacket();

        mResults->CommitResults();
        ReportProgress( byteFrame.mEndingSampleInclusive );
    }
}

// not in use
bool MELIBUAnalyzer::NeedsRerun() {
    return false;
}

inline double MELIBUAnalyzer::SamplesPerBit() {
    return ( double )GetSampleRate() / ( double )mSettings->mBitRate;
}

double MELIBUAnalyzer::HalfSamplesPerBit() {
    return SamplesPerBit() * 0.5;
}

void MELIBUAnalyzer::AdvanceHalfBit() {
    double numOfSamples = HalfSamplesPerBit();
    mSerial->Advance( numOfSamples );
}

void MELIBUAnalyzer::Advance( U16 nBits ) {
    mSerial->Advance( nBits * SamplesPerBit() );
}

U8 MELIBUAnalyzer::NumberOfDataBytes( double MELIBUVersion, U8 idField1, U8 idField2 ) {
    // in this function function select bit needs to be extracted first
    // number of data bytes in message are calculated based on function select bit and MELIBU version
    U8 length = 0;
    U8 functionSelect = 0;
    if( MELIBUVersion >= 2 ) {
        functionSelect = idField2 & 0x02; // 0x02 = 0000 0010; extract value in second bit
        length = idField2 & 0x38;         // 0x38 = 0011 1000; extraxt bits on 3, 4 and 5 places
        length = length >> 3;             // right shift to get 3bit value
        if( functionSelect == 0 )
            return ( length + 1 ) * 2;
        else
            return ( length + 1 ) * 6;
    } else {
        functionSelect = idField1 & 0x01; // 0x01 = 0000 0001
        if( functionSelect == 0 ) {
            length = idField2 & 0x1c;     // 0x1c = 0001 1100
            std::bitset < 8 > n( length );   // number of set bits in number
            return n.count() * 6;
        } else {
            length = idField2 & 0xfc;     // 0xfc = 1111 1100
            if( mSettings->mMELIBUVersion == 1.0 ) {
                std::bitset < 8 > n( length ); // number of set bits in number
                return n.count() * 6;
            } else {
                length = length >> 2;
                return ( length + 1 ) * 2;
            }
        }
    }
}

void MELIBUAnalyzer::FormatValue( std::ostringstream& ss, U64 value, U8 precision ) {
    ss.str( "" ); // empty ss
    ss.clear();   // clear from errors
    ss << "0x" << std::setfill( '0' ) << std::setw( precision ) << std::uppercase << std::hex << value;
}

U8 MELIBUAnalyzer::GetBreakField( S64& startingSample, S64& endingSample, bool& framingError ) {
    U32 min_break_field_low_bits = 11; // in MELIBU 2
    if( mSettings->mMELIBUVersion < 2 )
        min_break_field_low_bits = 13; // in MELIBU 1 minimum break field length is 13 low bits

    U32 num_break_bits = 0;
    bool valid_frame = false;
    StartingSampleInBreakField( min_break_field_low_bits, startingSample, num_break_bits, valid_frame );

    // sample (add marker) each byte of break field at the middle of bit
    for( U32 i = 0; i < num_break_bits; i++ ) {
        if( i == 0 )
            AdvanceHalfBit();
        else
            Advance( 1 );
        mResults->AddMarker( mSerial->GetSampleNumber(),
                             mSerial->GetBitState() == BIT_HIGH ? AnalyzerResults::One : AnalyzerResults::Zero,
                             mSettings->mInputChannel );
    }

    // validate stop bit
    Advance( 1 );
    if( mSerial->GetBitState() == BIT_HIGH ) {
        mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Stop, mSettings->mInputChannel );
        framingError = false;
    } else {
        mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorSquare, mSettings->mInputChannel );
        framingError = true;
    }
    // after all advancing we are now in the middle of stop bit
    // we need to advance more by half of the bit but not to advance to low bit because it is start bit of next byte
    if( mSerial->WouldAdvancingCauseTransition( HalfSamplesPerBit() ) ) {
        mSerial->AdvanceToAbsPosition( mSerial->GetSampleOfNextEdge() - 1 ); // advance to position before next edge
        mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorX, mSettings->mInputChannel ); // add marker because this means that bit is not long enough
    } else
        mSerial->Advance( HalfSamplesPerBit() ); // advance to end of stop bit
    endingSample = mSerial->GetSampleNumber();

    return ( valid_frame ) ? 0 : 1;
}

U8 MELIBUAnalyzer::ByteFrame( S64& startingSample, S64& endingSample, bool& framingError, bool& is_break_field ) {
    U8 data = 0;
    U8 mask = 0x01; // MELIBU 2: LSB first
    if( mSettings->mMELIBUVersion < 2 ) // MELIBU 1: MSB first
        mask = 0x80; // 1000 0000

    framingError = false;
    is_break_field = false;

    // locate start bit
    mSerial->AdvanceToNextEdge();
    if( mSerial->GetBitState() == BIT_HIGH ) {
        // start bit needs to be low; add error marker and advance to next edge (low)
        AdvanceHalfBit();
        mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorDot, mSettings->mInputChannel );
        mSerial->AdvanceToNextEdge();
    }
    startingSample = mSerial->GetSampleNumber();
    AdvanceHalfBit(); // advance to the middle of start bit
    mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Start, mSettings->mInputChannel );

    bool all_break_clear = true;
    // data bits; add marker at the middle of each bit
    for( U32 i = 0; i < 8; i++ ) {
        Advance( 1 );
        if( mSerial->GetBitState() == BIT_HIGH ) {
            data |= mask; // add bit to data
            all_break_clear = false; // if at least one bit is high and if there is error frame can't be recognized as break field
        }
        mResults->AddMarker( mSerial->GetSampleNumber(),
                             mSerial->GetBitState() == BIT_HIGH ? AnalyzerResults::One : AnalyzerResults::Zero,
                             mSettings->mInputChannel );

        if( mSettings->mMELIBUVersion == 2 )
            mask = mask << 1;
        else
            mask = mask >> 1;
    }

    // validate stop bit
    Advance( 1 );
    if( mSerial->GetBitState() == BIT_HIGH ) {
        mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Stop, mSettings->mInputChannel );
    } else {
        //check if we are really in a break frame
        //10 bits are read: start + 8 data btis + stop; check rest of the bits to see if it is break field
        //bool all_break_clear = !( mSerial->WouldAdvancingCauseTransition( SamplesPerBit() * ( mSettings->mMELIBUVersion == 2 ? 1 : 3 ) ) );
        all_break_clear &=
            !( mSerial->WouldAdvancingCauseTransition( SamplesPerBit() * ( mSettings->mMELIBUVersion == 2 ? 1 : 3 ) ) );

        // add marker for wrong stop bit
        if( !all_break_clear ) {
            mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorDot, mSettings->mInputChannel );
            framingError = true;
        } else {
            mSerial->AdvanceToNextEdge();
            bool high_bit_resent = !mSerial->WouldAdvancingCauseTransition( HalfSamplesPerBit() );
            if( high_bit_resent ) {
                endingSample = mSerial->GetSampleNumber();
                is_break_field = true;
                return 0x00;
            }
        }

        //mSerial->AdvanceToNextEdge();
        //bool high_bit_resent = !mSerial->WouldAdvancingCauseTransition( HalfSamplesPerBit() );
        //if( all_break_clear && high_bit_resent )
        //{
        //    endingSample = mSerial->GetSampleNumber();
        //    is_break_field = true;
        //    return 0x00;
        //}

        //mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorSquare, mSettings->mInputChannel );
        //framingError = true;
    }

    // advance to end of stop bit (half samples per bit or to sample before falling edge)
    if( mSerial->GetSampleOfNextEdge() - mSerial->GetSampleNumber() > HalfSamplesPerBit() )
        endingSample = mSerial->GetSampleNumber() + HalfSamplesPerBit();
    else
        endingSample = mSerial->GetSampleOfNextEdge();

    mSerial->AdvanceToAbsPosition( mSerial->GetSampleOfNextEdge() - 1 );
    //if( mSerial->WouldAdvancingCauseTransition( HalfSamplesPerBit() ) )
    //{
    //    mSerial->AdvanceToAbsPosition( mSerial->GetSampleOfNextEdge() - 1 );
    //    mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorX, mSettings->mInputChannel );
    //}
    //else
    //    mSerial->Advance( HalfSamplesPerBit() );

    //endingSample = mSerial->GetSampleNumber() + 1;

    return data;
}

void MELIBUAnalyzer::StartingSampleInBreakField( U32 minBreakFieldBits,
                                                 S64& startingSample,
                                                 U32& num_break_bits,
                                                 bool& valid_frame ) {
    bool toggling = false;  // bool indicating that we added unexpected_toggling row to UI table
    for( ;; ) {
        mSerial->AdvanceToNextEdge();
        if( mSerial->GetBitState() == BIT_HIGH ) {
            // add marker at every rising edge when searching for brak field
            mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorX, mSettings->mInputChannel );
            if( !toggling ) { // if there is more toggling before break field add toggling row just once
                FrameV2 frame_v2;
                frame_v2.AddBoolean( "header_toggling", true );
                mResults->AddFrameV2( frame_v2,
                                      "unexpected_toggling",
                                      mSerial->GetSampleNumber(),
                                      mSerial->GetSampleOfNextEdge() );
                toggling = true;
            }
            mSerial->AdvanceToNextEdge();
        }
        // do not advance, but only get the sample of next edge and calculate number of low bits
        num_break_bits =
            round( ( double )( ( mSerial->GetSampleOfNextEdge() - mSerial->GetSampleNumber() ) / SamplesPerBit() ) );
        // if number of low bits are greater than minimum frame is valid
        if( num_break_bits >= minBreakFieldBits ) {
            startingSample = mSerial->GetSampleNumber();
            valid_frame = true;
            break;
        }
    }
}

bool MELIBUAnalyzer::SendAckByte( double MELIBUVersion, U8 idField1, U8 idField2 ) {
    bool ack;

    if( mSettings->settingsFromMBDF ) { // get node setting for ack from map created based on mbdf file
        U8 slaveAdr = 0;
        if( mSettings->mMELIBUVersion < 2.0 )
            slaveAdr = ( idField1 & 0xfc ) >> 2;
        else
            slaveAdr = idField1;
        ack = mSettings->node_ack[ slaveAdr ];
    } else
        ack = mSettings->mACK;

    // ack is sent only when sent message is master to slave
    if( mSettings->mMELIBUVersion == 2.0 )
        ack &= ( ( idField2 & 0x01 ) == 0 );
    else
        ack &= ( ( idField1 & 0x02 ) == 0 );
    return ack;
}

void MELIBUAnalyzer::AddToCrc( bool addToCrc, U8 byteOrder, Frame byte, Frame prevByte ) {
    if( addToCrc ) { // add byte value to crc if needed
        // for melibu 2 for each two bytes first add second and than first; when byte order is zero nothing will be added
        if( mSettings->mMELIBUVersion == 2.0 && byteOrder == 1 ) {
            mCRC.add( byte.mData1 );
            mCRC.add( prevByte.mData1 );
        }
        // for melibu 1 add bytes to crc in order
        if( mSettings->mMELIBUVersion < 2.0 )
            mCRC.add( byte.mData1 );
    }
}

void MELIBUAnalyzer::AddFrameToTable( Frame f, std::ostringstream& ss ) {
    FrameV2 frame_v2; // frameV2 is used for tabular view of data bytes in UI

    switch( static_cast < MELIBUAnalyzerResults::tMELIBUFrameState > ( f.mType ) ) {
        case MELIBUAnalyzerResults::headerID1:
        case MELIBUAnalyzerResults::headerID2:
        case MELIBUAnalyzerResults::instruction1:
        case MELIBUAnalyzerResults::instruction2:
        case MELIBUAnalyzerResults::responseCRC1:
        case MELIBUAnalyzerResults::responseCRC2:
        case MELIBUAnalyzerResults::responseACK:
            FormatValue( ss, f.mData1, 2 );
            frame_v2.AddString( "data", ss.str().c_str() );
            break;
        // for response data add byte value and index of data in message
        case MELIBUAnalyzerResults::responseDataZero:
        case MELIBUAnalyzerResults::responseData:
            FormatValue( ss, f.mData1, 2 );
            frame_v2.AddString( "data", ss.str().c_str() );
            FormatValue( ss, f.mData2 - 1, 2 );
            frame_v2.AddString( "index", ss.str().c_str() );
            break;
        default:
            break;
    }

    // add flag strings to table
    auto flag_strings = FrameFlagsToString( f.mFlags );
    for( const auto& flag_string : flag_strings ) {
        if( flag_string == "crc_mismatch" ) {
            FormatValue( ss, mCRC.result(), 4 );
            frame_v2.AddString( flag_string.c_str(), ss.str().c_str() );
        } else
            frame_v2.AddBoolean( flag_string.c_str(), true );
    }
    mResults->AddFrameV2( frame_v2,
                          FrameTypeToString(
                              static_cast < MELIBUAnalyzerResults::tMELIBUFrameState > ( f.mType ) ).c_str(),
                          f.mStartingSampleInclusive,
                          f.mEndingSampleInclusive );
}

void MELIBUAnalyzer::ReadFrame( Frame& byteFrame, Frame& ibsFrame, bool& is_data_really_break,
                                bool& byteFramingError ) {
    is_data_really_break = false;
    ibsFrame.mStartingSampleInclusive = mSerial->GetSampleNumber(); // inter byte space is from current sample to starting sample of break or byte field
    // read break or byte field; byteFramingError and is_data_really_break are set in functions
    if( ( mFrameState == MELIBUAnalyzerResults::NoFrame ) || ( mFrameState == MELIBUAnalyzerResults::headerBreak ) ) {
        byteFrame.mData1 = GetBreakField( byteFrame.mStartingSampleInclusive,
                                          byteFrame.mEndingSampleInclusive,
                                          byteFramingError );
    } else {
        byteFrame.mData1 = ByteFrame( byteFrame.mStartingSampleInclusive,
                                      byteFrame.mEndingSampleInclusive,
                                      byteFramingError,
                                      is_data_really_break );
    }
    ibsFrame.mEndingSampleInclusive = byteFrame.mStartingSampleInclusive;
    byteFrame.mData2 = 0;
    byteFrame.mFlags = byteFramingError ? MELIBUAnalyzerResults::byteFramingError : 0;
    byteFrame.mType = mFrameState;
}

void MELIBUAnalyzer::CrcFrameValue( U16& crc, U64 data, U8 frameOrder ) {
    if( frameOrder == 0 ) {
        // add first byte to crc: just set crc value
        crc = data;
    } else {
        // add second byte to crc (connect with current value); for MELIBU 2 second byte is msb byte, for MELIBU 1 second byte is lsb
        if( mSettings->mMELIBUVersion == 2.0 )
            crc |= ( data << 8 );
        else {
            crc = crc << 8;
            crc |= data;
        }
    }
}

void MELIBUAnalyzer::AddMissingByteFrame( bool& showIBS, Frame& ibsFrame ) {
    mFrameState = MELIBUAnalyzerResults::NoFrame;
    showIBS = false; // if byte frame is break field do not add ibs frame to frames

    // add row to table to mark missig byte
    FrameV2 frame_v2;
    frame_v2.AddBoolean( "missing byte", true );
    // starting sample is not starting sample of header frame but starting sample of inter byte space
    // ending sample is starting sample of header break which is the same as ending sample of inter byte space
    mResults->AddFrameV2( frame_v2, "missing_byte", ibsFrame.mStartingSampleInclusive,
                          ibsFrame.mEndingSampleInclusive );
}

U32 MELIBUAnalyzer::GenerateSimulationData( U64 minimum_sample_index,
                                            U32 device_sample_rate,
                                            SimulationChannelDescriptor** simulation_channels ) {
    if( mSimulationInitilized == false ) {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index,
                                                            device_sample_rate,
                                                            simulation_channels );
}

U32 MELIBUAnalyzer::GetMinimumSampleRateHz() {
    return mSettings->mBitRate * 4;
}

const char* MELIBUAnalyzer::GetAnalyzerName() const {
    return "MeLiBu analyzer";
}

const char* GetAnalyzerName() {
    return "MeLiBu analyzer";
}

Analyzer* CreateAnalyzer() {
    return new MELIBUAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer ) {
    delete analyzer;
}