#include "MELIBUAnalyzerSettings.h"
#include <AnalyzerHelpers.h>
#include <iostream>


MELIBUAnalyzerSettings::MELIBUAnalyzerSettings() : mInputChannel( UNDEFINED_CHANNEL ), mBitRate( 9600 ), mMELIBUVersion(
        2.0 ), mACK( false ) {
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


    AddInterface( mInputChannelInterface.get() );
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
    mBitRate = mBitRateInterface->GetInteger();
    mMELIBUVersion = mMELIBUVersionInterface->GetNumber();
    mACK = mMELIBUAckEnabledInterface->GetValue();

    ClearChannels();
    AddChannel( mInputChannel, "MeLiBu analyzer", true );

    return true;
}

void MELIBUAnalyzerSettings::UpdateInterfacesFromSettings() {
    mInputChannelInterface->SetChannel( mInputChannel );
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

    return SetReturnString( text_archive.GetString() );
}
