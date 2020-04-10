#pragma once

#include "eeprom_metadata.h"
EEPROMMetadata e2prom_metadata;

static constexpr EEPROMMetadata current_configuration{
    MetadataVersion{
        .major = 1,
        .minor = 0,
        .patch = 0,
    },
    DeviceType::DotPhat,
    SoftwareVersion{
     .major = 1,
     .minor = 0,
     .patch = 0},
    TimeZoneInfo{
     .timezone_sign = 1,
     .utc_offset = 1,
     .is_daylight_saving_active = LOCATION_DAYLIGHT_SAVING,
     .is_china_time = 0},
    {// software_version_last_updated_timestamp
     static_cast<uint8_t>(UNIX_TIMESTAMP >> 24),
     static_cast<uint8_t>(UNIX_TIMESTAMP >> 16),
     static_cast<uint8_t>(UNIX_TIMESTAMP >> 8),
     static_cast<uint8_t>(UNIX_TIMESTAMP)},
    HardwareVersion{
     .major = 1,
     .minor = 1,
     .patch = 0},
    TimeZoneInfo{
     .timezone_sign = 1,
     .utc_offset = 8,
     .is_daylight_saving_active = 1,
     .is_china_time = 1},
    {// hardware_version_timestamp : add 8 hours to the PCB manufactureing time
     // in China
     0x5A, 0xBF, 0x78, 0x1D},
    {"Board1"},
    UEnergyInfo{
     {InstalledCapacity::None, 0}},
    GPSPosition{
        48.189756, 11.572531
    },
    {"No xtra loc info"},
    InstalledDevices{
     .temperature_sensor = 1,
     .ultraviolet_sensor = 1,
     .eeprom = 1,
     .piezo_speaker = 0,
     .crypto_module = 0,
     .high_precision_time_reference = 1,
     .reset_pushbutton = 1,
     .act_pushbutton = 1},
    InstalledDevices2{
     .usb = 1,
     .external_antenna = 0,
     .antenna_calibration = 0,
     .solar_panel = 1},
    InstalledLeds{
     .usb_power = 1,
     .outA = 1,
     .outB = 1,
     .reset = 1,
     .rgb = 1,
     .tx = 1,
     .rx = 1,
     .reserved = 1}};
