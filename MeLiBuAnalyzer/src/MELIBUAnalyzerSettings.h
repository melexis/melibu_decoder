#ifndef MELIBU_ANALYZER_SETTINGS
#define MELIBU_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class MELIBUAnalyzerSettings: public AnalyzerSettings
{
 public:
    MELIBUAnalyzerSettings();
    virtual ~MELIBUAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    void UpdateInterfacesFromSettings();
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();

    const char* mbdfFilepath;
    Channel mInputChannel;
    double mMELIBUVersion;
    U32 mBitRate;
    bool mACK;

 protected:
    std::auto_ptr < AnalyzerSettingInterfaceChannel > mInputChannelInterface;
    std::auto_ptr < AnalyzerSettingInterfaceText > mMbdfFileInterface;
    std::auto_ptr < AnalyzerSettingInterfaceInteger > mBitRateInterface;
    std::auto_ptr < AnalyzerSettingInterfaceNumberList > mMELIBUVersionInterface;
    std::auto_ptr < AnalyzerSettingInterfaceBool > mMELIBUAckEnabledInterface;

    std::string RunPythonMbdfParser( const char* cmd );
};

#endif //MELIBU_ANALYZER_SETTINGS
