#pragma once

// R-Bus fault codes (PGN 126978 Error Message), generated from the RedVision app's
// FaultCodes enum: code, short title, description. Do not edit by hand.

#include <cstdint>

namespace esphome {
namespace redarc_rbus {

struct FaultCode {
  uint16_t code;
  const char *title;
  const char *description;
};

static const FaultCode FAULT_CODES[] = {
    {0x0110, "Multiple Battery Sensors", "Multiple battery sensors found on the bus."},
    {0x0210, "Dry Cell", "Profile detected dry cell -- voltage drops too fast when not held up by charger. This is detected by twelve cycles through the Float/Maintain loop within five minutes. Does not occur in Touring mode."},
    {0x0220, "Dead Cell", "Profile detected dead cell -- voltage drops too far when not being charged. This is detected during a 30 second open circuit 20 minutes into charging. Drop threshold delta is -2V."},
    {0x0230, "Battery Unrecoverable", "Battery Recovery has failed."},
    {0x0231, "Start Batt Recovery Error", "Start Battery Recovery has failed"},
    {0x0232, "Start Batt Recovery Error No Battery", "Start Battery Recovery cannot detect battery on desired port"},
    {0x0310, "Charger Over Current", "Output over current fault"},
    {0x0320, "Charger Over Voltage", "Output over voltage fault"},
    {0x0410, "Battery Over Temperature", "Output battery is too hot to safely charge. From sensor measurement. What should the upper limit be\\?"},
    {0x0420, "Battery Under Temperature", "Output battery is too cold to safely charge. From sensor measurement. What should the lower limit be\\?"},
    {0x0430, "Device Over Temperature", "Unit internals too hot to safely charge. Upper limits defined for each sensor."},
    {0x0440, "Load Disconnect Shorted", "Load disconnect output shorted"},
    {0x0450, "Battery Over Voltage", "Output battery over voltage"},
    {0x0451, "Battery Flat", "The Battery is Flat"},
    {0x0452, "Battery Disconnected", "Output Battery is disconnected, but battery sensor is still reporting. This could be due to cable fault, isolation switch disconnection, or battery fuse blown."},
    {0x0453, "Battery Missing", "Output Battery is disconnected, and battery sensor is not reporting (or there is no battery sensor). This could be due to battery self-protection shutdown, cable fault, isolation switch disconnection, or battery fuse blown."},
    {0x0460, "Internal Hardware Fault", "Internal hardware fault"},
    {0x0461, "Rbus Comms Fault", "RBUS communication fault"},
    {0x0462, "Configuration Fault", "Device configuration or compatibility error"},
    {0x0463, "Internal Hardware Warning", "Non-fatal internal hardware fault"},
    {0x0464, "External Flash", "External flash failure"},
    {0x0470, "Battery Over Current", "Battery Current is higher than some user-defined threshold"},
    {0x0510, "Bad Cable", "Too much voltage drop between charger and battery sensor. Indicates cable is dodgy or too thin."},
    {0x0520, "Reverse Polarity Detection", "Reverse Polarity Detection"},
    {0x0530, "Short Circuit", "Output battery voltage is extremely low while charging. This could be due to output being shorted to ground, or excessive load is connected."},
    {0x0610, "Solar Over Voltage", "Solar input over voltage"},
    {0x0620, "Dc Over Voltage", "DC input over voltage"},
    {0x0621, "Dc Under Voltage", "DC input under voltage"},
    {0x0630, "Ac Over Voltage", "AC input over voltage"},
    {0x0640, "Ac Over Temperature", "AC board over temperature"},
    {0x0641, "Ac Under Temperature", "AC board under temperature"},
    {0x0750, "Loads Overcurrent", "Load overcurrent trip"},
    {0x1000, "Inverter Olpl", "Overload protection low (101~115%)"},
    {0x1001, "Inverter Olpm", "Overload protection middle (116~200%)"},
    {0x1002, "Inverter Olph", "Overload protection high (>200%)"},
    {0x1003, "Inverter Oocp", "Output over current protection"},
    {0x1010, "Inverter Uvp", "Under voltage protection"},
    {0x1011, "Inverter Ovp", "Over voltage protection"},
    {0x1020, "Inverter Otp", "Over temperature protection"},
    {0x1021, "Inverter Utp", "Under temperature protection"},
    {0x1030, "Inverter Id", "ID fail"},
    {0x1031, "Inverter Sof", "Soft start fail"},
    {0x1032, "Inverter Pll", "PLL fail"},
    {0x1033, "Inverter Poff", "Power off"},
    {0x1034, "Inverter Short", "Short protection"},
    {0x1040, "Inverter Comms", "Communications failed with inverter"},
    {0x1100, "Power Fuse Blown", "Power fuse blown"},
    {0x1101, "Internal Comms Fault", "Internal communications failed"},
    {0x1102, "Local Override", "Local Override Activated"},
    {0x1200, "Trailer Detection Fault", "Trailer detection failed"},
    {0x1201, "Brake Output Fault", "Brake output fault"},
    {0x1202, "Brake Light Fault", "Brake light fault"},
    {0x1203, "Auxiliary Output Fault", "Auxiliary output fault"},
    {0x1204, "Ground Disconnection", "System detects ground connection faults"},
    {0x1205, "Hydraulic Actuator Fault", "System detects fault on connected hydraulic actuator"},
    {0x1206, "Brake On Lamp Circuit Fault", "Stop lamp and brake circuits are swapped or connected together"},
    {0x8010, "No Battery Sensor", "No communications from the battery sensor for 2 seconds. Sensor declared Missing In Action. Charger will drop to 2A limp mode."},
    {0x8110, "Low Soc", "State of charge is below some user-defined threshold."},
    {0x8120, "Low Voltage", "Output battery voltage is below some user-defined threshold."},
    {0xE000, "Current Trip Phase A", "Phase-A over/under current"},
    {0xE001, "Current Trip Phase B", "Phase-B over/under current"},
    {0xE010, "Over Current Trip Software", "Software over current trip"},
    {0xE011, "Under Current Trip Software", "Software under current trip"},
    {0xE020, "Internal Supply Undervoltage", "Internal Power Supply Undervoltage"},
    {0xE030, "Internal Recovery Active", "Internal recovery active, automatically attempt to recover and continue from an internal error/fault condition"},
    {0xFA00, "Battery Sensor Calibration", "Battery sensor flash corrupted (calibration and manufacturing values)"},
    {0xFA01, "Adc Insane", "Redundant ADCs giving inconsistent reports"},
    {0xFA02, "Regulator Insane", "Regulator giving insane values"},
    {0xFA10, "Ac Lopsided Mains", "AC waveform error"},
    {0xFA11, "Ac Vmid Over Voltage", "AC over voltage"},
    {0xFA20, "Battery Sensor Eeprom", "Battery sensor eeprom corrupted (user settings)"},
};

}  // namespace redarc_rbus
}  // namespace esphome
