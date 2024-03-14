#ifndef MELIBU_ANALYZER_SETTINGS
#define MELIBU_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>
#include <fstream>
#include <map>
#include <iterator>
#include <iostream>

class MELIBUAnalyzerSettings: public AnalyzerSettings
{
 public:
    MELIBUAnalyzerSettings();
    virtual ~MELIBUAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();
    void UpdateInterfacesFromSettings();

    const char* mbdfFilepath = "";
    const char* dllFolderPath = "";

    Channel mInputChannel = UNDEFINED_CHANNEL;
    double mMELIBUVersion = 1.0;
    U32 mBitRate = 9600;
    bool mACK = false;
    bool settingsFromMBDF = false;
    std::map < int, bool > node_ack;

 protected:
    std::auto_ptr < AnalyzerSettingInterfaceChannel > mInputChannelInterface;
    std::auto_ptr < AnalyzerSettingInterfaceText > mMbdfFileInterface;
    std::auto_ptr < AnalyzerSettingInterfaceInteger > mBitRateInterface;
    std::auto_ptr < AnalyzerSettingInterfaceNumberList > mMELIBUVersionInterface;
    std::auto_ptr < AnalyzerSettingInterfaceBool > mMELIBUAckEnabledInterface;


    std::string getDLLPath();
    std::string RunPythonMbdfParser( std::string script_path, std::string other_args );
    void parsePythonOutput( std::string output );

    std::ofstream debugFile;
};

#endif //MELIBU_ANALYZER_SETTINGS
