#include "MELIBUAnalyzerSettings.h"
#include <AnalyzerHelpers.h>
#include <Analyzer.h>

#include <iostream>
#include <fstream>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include <Windows.h>

#include <regex>

// set initial values for variables and add input interfaces
MELIBUAnalyzerSettings::MELIBUAnalyzerSettings()
    : mInputChannel( UNDEFINED_CHANNEL ),
    mBitRate{ 1000000 },
    mMELIBUVersion( 1.0 ),
    mACK( false ),
    mMbdfFilepath( "" ),
    dllFolderPath( "" ),
    settingsFromMBDF( false ),
    mLoadSettingsFromMbdf( false ) {

    mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mInputChannelInterface->SetTitleAndTooltip( "Serial", "Standard MeLiBu analyzer" );
    mInputChannelInterface->SetChannel( mInputChannel );

    mMbdfFileInterface.reset( new AnalyzerSettingInterfaceText() );
    mMbdfFileInterface->SetTitleAndTooltip( "MBDF filepath", "MBDF filepath" );
    mMbdfFileInterface->SetTextType( AnalyzerSettingInterfaceText::FilePath );
    mMbdfFileInterface->SetText( mMbdfFilepath );

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

    mMELIBULoadFromMbdfInterface.reset( new AnalyzerSettingInterfaceBool() );
    mMELIBULoadFromMbdfInterface->SetTitleAndTooltip( "Settings from MBDF",
                                                      "Select if you want to load settings from entered MBDF file." );
    mMELIBULoadFromMbdfInterface->SetCheckBoxText( "Load settings from MBDF" );
    mMELIBULoadFromMbdfInterface->SetValue( mLoadSettingsFromMbdf );

    AddInterface( mInputChannelInterface.get() );
    AddInterface( mMbdfFileInterface.get() );
    AddInterface( mBitRateInterface.get() );
    AddInterface( mMELIBUVersionInterface.get() );
    AddInterface( mMELIBUAckEnabledInterface.get() );
    AddInterface( mMELIBULoadFromMbdfInterface.get() );

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
    // set vars from UI
    mInputChannel = mInputChannelInterface->GetChannel();
    mMbdfFilepath = mMbdfFileInterface->GetText();
    mACK = mMELIBUAckEnabledInterface->GetValue();
    mLoadSettingsFromMbdf = mMELIBULoadFromMbdfInterface->GetValue();
    mBitRate = mBitRateInterface->GetInteger();
    mMELIBUVersion = mMELIBUVersionInterface->GetNumber();

    settingsFromMBDF = false; // reset
    node_ack.clear(); // empty (node,ack) map

    // get dll path and create path to python script
    std::string dll_path = getDLLPath();
    std::size_t found = dll_path.find_last_of( "/\\" );
    dllFolderPath = dll_path.substr( 0, found ).c_str();
    std::string python_script_path = dll_path.substr( 0, found ) + "\\read_mbdf.py";

    // check if mbdf file path exists
    struct stat sb;
    if( ( stat( mMbdfFilepath, &sb ) == 0 ) && mLoadSettingsFromMbdf ) { // run python script and save values to vars
        std::string python_output = RunPythonMbdfParser( python_script_path, "" );
        parsePythonOutput( python_output ); // parse python output and save values to class variables
        settingsFromMBDF = true;
    } else {
        mBitRate = mBitRateInterface->GetInteger();
        mMELIBUVersion = mMELIBUVersionInterface->GetNumber();
    }
    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );

    return true;
}

void MELIBUAnalyzerSettings::UpdateInterfacesFromSettings() {
    mInputChannelInterface->SetChannel( mInputChannel );
    mMbdfFileInterface->SetText( mMbdfFilepath );
    mBitRateInterface->SetInteger( mBitRate );
    mMELIBUVersionInterface->SetNumber( mMELIBUVersion );
    mMELIBUAckEnabledInterface->SetValue( mACK );
    mMELIBULoadFromMbdfInterface->SetValue( mLoadSettingsFromMbdf );
}

void MELIBUAnalyzerSettings::LoadSettings( const char* settings ) {
    SimpleArchive text_archive;
    text_archive.SetString( settings );

    text_archive >> mInputChannel;
    text_archive >> mBitRate;
    text_archive >> mMELIBUVersion;
    text_archive >> mACK;
    text_archive >> mLoadSettingsFromMbdf;
    mMbdfFilepath = text_archive.GetString();

    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );
}

const char* MELIBUAnalyzerSettings::SaveSettings() {
    SimpleArchive text_archive;
    text_archive.SetString( "" );
    text_archive << mInputChannel;
    text_archive << mBitRate;
    text_archive << mMELIBUVersion;
    text_archive << mACK;
    text_archive << mLoadSettingsFromMbdf;
    text_archive << mMbdfFilepath;

    return SetReturnString( text_archive.GetString() );
}

std::string MELIBUAnalyzerSettings::RunPythonMbdfParser( std::string script_path, std::string other_args ) {
    std::string python_exe_path = dllFolderPath;
    python_exe_path += "\\Melexis\\Python\\python.exe";
    std::string whole_input = python_exe_path + " " + script_path + " " + mMbdfFilepath + " " + other_args;
    std::array < char, 256 > buffer;
    std::string result;
    std::shared_ptr < FILE > pipe( _popen( whole_input.c_str(), "r" ), _pclose );
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

void MELIBUAnalyzerSettings::parsePythonOutput( std::string output ) {
    //std::ofstream outfile;
    //outfile.open( "C:\\Projects\\melibu_decoder\\MeLiBuAnalyzer\\build\\Analyzers\\Release\\filename.txt", std::ios_base::app ); // append instead of overwrite
    //outfile << output << "\n-------------\n";
    mMELIBUVersion = stod( output.substr( 0, output.find_first_of( "\n" ) ) );
    output.erase( output.begin(), output.begin() + output.find_first_of( "\n" ) + 1 );
    mBitRate = stod( output.substr( 0, output.find_first_of( "\n" ) ) );
    output.erase( output.begin(), output.begin() + output.find_first_of( "\n" ) + 1 );
    while( output != "" ) {
        U8 slaveAdr = stoi( output.substr( 0, output.find_first_of( "\n" ) ) );
        output.erase( output.begin(), output.begin() + output.find_first_of( "\n" ) + 1 );
        std::string s;
        if( output.find_first_of( "\n" ) != std::string::npos ) {
            s = output.substr( 0, output.find_first_of( "\n" ) );
            output.erase( output.begin(), output.begin() + output.find_first_of( "\n" ) + 1 );
        } else {
            s = output;
            output = "";
        }

        bool ack = s == "True" ? true : false;
        node_ack.insert( std::pair < U8, bool > ( slaveAdr, ack ) );
    }

}

void helperFcn() {};

std::string MELIBUAnalyzerSettings::getDLLPath() {
    char path[ MAX_PATH ];
    HMODULE hm = NULL;
    GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       ( LPCSTR )&helperFcn,
                       &hm );
    GetModuleFileName( hm, path, sizeof( path ) );

    return path;
}
