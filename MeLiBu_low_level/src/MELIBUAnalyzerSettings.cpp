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

#include <stdexcept>

// set initial values for variables and add input interfaces
MELIBUAnalyzerSettings::MELIBUAnalyzerSettings()
    : mInputChannel( UNDEFINED_CHANNEL ),
    mPythonExe( "python" ),
    mBitRate{ 1000000 },
    mMELIBUVersion( 1.0 ),
    mACK( false ),
    mMbdfFilepath( "" ),
    mACKValue( 0x7e ),
    mLoadSettingsFromMbdf( false ),
    settingsFromMBDF( false ),
    pythonScriptError( false ) {

    mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mInputChannelInterface->SetTitleAndTooltip( "Serial", "Standard MeLiBu analyzer" );
    mInputChannelInterface->SetChannel( mInputChannel );

    mPythonExeInterface.reset( new AnalyzerSettingInterfaceText() );
    mPythonExeInterface->SetTitleAndTooltip( "Python path", "Python path" );
    mPythonExeInterface->SetTextType( AnalyzerSettingInterfaceText::NormalText );
    mPythonExeInterface->SetText( mPythonExe );

    mMbdfFileInterface.reset( new AnalyzerSettingInterfaceText() );
    mMbdfFileInterface->SetTitleAndTooltip( "MBDF filepath", "MBDF filepath" );
    mMbdfFileInterface->SetTextType( AnalyzerSettingInterfaceText::FilePath );
    mMbdfFileInterface->SetText( mMbdfFilepath );

    mMELIBULoadFromMbdfInterface.reset( new AnalyzerSettingInterfaceBool() );
    mMELIBULoadFromMbdfInterface->SetTitleAndTooltip( "Settings from MBDF",
                                                      "Select if you want to load settings from entered MBDF file." );
    mMELIBULoadFromMbdfInterface->SetCheckBoxText( "Load MeLiBu version & bit rate from MBDF" );
    mMELIBULoadFromMbdfInterface->SetValue( mLoadSettingsFromMbdf );

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

    mAckValueInterface.reset( new AnalyzerSettingInterfaceText() );
    mAckValueInterface->SetTitleAndTooltip( "ACK value (only for MeLiBu 2)", "Define valid ACK value for MeLiBu 2" );
    mAckValueInterface->SetTextType( AnalyzerSettingInterfaceText::NormalText );
    mAckValueInterface->SetText( ( std::to_string( mACKValue ) ).c_str() );

    AddInterface( mInputChannelInterface.get() );
    AddInterface( mPythonExeInterface.get() );
    AddInterface( mMbdfFileInterface.get() );
    AddInterface( mMELIBULoadFromMbdfInterface.get() );
    AddInterface( mBitRateInterface.get() );
    AddInterface( mMELIBUVersionInterface.get() );
    AddInterface( mMELIBUAckEnabledInterface.get() );
    AddInterface( mAckValueInterface.get() );

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
    this->mInputChannel = this->mInputChannelInterface->GetChannel();
    this->mPythonExe = this->mPythonExeInterface->GetText();
    this->mMbdfFilepath = this->mMbdfFileInterface->GetText();
    this->mACK = this->mMELIBUAckEnabledInterface->GetValue();
    this->mLoadSettingsFromMbdf = this->mMELIBULoadFromMbdfInterface->GetValue();
    this->mBitRate = this->mBitRateInterface->GetInteger();
    this->mMELIBUVersion = this->mMELIBUVersionInterface->GetNumber();
    this->mACKValue = std::stoul( this->mAckValueInterface->GetText(), nullptr, 16 );

    this->settingsFromMBDF = false; // reset
    this->node_ack.clear();         // empty (node,ack) map

    // get dll path and create path to python script
    std::string dll_path = getDLLPath(); // .../MeLiBu_low_level/build/Analyzers/Release/MELIBUAnalyzer.dll
    std::string python_script_path = dll_path;
    for( int i = 0; i < 4; i++ ) {
        std::size_t found = python_script_path.find_last_of( "/\\" );
        python_script_path = python_script_path.substr( 0, found );
    }
    // python_script_path =  .../MeLiBu_low_level
    python_script_path += "\\read_mbdf.py";
    std::ofstream outfile;
    outfile.open( "C:\\Projects\\melibu_decoder\\MeLiBu_low_level\\filename.txt", std::ios_base::app ); //
    outfile << python_script_path << "\n-------------\n";

    // check if mbdf file path exists
    struct stat sb;
    if( ( stat( this->mMbdfFilepath, &sb ) == 0 ) && this->mLoadSettingsFromMbdf ) { // run python script and save values to vars
        std::string python_output = RunPythonMbdfParser( python_script_path, "" );
        if( python_output == "" ) { // error in python script
            this->pythonScriptError = true;
            this->settingsFromMBDF = false;
        } else {
            parsePythonOutput( python_output ); // parse python output and save values to class variables
            this->settingsFromMBDF = true;
            this->pythonScriptError = false;
        }
    }
    ClearChannels();
    AddChannel( this->mInputChannel, "MeLiBu analyzer", true );

    return true;
}

