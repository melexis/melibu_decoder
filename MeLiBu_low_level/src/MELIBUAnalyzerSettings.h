#ifndef MELIBU_ANALYZER_SETTINGS
#define MELIBU_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>
#include <AnalyzerHelpers.h>
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

    // looks like these three functions are not called from logic application
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();
    void UpdateInterfacesFromSettings();

    // variables to store UI inputs
    Channel mInputChannel;
    U32 mBitRate;
    double mMELIBUVersion;
    bool mACK;
    int mACKValue;

 protected:
    std::auto_ptr < AnalyzerSettingInterfaceChannel > mInputChannelInterface;
    std::auto_ptr < AnalyzerSettingInterfaceNumberList > mMELIBUVersionInterface;
    std::auto_ptr < AnalyzerSettingInterfaceInteger > mBitRateInterface;
    std::auto_ptr < AnalyzerSettingInterfaceBool > mMELIBUAckEnabledInterface;
    std::auto_ptr < AnalyzerSettingInterfaceText > mAckValueInterface;
};

#endif //MELIBU_ANALYZER_SETTINGS
