#include "MELIBUAnalyzer.h"
#include "MELIBUAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <math.h>
#include <bitset>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>

// add only initialization of new variables
MELIBUAnalyzer::MELIBUAnalyzer()
    :   Analyzer2(),
    mSettings( new MELIBUAnalyzerSettings() ),
    mSimulationInitilized( false ),
    mFrameState( MELIBUAnalyzerResults::NoFrame ) {
    // don't change
    SetAnalyzerSettings( mSettings.get() );
    UseFrameV2();
}

MELIBUAnalyzer::~MELIBUAnalyzer() {
    KillThread();
}

void MELIBUAnalyzer::SetupResults() {
    this->mResults.reset( new MELIBUAnalyzerResults( this, this->mSettings.get() ) );
    SetAnalyzerResults( this->mResults.get() );
    this->mResults->AddChannelBubblesWillAppearOn( this->mSettings->mInputChannel );
}

void MELIBUAnalyzer::WorkerThread() {
    this->mFrameState = MELIBUAnalyzerResults::NoFrame; // initialize frame state

    U8 nDataBytes { 0 };                           // number of data in message
    bool byteFramingError { false };
    Frame byteFrame; // byte frame from start to stop bit
    Frame ibsFrame;  // inter byte space frame
    bool is_data_really_break { false }; // for break field found with ByteFrame function
    bool ready_to_save { false };
    bool is_start_of_packet { false };

    U8 id[] { 0, 0 }; // header id values
    U16 crc { 0 };               // crc value read from crc byte fields
    U8 ack_value = this->mSettings->mMELIBUVersion == 2.0 ? this->mSettings->mACKValue : 0x7E;

    ibsFrame.mData1 = 0;
    ibsFrame.mData2 = 0;
    ibsFrame.mFlags = 0;
    ibsFrame.mType = 0;
    this->mSerial = GetAnalyzerChannelData( this->mSettings->mInputChannel );

    if( this->mSerial->GetBitState() == BIT_LOW )
        this->mSerial->AdvanceToNextEdge();

    this->mResults->CancelPacketAndStartNewPacket();

    for( ; ; ) {
        ReadFrame( byteFrame, ibsFrame, is_data_really_break, byteFramingError ); // read byte frame or header break
        AddToCrc( byteFrame );

        if( is_data_really_break ) // break field found insted of byte frame; this is not regular situation
            AddMissingByteFrame( ibsFrame );

        is_start_of_packet = false;
        ready_to_save = false;

        // in each case set mFrameState for next iteration
        switch( this->mFrameState ) {
            case MELIBUAnalyzerResults::NoFrame:
            case MELIBUAnalyzerResults::headerBreak:

                if( byteFrame.mData1 == 0x00 ) {
                    this->mFrameState = MELIBUAnalyzerResults::headerID1;
                    byteFrame.mType = MELIBUAnalyzerResults::headerBreak;
                    is_start_of_packet = true;
                    this->mCRC.clear(); // reset crc
                } else { // reset
                    byteFrame.mFlags |= MELIBUAnalyzerResults::headerBreakExpected;
                    this->mFrameState = MELIBUAnalyzerResults::NoFrame;
                }
                break;

            case MELIBUAnalyzerResults::headerID1:

                this->mFrameState = MELIBUAnalyzerResults::headerID2;
                id[0] = byteFrame.mData1; // save byte value to id1
                break;

            case MELIBUAnalyzerResults::headerID2:

                id[1] = byteFrame.mData1; // save byte value to id2
                nDataBytes = NumberOfDataBytes( id[ 0 ], id[1] );

                if( nDataBytes == 0 )
                    this->mFrameState = MELIBUAnalyzerResults::responseCRC1;
                else
                    this->mFrameState = MELIBUAnalyzerResults::responseDataZero;
                // if instruction bit is set read two bytes for instruction; only possible for MELIBU 2
                if( this->mSettings->mMELIBUVersion == 2.0 && ( id[1] & 0x04 ) != 0 )
                    this->mFrameState = MELIBUAnalyzerResults::instruction1;
                break;

            case MELIBUAnalyzerResults::instruction1:

                this->mFrameState = MELIBUAnalyzerResults::instruction2;
                break;

            case MELIBUAnalyzerResults::instruction2:

                if(nDataBytes == 0)
                    this->mFrameState = MELIBUAnalyzerResults::responseCRC1;
                else
                    this->mFrameState = MELIBUAnalyzerResults::responseDataZero;
                break;

            case MELIBUAnalyzerResults::responseDataZero:

                this->mFrameState = MELIBUAnalyzerResults::responseData;
                nDataBytes--;
                break;

            case MELIBUAnalyzerResults::responseData:

                // if all data bytes are read, read response crc 1 field next
                if(nDataBytes == 1)
                    this->mFrameState = MELIBUAnalyzerResults::responseCRC1;
                nDataBytes--;
                break;

            case MELIBUAnalyzerResults::responseCRC1:

                this->mFrameState = MELIBUAnalyzerResults::responseCRC2;
                CrcFrameValue( crc, byteFrame.mData1, 0 );
                break;

            case MELIBUAnalyzerResults::responseCRC2:
            {
                bool ack = SendAckByte( id[ 0 ], id[ 1 ] );
                this->mFrameState = ack ? MELIBUAnalyzerResults::responseACK : MELIBUAnalyzerResults::NoFrame;
                ready_to_save = !ack; // if we need to read ack byte data is not ready for saving
                CrcFrameValue( crc, byteFrame.mData1, 1 );

                if( this->mCRC.result() != crc ) { // add flag if calculated crc is not the same as read crc
                    byteFrame.mFlags |= MELIBUAnalyzerResults::crcMismatch;
                    this->mResults->AddMarker( mSerial->GetSampleNumber(),
                                               AnalyzerResults::ErrorSquare,
                                               this->mSettings->mInputChannel );
                }
                break;
            }
            case MELIBUAnalyzerResults::responseACK:

                this->mFrameState = MELIBUAnalyzerResults::NoFrame;
                if( byteFrame.mData1 != ack_value ) { // add marker is ack value is not 0x7E (0x7E means that reception of the frame was OK)
                    this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                               AnalyzerResults::ErrorSquare,
                                               this->mSettings->mInputChannel );
                    byteFrame.mFlags |= MELIBUAnalyzerResults::receptionFailed;
                }
                nDataBytes = 0;
                ready_to_save = true;
                break;

        }

        byteFrame.mData2 = NumberOfDataBytes( id[0], id[1] ) - nDataBytes; // number of data in message

        if( is_start_of_packet )
            this->mResults->CommitPacketAndStartNewPacket(); // commit previous packet

        this->mResults->AddFrame( byteFrame ); // add frame to graph view
        AddFrameToTable( byteFrame );      // add frame to tabular view

        if( ready_to_save )
            this->mResults->CommitPacketAndStartNewPacket();

        this->mResults->CommitResults();
        ReportProgress( byteFrame.mEndingSampleInclusive );
    }
}

