#include "MELIBUAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

#include <iostream>
#include <fstream>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <sys/stat.h>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

// set initial values for variables and add input interfaces
MELIBUAnalyzerSettings::MELIBUAnalyzerSettings()
    : mInputChannel( UNDEFINED_CHANNEL ), mBitRate( 9600 ), mMELIBUVersion( 2.0 ), mACK( false ), mbdfFilepath( "" ) {
    mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mInputChannelInterface->SetTitleAndTooltip( "Serial", "Standard MeLiBu analyzer" );
    mInputChannelInterface->SetChannel( mInputChannel );

    mMbdfFileInterface.reset( new AnalyzerSettingInterfaceText() );
    mMbdfFileInterface->SetTitleAndTooltip( "MBDF filepath", "MBDF filepath" );
    mMbdfFileInterface->SetText( mbdfFilepath );

    mBitRateInterface.reset( new AnalyzerSettingInterfaceInteger() );
    mBitRateInterface->SetTitleAndTooltip( "Bit Rate (Bits/S)",  "Specify the bit rate in bits per second." );
    mBitRateInterface->SetMax( 6000000 );
    mBitRateInterface->SetMin( 1 );
    mBitRateInterface->SetInteger( mBitRate );

    mMELIBUVersionInterface.reset( new AnalyzerSettingInterfaceNumberList() );
    mMELIBUVersionInterface->SetTitleAndTooltip( "MeLiBu Version", "Specify the MeLiBu protocol version 1 or 2." );
    mMELIBUVersionInterface->AddNumber( 1.0, "MeLiBu 1", "MeLiBu Protocol Specification Version 1, normal mode" );
    mMELIBUVersionInterface->AddNumber( 1.1,
                                        "MeLiBu 1 - extended mode",
                                        "MeLiBu Protocol Specification Version 1, extended mode" );
    mMELIBUVersionInterface->AddNumber( 2.0, "MeLiBu 2", "MeLiBu Protocol Specification Version 2" );
    mMELIBUVersionInterface->SetNumber( mMELIBUVersion );

    mMELIBUAckEnabledInterface.reset( new AnalyzerSettingInterfaceBool() );
    mMELIBUAckEnabledInterface->SetTitleAndTooltip( "ACK supported", "Select if ACK is enabled." );
    mMELIBUAckEnabledInterface->SetCheckBoxText( "ACK" );
    mMELIBUAckEnabledInterface->SetValue( mACK );


    AddInterface( mInputChannelInterface.get() );
    AddInterface( mMbdfFileInterface.get() );
    AddInterface( mBitRateInterface.get() );
    AddInterface( mMELIBUVersionInterface.get() );
    AddInterface( mMELIBUAckEnabledInterface.get() );

    // no effect when calling these 4 functions
    // custom export options are not supported in V2
    AddExportOption( 0, "Export as text file" );
    AddExportExtension( 0, "text", "txt" );
    AddExportOption( 1, "Export as csv file" );
    AddExportExtension( 1, "csv", "csv" );

    ClearChannels();
    AddChannel( mInputChannel, "Serial", false );
}

MELIBUAnalyzerSettings::~MELIBUAnalyzerSettings() {}

bool MELIBUAnalyzerSettings::SetSettingsFromInterfaces() {
    mInputChannel = mInputChannelInterface->GetChannel();
    mbdfFilepath = mMbdfFileInterface->GetText();

    std::ofstream MyFile; // this is only for debugging
    MyFile.open( "C:\\Projects\\melibu_decoder\\MeLiBuAnalyzer\\build\\Analyzers\\Release\\filename.txt" );

    fs::path p = fs::current_path(); // returns C:\Program Files\Logic\read_mbdf.py which is wrong; for now path_strings needs to be hardcoded
    std::string path_string = p.string();
    path_string += "\\read_mbdf.py";
    MyFile << path_string << '\n';
    path_string = "C:\\Projects\\melibu_decoder\\MeLiBuAnalyzer\\build\\Analyzers\\Release\\read_mbdf.py";
    struct stat sb;

    // check if mbdf file path exists
    if( stat( mbdfFilepath, &sb ) == 0 ) {
        // run python script and save values to vars
        std::string whole_input = "\"C:\\Program Files (x86)\\Melexis\\Python\\python.exe\" " + path_string + " " +
                                  mbdfFilepath;
        std::string python_output = RunPythonMbdfParser( whole_input.c_str() );
        mMELIBUVersion = stod( python_output );
        MyFile << "from mbdf " << mMELIBUVersion << '\n';
    } else {
        // save values from UI to vars
        mMELIBUVersion = mMELIBUVersionInterface->GetNumber();
        MyFile << "from UI " << mMELIBUVersion << '\n';
    }
    mBitRate = mBitRateInterface->GetInteger();
    mACK = mMELIBUAckEnabledInterface->GetValue();
    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );

    MyFile.close();
    return true;
}

void MELIBUAnalyzerSettings::UpdateInterfacesFromSettings() {
    mInputChannelInterface->SetChannel( mInputChannel );
    mMbdfFileInterface->SetText( mbdfFilepath );
    mBitRateInterface->SetInteger( mBitRate );
    mMELIBUVersionInterface->SetNumber( mMELIBUVersion );
    mMELIBUAckEnabledInterface->SetValue( mACK );
}

void MELIBUAnalyzerSettings::LoadSettings( const char* settings ) {
    SimpleArchive text_archive;
    text_archive.SetString( settings );

    text_archive >> mInputChannel;
    //text_archive >> ( char const** )mbdfFilepath;
    text_archive >> mBitRate;
    text_archive >> mMELIBUVersion;
    text_archive >> mACK;

    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );

    UpdateInterfacesFromSettings();
}

const char* MELIBUAnalyzerSettings::SaveSettings() {
    SimpleArchive text_archive;

    text_archive << mInputChannel;
    //text_archive << ( const char* )mbdfFilepath;
    text_archive << mBitRate;
    text_archive << mMELIBUVersion;
    text_archive << mACK;

    return SetReturnString( text_archive.GetString() );
}

std::string MELIBUAnalyzerSettings::RunPythonMbdfParser( const char* cmd ) {
    std::array < char, 128 > buffer;
    std::string result;
    std::shared_ptr < FILE > pipe( _popen( cmd, "r" ), _pclose );
    if( !pipe ) {
        throw std::runtime_error( "popen() failed!" );
    }
    while( !feof( pipe.get() ) ) {
        if( fgets( buffer.data(), 128, pipe.get() ) != nullptr ) {
            result += buffer.data();
        }
    }
    return result;
}
