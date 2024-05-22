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
    const char* mPythonExe;
    const char* mMbdfFilepath;
    U32 mBitRate;
    double mMELIBUVersion;
    bool mACK;
    bool mLoadSettingsFromMbdf;
    int mACKValue;

    typedef enum {
        FromUI,
        FromMBDF,
        MBDFPathError,
        PythonError
    } LoadedSettingsFlag;

    std::map < int, bool > node_ack; // when reading config from mbdf each node can have different bool for receiving ack byte
    LoadedSettingsFlag settingsFlag; // tells if settings are loaded from UI or MBDF and if there were some errors when loading settings

 protected:
    std::auto_ptr < AnalyzerSettingInterfaceChannel > mInputChannelInterface;
    std::auto_ptr < AnalyzerSettingInterfaceText > mPythonExeInterface;
    std::auto_ptr < AnalyzerSettingInterfaceText > mMbdfFileInterface;
    std::auto_ptr < AnalyzerSettingInterfaceBool > mMELIBULoadFromMbdfInterface;
    std::auto_ptr < AnalyzerSettingInterfaceNumberList > mMELIBUVersionInterface;
    std::auto_ptr < AnalyzerSettingInterfaceInteger > mBitRateInterface;
    std::auto_ptr < AnalyzerSettingInterfaceBool > mMELIBUAckEnabledInterface;
    std::auto_ptr < AnalyzerSettingInterfaceText > mAckValueInterface;


    std::string getPythonScriptPath();
    std::string RunPythonMbdfParser( std::string& script_path);
    void parsePythonOutput( std::string& output );
};

#endif //MELIBU_ANALYZER_SETTINGS