// not in use
bool MELIBUAnalyzer::NeedsRerun() {
    return false;
}

inline double MELIBUAnalyzer::SamplesPerBit() {
    return ( double )GetSampleRate() / ( double )this->mSettings->mBitRate;
}

double MELIBUAnalyzer::HalfSamplesPerBit() {
    return SamplesPerBit() * 0.5;
}

void MELIBUAnalyzer::AdvanceHalfBit() {
    double numOfSamples = HalfSamplesPerBit();
    this->mSerial->Advance( numOfSamples );
}

void MELIBUAnalyzer::Advance( U16 nBits ) {
    this->mSerial->Advance( nBits * SamplesPerBit() );
}

U8 MELIBUAnalyzer::NumberOfDataBytes( U8 idField1, U8 idField2 ) {
    // in this function function select bit needs to be extracted first
    // number of data bytes in message are calculated based on function select bit and MELIBU version

    if( this->mSettings->mMELIBUVersion >= 2 ) {
        U8 functionSelect = idField2 & 0x02; // 0x02 = 0000 0010; extract value in second bit
        U8 length = idField2 & 0x38;         // 0x38 = 0011 1000; extraxt bits on 3, 4 and 5 places
        length = length >> 3;             // right shift to get 3bit value
        if( functionSelect == 0 ) {
            switch( length ) {
                case 6:
                    return 18;
                case 7:
                    return 24;
                default: // 0,1,2,3,4,5 cases
                    return length * 2;
            }
        } else {
            switch( length ) {
                case 0:
                    return 6;
                case 6:
                    return 84;
                case 7:
                    return 128;
                default: // 1,2,3,4,5 cases
                    return length * 12;
            }
        }
    } else {
        U8 functionSelect = idField1 & 0x01; // 0x01 = 0000 0001
        U8 length { 0 };
        if( functionSelect == 0 ) {
            length = idField2 & 0x1c;     // 0x1c = 0001 1100
            std::bitset < 8 > n( length );   // number of set bits in number
            return n.count() * 6;
        } else {
            length = idField2 & 0xfc;     // 0xfc = 1111 1100
            if( this->mSettings->mMELIBUVersion == 1.0 ) {
                std::bitset < 8 > n( length ); // number of set bits in number
                return n.count() * 6;
            } else {   // MeLiBu 1.1 (extended mode)
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

U8 MELIBUAnalyzer::GetBreakField( S64& startingSample, S64& endingSample, bool& framingError, bool& toggling ) {
    U32 min_break_field_low_bits { 13 }; // for MeLiBu 1
    if( this->mSettings->mMELIBUVersion >= 2.0 )
        min_break_field_low_bits = 11; // for MeLiBu 2

    U32 num_break_bits { 0 };
    bool valid_frame { false };
    StartingSampleInBreakField( min_break_field_low_bits, startingSample, num_break_bits, valid_frame, toggling );

    // sample (add marker) each byte of break field at the middle of bit
    for( U32 i = 0; i < num_break_bits; i++ ) {
        if( i == 0 )
            AdvanceHalfBit();
        else
            Advance( 1 );
        this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                   this->mSerial->GetBitState() == BIT_HIGH ? AnalyzerResults::One : AnalyzerResults::Zero,
                                   this->mSettings->mInputChannel );
    }

    // validate stop bit
    Advance( 1 );
    if( this->mSerial->GetBitState() == BIT_HIGH ) {
        this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                   AnalyzerResults::Stop,
                                   this->mSettings->mInputChannel );
        framingError = false;
    } else {
        this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                   AnalyzerResults::ErrorSquare,
                                   this->mSettings->mInputChannel );
        framingError = true;
    }

    // after all advancing we are now in the middle of stop bit
    SetEndingSampleInStopBit( endingSample );
    this->mSerial->AdvanceToAbsPosition( this->mSerial->GetSampleOfNextEdge() - 1 );

    return ( valid_frame ) ? 0 : 1;
}

U8 MELIBUAnalyzer::ByteFrame( S64& startingSample, S64& endingSample, bool& framingError, bool& is_break_field ) {
    U8 data = 0;
    U8 mask = 0x01; // MELIBU 2: LSB first
    if( this->mSettings->mMELIBUVersion < 2 ) // MELIBU 1: MSB first
        mask = 0x80; // 1000 0000

    framingError = false;
    is_break_field = false;

    // locate start bit
    this->mSerial->AdvanceToNextEdge();
    if( this->mSerial->GetBitState() == BIT_HIGH ) {
        // start bit needs to be low; add error marker and advance to next edge (low)
        AdvanceHalfBit();
        this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                   AnalyzerResults::ErrorDot,
                                   this->mSettings->mInputChannel );
        this->mSerial->AdvanceToNextEdge();
    }
    startingSample = this->mSerial->GetSampleNumber();
    AdvanceHalfBit(); // advance to the middle of start bit
    this->mResults->AddMarker( this->mSerial->GetSampleNumber(), AnalyzerResults::Start,
                               this->mSettings->mInputChannel );

    bool all_break_clear = true;
    // data bits; add marker at the middle of each bit
    for( U32 i = 0; i < 8; i++ ) {
        Advance( 1 );
        if( this->mSerial->GetBitState() == BIT_HIGH ) {
            data |= mask; // add bit to data
            all_break_clear = false; // if at least one bit is high and if there is error frame can't be recognized as break field
        }
        this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                   this->mSerial->GetBitState() == BIT_HIGH ? AnalyzerResults::One : AnalyzerResults::Zero,
                                   this->mSettings->mInputChannel );

        if( this->mSettings->mMELIBUVersion == 2 )
            mask = mask << 1;
        else
            mask = mask >> 1;
    }

    // validate stop bit
    Advance( 1 );
    if( this->mSerial->GetBitState() == BIT_HIGH ) {
        this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                   AnalyzerResults::Stop,
                                   this->mSettings->mInputChannel );
    } else {
        //check if we are really in a break frame
        //10 bits are read: start + 8 data btis + stop; check rest of the bits to see if it is break field
        int additional_bits = this->mSettings->mMELIBUVersion < 2.0 ? 3 : 1;
        all_break_clear &= !( this->mSerial->WouldAdvancingCauseTransition( SamplesPerBit() * additional_bits ) ); // true if all_break_clear was true and no transition to high bit

        // add marker for wrong stop bit
        if( !all_break_clear ) {
            this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                       AnalyzerResults::ErrorSquare,
                                       this->mSettings->mInputChannel );
            framingError = true;
        } else {
            this->mSerial->AdvanceToNextEdge();
            bool high_bit_resent = !this->mSerial->WouldAdvancingCauseTransition( HalfSamplesPerBit() );
            if( high_bit_resent ) {
                endingSample = this->mSerial->GetSampleNumber();
                is_break_field = true;
                return 0x00;
            }
        }

    }

    SetEndingSampleInStopBit( endingSample );
    this->mSerial->AdvanceToAbsPosition( this->mSerial->GetSampleOfNextEdge() - 1 );

    return data;
}

