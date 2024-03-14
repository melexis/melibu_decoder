#ifndef MELIBU_ANALYZER_RESULTS
#define MELIBU_ANALYZER_RESULTS

#include <AnalyzerResults.h>

class MELIBUAnalyzer;
class MELIBUAnalyzerSettings;

class MELIBUAnalyzerResults: public AnalyzerResults
{
 public:
    typedef enum {
        NoFrame = 0,
        // Header
        headerBreak,
        headerID1,
        headerID2,
        // Instruction
        instruction1,
        instruction2,
        // Response
        responseDataZero,
        responseData,
        responseCRC1,
        responseCRC2,
        responseACK

    } tMELIBUFrameState;

    typedef enum {
        Okay = 0x00,
        byteFramingError = 0x01,
        headerBreakExpected = 0x02,
        crcMismatch = 0x04,
        receptionFailed = 0x08
    } tMELIBUFrameFlags;

 public:
    MELIBUAnalyzerResults( MELIBUAnalyzer * analyzer, MELIBUAnalyzerSettings * settings );
    virtual ~MELIBUAnalyzerResults();

    virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
    virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

    virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base );
    virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
    virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

 protected: //functions

 protected: //vars
    MELIBUAnalyzerSettings* mSettings;
    MELIBUAnalyzer* mAnalyzer;

    bool writeToTerminal = false;
};

#endif //MELIBU_ANALYZER_RESULTS
