#ifndef MELIBU_SIMULATION_DATA_GENERATOR
#define MELIBU_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include <string>
#include <vector>
#include "MELIBUCrc.h"
class MELIBUAnalyzerSettings;

class MELIBUSimulationDataGenerator
{
 public:
    MELIBUSimulationDataGenerator();
    ~MELIBUSimulationDataGenerator();

    void Initialize( U32 simulation_sample_rate, MELIBUAnalyzerSettings* settings );
    U32 GenerateSimulationData( U64 newest_sample_requested,
                                U32 sample_rate,
                                SimulationChannelDescriptor** simulation_channel );

 protected:
    MELIBUAnalyzerSettings* mSettings;
    U32 mSimulationSampleRateHz;

    std::vector < std::vector < std::string >> simulation_bytes;

 protected:
    void CreateFrame();
    void CreateBreakField();
    void CreateSerialByte( U8 byte );
    void SwapEnds( U8& byte );
    U32 Random( U32 min, U32 max );

    void CreateSerialByteWrongStop( U8 byte );

    void ReadSimulationBytes();

    SimulationChannelDescriptor mSerialSimulationData;
    MELIBUCrc mCRC;

};
#endif //MELIBU_SIMULATION_DATA_GENERATOR