void MELIBUAnalyzer::StartingSampleInBreakField( U32& minBreakFieldBits,
                                                 S64& startingSample,
                                                 U32& num_break_bits,
                                                 bool& valid_frame,
                                                 bool& toggling ) {
    toggling = false;
    for( ;; ) {
        this->mSerial->AdvanceToNextEdge();
        if( this->mSerial->GetBitState() == BIT_HIGH ) {
            // add marker at every rising edge when searching for brak field
            this->mResults->AddMarker( this->mSerial->GetSampleNumber(),
                                       AnalyzerResults::ErrorX,
                                       this->mSettings->mInputChannel );
            toggling = true;
            this->mSerial->AdvanceToNextEdge();
        }
        // do not advance, but only get the sample of next edge and calculate number of low bits
        num_break_bits =
            round(
                ( double )( ( this->mSerial->GetSampleOfNextEdge() - this->mSerial->GetSampleNumber() ) /
                            SamplesPerBit() ) );
        if( num_break_bits >= minBreakFieldBits ) { // if number of low bits are greater than minimum frame is valid
            startingSample = this->mSerial->GetSampleNumber();
            valid_frame = true;
            break;
        }
    }
}

void MELIBUAnalyzer::SetEndingSampleInStopBit( S64& endingSample ) {
    if( this->mSerial->GetSampleOfNextEdge() - this->mSerial->GetSampleNumber() > HalfSamplesPerBit() )
        endingSample = this->mSerial->GetSampleNumber() + HalfSamplesPerBit();
    else
        endingSample = this->mSerial->GetSampleOfNextEdge();
}

