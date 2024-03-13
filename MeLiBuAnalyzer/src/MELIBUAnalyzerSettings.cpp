#include "MELIBUAnalyzerSettings.h"
#include <AnalyzerHelpers.h>

#include <iostream>
#include <fstream>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include <Windows.h>

// set initial values for variables and add input interfaces
MELIBUAnalyzerSettings::MELIBUAnalyzerSettings()
    : mInputChannel( UNDEFINED_CHANNEL ), mBitRate( 9600 ), mMELIBUVersion( 1.0 ), mACK( false ), mbdfFilepath( "" ) {
    debugFile.open( "C:\\Projects\\melibu_decoder\\MeLiBuAnalyzer\\build\\Analyzers\\Release\\filename.txt",
                    std::ios_base::app );
    debugFile << "constructor\n";
    debugFile.close();

    mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mInputChannelInterface->SetTitleAndTooltip( "Serial", "Standard MeLiBu analyzer" );
    mInputChannelInterface->SetChannel( mInputChannel );

    mMbdfFileInterface.reset( new AnalyzerSettingInterfaceText() );
    mMbdfFileInterface->SetTitleAndTooltip( "MBDF filepath", "MBDF filepath" );
    mMbdfFileInterface->SetTextType( AnalyzerSettingInterfaceText::FilePath );
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

void helperFcn() {};

bool MELIBUAnalyzerSettings::SetSettingsFromInterfaces() {
    // set vars from UI
    mInputChannel = mInputChannelInterface->GetChannel();
    mbdfFilepath = mMbdfFileInterface->GetText();
    mACK = mMELIBUAckEnabledInterface->GetValue();

    // get dll path and create path to python script
    std::string dll_path = getDLLPath();
    std::size_t found = dll_path.find_last_of( "/\\" );
    std::string python_script_path = dll_path.substr( 0, found ) + "\\read_mbdf.py";

    // check if mbdf file path exists
    struct stat sb;
    if( stat( mbdfFilepath, &sb ) == 0 ) { // run python script and save values to vars
        std::string whole_input = "\"C:\\Program Files (x86)\\Melexis\\Python\\python.exe\" " + python_script_path +
                                  " " + mbdfFilepath;
        std::string python_output = RunPythonMbdfParser( whole_input.c_str() );
        mMELIBUVersion = stod( python_output.substr( 0, python_output.find_first_of( "\n" ) ) );
        mBitRate = stod( python_output.substr( python_output.find_first_of( "\n" ) + 1 ) );
    } else {   // save values from UI to vars
        mBitRate = mBitRateInterface->GetInteger();
        mMELIBUVersion = mMELIBUVersionInterface->GetNumber();
    }
    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );

    debugFile.open( "C:\\Projects\\melibu_decoder\\MeLiBuAnalyzer\\build\\Analyzers\\Release\\filename.txt",
                    std::ios_base::app );
    debugFile << getDLLPath() << '\n';
    debugFile << python_script_path << '\n';
    debugFile.close();

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
    text_archive >> mBitRate;
    text_archive >> mMELIBUVersion;
    text_archive >> mACK;
    mbdfFilepath = text_archive.GetString();

    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );

    UpdateInterfacesFromSettings();
}

const char* MELIBUAnalyzerSettings::SaveSettings() {
    SimpleArchive text_archive;

    text_archive << mInputChannel;
    text_archive << mBitRate;
    text_archive << mMELIBUVersion;
    text_archive << mACK;
    text_archive << mbdfFilepath;

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

std::string MELIBUAnalyzerSettings::getDLLPath() {
    char path[ MAX_PATH ];
    HMODULE hm = NULL;
    GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       ( LPCSTR )&helperFcn,
                       &hm );
    GetModuleFileName( hm, path, sizeof( path ) );

    return path;
}
