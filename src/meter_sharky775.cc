/*
 Copyright (C) 2018-2021 Fredrik Öhrström

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"meters.h"
#include"meters_common_implementation.h"
#include"dvparser.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

struct Sharky775 : public virtual HeatMeter, public virtual MeterCommonImplementation {
    Sharky775(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);
    double targetEnergyConsumption(Unit u);
    double currentPowerConsumption(Unit u);
    string status();
    double totalVolume(Unit u);
    double targetVolume(Unit u);

private:
    void processContent(Telegram *t);

    uchar info_codes_ {};
    double total_energy_kwh_ {};
    double target_energy_kwh_ {};
    double current_power_kw_ {};
    double total_volume_m3_ {};
    string target_date_ {};
};

Sharky775::Sharky775(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::SHARKY775)
{
    setExpectedELLSecurityMode(ELLSecurityMode::AES_CTR);

    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("current_power_consumption", Quantity::Power,
             [&](Unit u){ return currentPowerConsumption(u); },
             "Current power consumption.",
             true, true);

    addPrint("total_volume", Quantity::Volume,
             [&](Unit u){ return totalVolume(u); },
             "Total volume of heat media.",
             true, true);

    addPrint("at_date", Quantity::Text,
             [&](){ return target_date_; },
             "Date when total energy consumption was recorded.",
             false, true);

    addPrint("total_energy_consumption_at_date", Quantity::Energy,
             [&](Unit u){ return targetEnergyConsumption(u); },
             "The total energy consumption recorded at the target date.",
             false, true);

    addPrint("current_status", Quantity::Text,
             [&](){ return status(); },
             "Status of meter.",
             true, true);
}

shared_ptr<HeatMeter> createSharky775(MeterInfo &mi) {
    return shared_ptr<HeatMeter>(new Sharky775(mi));
}

double Sharky775::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(total_energy_kwh_, Unit::KWH, u);
}

double Sharky775::targetEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(target_energy_kwh_, Unit::KWH, u);
}

double Sharky775::totalVolume(Unit u)
{
    assertQuantity(u, Quantity::Volume);
    return convert(total_volume_m3_, Unit::M3, u);
}

double Sharky775::currentPowerConsumption(Unit u)
{
    assertQuantity(u, Quantity::Power);
    return convert(current_power_kw_, Unit::KW, u);
}

void Sharky775::processContent(Telegram *t)
{
    /*
      (sharky775) 11: bcdb payload crc
      (sharky775) 13: 78 frame type (long frame)
      (sharky775) 14: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (sharky775) 15: 06 vif (Energy kWh)
      (sharky775) 16: * 2C0000 total energy consumption (44.000000 kWh)
      (sharky775) 19: 43 dif (24 Bit Integer/Binary Instantaneous value storagenr=1)
      (sharky775) 1a: 06 vif (Energy kWh)
      (sharky775) 1b: * 000000 target energy consumption (0.000000 kWh)
      (sharky775) 1e: 03 dif (24 Bit Integer/Binary Instantaneous value)
      (sharky775) 1f: 14 vif (Volume 10⁻² m³)
      (sharky775) 20: * 630000 total volume (0.990000 m3)
      (sharky775) 23: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (sharky775) 24: 6C vif (Date type G)
      (sharky775) 25: * 7F2A target date (2019-10-31 00:00)
      (sharky775) 27: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (sharky775) 28: 2D vif (Power 10² W)
      (sharky775) 29: * 1300 current power consumption (1.900000 kW)
      (sharky775) 2b: 01 dif (8 Bit Integer/Binary Instantaneous value)
      (sharky775) 2c: FF vif (Vendor extension)
      (sharky775) 2d: 21 vife (per minute)
      (sharky775) 2e: * 00 info codes (00)
    */

    int offset;
    string key;

    extractDVuint8(&t->values, "01FF21", &offset, &info_codes_);
    t->addMoreExplanation(offset, " info codes (%s)", status().c_str());

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_energy_kwh_);
        t->addMoreExplanation(offset, " total energy consumption (%f kWh)", total_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::Volume, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &total_volume_m3_);
        t->addMoreExplanation(offset, " total volume (%f m3)", total_volume_m3_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::EnergyWh, 1, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &target_energy_kwh_);
        t->addMoreExplanation(offset, " target energy consumption (%f kWh)", target_energy_kwh_);
    }

    if(findKey(MeasurementType::Instantaneous, ValueInformation::PowerW, 0, 0, &key, &t->values)) {
        extractDVdouble(&t->values, key, &offset, &current_power_kw_);
        t->addMoreExplanation(offset, " current power consumption (%f kW)", current_power_kw_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::Date, 1, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        target_date_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " target date (%s)", target_date_.c_str());
    }
}

string Sharky775::status()
{
    string s;
    return s;
}