bool MELIBUAnalyzer::SendAckByte( U8& idField1, U8& idField2 ) {
    bool ack;

    if( this->mSettings->settingsFlag == MELIBUAnalyzerSettings::FromMBDF ) { // get node setting for ack from map created based on mbdf file
        U8 slaveAdr = 0;
        if( this->mSettings->mMELIBUVersion < 2.0 )
            slaveAdr = ( idField1 & 0xfc ) >> 2;
        else
            slaveAdr = idField1;
        ack = this->mSettings->node_ack[ slaveAdr ];
    } else
        ack = this->mSettings->mACK;

    // ack is sent only when sent message is master to slave
    if( this->mSettings->mMELIBUVersion == 2.0 )
        ack &= ( ( idField2 & 0x01 ) == 0 );
    else
        ack &= ( ( idField1 & 0x02 ) == 0 );
    return ack;
}

void MELIBUAnalyzer::AddToCrc( Frame& byte ) {
    switch(this->mFrameState) {
        case MELIBUAnalyzerResults::headerID1:
        case MELIBUAnalyzerResults::headerID2:
        case MELIBUAnalyzerResults::instruction1:
        case MELIBUAnalyzerResults::instruction2:
        case MELIBUAnalyzerResults::responseDataZero:
        case MELIBUAnalyzerResults::responseData:
            this->mCRC.add( byte.mData1 );
    }
}