void MELIBUAnalyzerSettings::UpdateInterfacesFromSettings() {
    this->mInputChannelInterface->SetChannel( this->mInputChannel );
    this->mMbdfFileInterface->SetText( this->mMbdfFilepath );
    this->mBitRateInterface->SetInteger( this->mBitRate );
    this->mMELIBUVersionInterface->SetNumber( this->mMELIBUVersion );
    this->mMELIBUAckEnabledInterface->SetValue( this->mACK );
    this->mMELIBULoadFromMbdfInterface->SetValue( this->mLoadSettingsFromMbdf );
}

void MELIBUAnalyzerSettings::LoadSettings( const char* settings ) {
    SimpleArchive text_archive;
    text_archive.SetString( settings );

    text_archive >> this->mInputChannel;
    text_archive >> this->mBitRate;
    text_archive >> this->mMELIBUVersion;
    text_archive >> this->mACK;
    text_archive >> this->mLoadSettingsFromMbdf;
    this->mMbdfFilepath = text_archive.GetString();

    ClearChannels();
    AddChannel( this->mInputChannel, "MeLiBu analyzer", true );
}

const char* MELIBUAnalyzerSettings::SaveSettings() {
    SimpleArchive text_archive;
    text_archive.SetString( "" );
    text_archive << this->mInputChannel;
    text_archive << this->mBitRate;
    text_archive << this->mMELIBUVersion;
    text_archive << this->mACK;
    text_archive << this->mLoadSettingsFromMbdf;
    text_archive << this->mMbdfFilepath;

    return SetReturnString( text_archive.GetString() );
}

std::string MELIBUAnalyzerSettings::RunPythonMbdfParser( std::string script_path, std::string other_args ) {
    std::string python_exe_path = this->mPythonExe;
    std::string whole_input = python_exe_path + " " + script_path + " " + this->mMbdfFilepath + " " + other_args;
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
    this->mMELIBUVersion = stod( output.substr( 0, output.find_first_of( "\n" ) ) );   // read first line
    output.erase( output.begin(), output.begin() + output.find_first_of( "\n" ) + 1 ); // delete first line
    this->mBitRate = stod( output.substr( 0, output.find_first_of( "\n" ) ) );         // read first line
    output.erase( output.begin(), output.begin() + output.find_first_of( "\n" ) + 1 ); // delete first line
    // read first line and delete it while there are no lines to read
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
        this->node_ack.insert( std::pair < U8, bool > ( slaveAdr, ack ) ); // add to map
    }

}

void helperFcn() {}; // used for getDLLPath()

std::string MELIBUAnalyzerSettings::getDLLPath() {
    char path[ MAX_PATH ];
    HMODULE hm = NULL;
    GetModuleHandleEx( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       ( LPCSTR )&helperFcn,
                       &hm );
    GetModuleFileName( hm, path, sizeof( path ) );

    return path;
}
