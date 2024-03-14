#ifndef MELIBU_ANALYZER_H
#define MELIBU_ANALYZER_H

#include <Analyzer.h>
#include "MELIBUAnalyzerResults.h"
#include "MELIBUSimulationDataGenerator.h"
#include "MELIBUCrc.h"
#include <map>

namespace
{
    std::map < MELIBUAnalyzerResults::tMELIBUFrameState, std::string > FrameTypeStringLookup = {
        { MELIBUAnalyzerResults::NoFrame, "no_frame" },
        { MELIBUAnalyzerResults::headerBreak, "header_break" },
        { MELIBUAnalyzerResults::headerID1, "header_ID1" },
        { MELIBUAnalyzerResults::headerID2, "header_ID2" },
        { MELIBUAnalyzerResults::instruction1, "instruction_byte" },
        { MELIBUAnalyzerResults::instruction2, "instruction_byte" },
        { MELIBUAnalyzerResults::responseDataZero, "data" },
        { MELIBUAnalyzerResults::responseData, "data" },
        { MELIBUAnalyzerResults::responseCRC1, "crc1" },
        { MELIBUAnalyzerResults::responseCRC2, "crc2" },
        { MELIBUAnalyzerResults::responseACK, "ack" },
    };

    std::string FrameTypeToString( MELIBUAnalyzerResults::tMELIBUFrameState state ) {
        return FrameTypeStringLookup.at( state );
    }

    std::vector < std::string > FrameFlagsToString( U8 flags ) {
        std::vector < std::string > strings;
        if( flags & MELIBUAnalyzerResults::byteFramingError )
            strings.push_back( "byte_framing_error" );
        if( flags & MELIBUAnalyzerResults::headerBreakExpected )
            strings.push_back( "header_break_expected" );
        if( flags & MELIBUAnalyzerResults::crcMismatch )
            strings.push_back( "crc_mismatch" );
        if( flags & MELIBUAnalyzerResults::receptionFailed )
            strings.push_back( "reception_failed" );

        return strings;
    }
}

class MELIBUAnalyzerSettings;
class ANALYZER_EXPORT MELIBUAnalyzer: public Analyzer2
{
 public:
    MELIBUAnalyzer();
    virtual ~MELIBUAnalyzer();

    virtual void SetupResults();
    virtual void WorkerThread(); // main analyzer function

    virtual U32 GenerateSimulationData( U64 newest_sample_requested,
                                        U32 sample_rate,
                                        SimulationChannelDescriptor** simulation_channels );
    virtual U32 GetMinimumSampleRateHz();

    virtual const char* GetAnalyzerName() const;
    virtual bool NeedsRerun(); // not in use

 protected:
    inline double SamplesPerBit();
    double HalfSamplesPerBit();
    void AdvanceHalfBit();
    void Advance( U16 nBits );
    U8 numberOfDataBytes( double MELIBUVersion, U8 idField1, U8 idField2 );
    void formatValue( std::ostringstream& ss, U64 value, U8 precision );

    U8 GetBreakField( S64& startingSample, S64& endingSample, bool& framingError );
    U8 ByteFrame( S64& startingSample, S64& endingSample, bool& framingError, bool& is_break_field );
    void StartingSampleInBreakField( U32 minBreakFieldBits, S64& startingSample, U32& num_break_bits,
                                     bool& valid_frame );
    bool SendAckByte( double MELIBUVersion, U8 idField1, U8 idField2 );
    void AddToCrc( bool addToCrc, U8 byteOrder, Frame byte, Frame prevByte );
    void AddFrameToTable( Frame f, std::ostringstream& ss );

 protected: //vars
    std::auto_ptr < MELIBUAnalyzerSettings > mSettings;
    std::auto_ptr < MELIBUAnalyzerResults > mResults;
    AnalyzerChannelData* mSerial;

    MELIBUSimulationDataGenerator mSimulationDataGenerator;
    bool mSimulationInitilized;
    MELIBUAnalyzerResults::tMELIBUFrameState mFrameState;
    MELIBUCrc mCRC;


    //Serial analysis vars:
    U32 mSampleRateHz;
    U32 mStartOfStopBitOffset;
    U32 mEndOfStopBitOffset;
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer * __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //MELIBU_ANALYZER_H