void MELIBUAnalyzer::AddFrameToTable( Frame& f ) {
    FrameV2 frame_v2; // frameV2 is used for tabular view of data bytes in UI
    std::ostringstream ss;

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
            frame_v2.AddString( flag_string.c_str(), ss.str().c_str() ); // add column named crc_mismatch with calculated crc field value
        } else
            frame_v2.AddBoolean( flag_string.c_str(), true );            // add column named as flag with field value true
    }
    this->mResults->AddFrameV2( frame_v2,
                                FrameTypeToString(
                                    static_cast < MELIBUAnalyzerResults::tMELIBUFrameState > ( f.mType ) ).c_str(),
                                f.mStartingSampleInclusive,
                                f.mEndingSampleInclusive );
}

void MELIBUAnalyzer::ReadFrame( Frame& byteFrame, Frame& ibsFrame, bool& is_data_really_break,
                                bool& byteFramingError ) {
    is_data_really_break = false;
    ibsFrame.mStartingSampleInclusive = this->mSerial->GetSampleNumber(); // inter byte space is from current sample to starting sample of break or byte field
    // read break or byte field; byteFramingError and is_data_really_break are set in functions
    byteFrame.mFlags = 0;
    if( ( this->mFrameState == MELIBUAnalyzerResults::NoFrame ) ||
        ( this->mFrameState == MELIBUAnalyzerResults::headerBreak ) ) {
        bool toggling = false;
        byteFrame.mData1 = GetBreakField( byteFrame.mStartingSampleInclusive,
                                          byteFrame.mEndingSampleInclusive,
                                          byteFramingError,
                                          toggling );
        byteFrame.mFlags |= ( toggling ? MELIBUAnalyzerResults::headerToggling : 0 );
    } else {
        byteFrame.mData1 = ByteFrame( byteFrame.mStartingSampleInclusive,
                                      byteFrame.mEndingSampleInclusive,
                                      byteFramingError,
                                      is_data_really_break );
    }
    ibsFrame.mEndingSampleInclusive = byteFrame.mStartingSampleInclusive;
    byteFrame.mData2 = 0;
    byteFrame.mFlags |= ( byteFramingError ? MELIBUAnalyzerResults::byteFramingError : 0 );
    byteFrame.mType = mFrameState;
}

void MELIBUAnalyzer::CrcFrameValue( U16& crc, U64 data, U8 frameOrder ) {
    if( frameOrder == 0 ) {
        crc = data; // add first byte to crc: just set crc value
    } else {
        // add second byte to crc (connect with current value); for MELIBU 2 second byte is msb byte, for MELIBU 1 second byte is lsb
        if( this->mSettings->mMELIBUVersion == 2.0 )
            crc |= ( data << 8 );
        else {
            crc = crc << 8;
            crc |= data;
        }
    }
}

void MELIBUAnalyzer::AddMissingByteFrame( Frame& ibsFrame ) {
    this->mFrameState = MELIBUAnalyzerResults::NoFrame;

    // add row to table to mark missig byte
    FrameV2 frame_v2;
    frame_v2.AddBoolean( "missing byte", true );
    // starting sample is not starting sample of header frame but starting sample of inter byte space
    // ending sample is starting sample of header break which is the same as ending sample of inter byte space
    this->mResults->AddFrameV2( frame_v2,
                                "missing_byte",
                                ibsFrame.mStartingSampleInclusive,
                                ibsFrame.mEndingSampleInclusive );                                                              // only adds row to table
}

U32 MELIBUAnalyzer::GenerateSimulationData( U64 minimum_sample_index,
                                            U32 device_sample_rate,
                                            SimulationChannelDescriptor** simulation_channels ) {
    if( this->mSimulationInitilized == false ) {
        this->mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), this->mSettings.get() );
        this->mSimulationInitilized = true;
    }

    return this->mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index,
                                                                  device_sample_rate,
                                                                  simulation_channels );
}

U32 MELIBUAnalyzer::GetMinimumSampleRateHz() {
    return this->mSettings->mBitRate * 4;
}

// next two functions needs to return analyzer name string; this name will be shown in drop down menu when adding analyzers
const char* MELIBUAnalyzer::GetAnalyzerName() const {
    return "MeLiBu_low_level";
}

const char* GetAnalyzerName() {
    return "MeLiBu_low_level";
}

Analyzer* CreateAnalyzer() {
    return new MELIBUAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer ) {
    delete analyzer;
}