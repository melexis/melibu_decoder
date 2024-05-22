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
    mBitRate{ 1000000 },
    mMELIBUVersion( 1.0 ),
    mACK( false ),
    mACKValue( 0x7e ) {

    mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
    mInputChannelInterface->SetTitleAndTooltip( "Serial", "Standard MeLiBu analyzer" );
    mInputChannelInterface->SetChannel( mInputChannel );

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
    this->mACK = this->mMELIBUAckEnabledInterface->GetValue();
    this->mBitRate = this->mBitRateInterface->GetInteger();
    this->mMELIBUVersion = this->mMELIBUVersionInterface->GetNumber();
    this->mACKValue = std::stoul( this->mAckValueInterface->GetText(), nullptr, 16 );


    //std::ofstream outfile;
    //outfile.open( "C:\\Projects\\melibu_decoder\\MeLiBu_low_level\\filename.txt", std::ios_base::app ); //
    //outfile << python_script_path << "\n-------------\n";
    // outfile.close();

    ClearChannels();
    AddChannel( this->mInputChannel, "MeLiBu analyzer", true );

    return true;
}

void MELIBUAnalyzerSettings::UpdateInterfacesFromSettings() {
    this->mInputChannelInterface->SetChannel( this->mInputChannel );
    this->mBitRateInterface->SetInteger( this->mBitRate );
    this->mMELIBUVersionInterface->SetNumber( this->mMELIBUVersion );
    this->mMELIBUAckEnabledInterface->SetValue( this->mACK );
}

void MELIBUAnalyzerSettings::LoadSettings( const char* settings ) {
    SimpleArchive text_archive;
    text_archive.SetString( settings );

    text_archive >> this->mInputChannel;
    text_archive >> this->mBitRate;
    text_archive >> this->mMELIBUVersion;
    text_archive >> this->mACK;

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

    return SetReturnString( text_archive.GetString() );
}
