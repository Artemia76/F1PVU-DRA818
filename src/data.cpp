#include "data.hpp"

Data::Data ()
{
    _read_from_eeprom();
    
    // On test des données
    if ((_data.freq_rx < LIM_FREQ_B) || (_data.freq_rx > LIM_FREQ_H))
    {
        _data.freq_rx = LIM_FREQ_B;
    }

    if ((_data.freq_tx < LIM_FREQ_B) || (_data.freq_tx > LIM_FREQ_H))
    {
        _data.freq_tx = LIM_FREQ_B;
    }

    if ((_data.freq_step < LIM_FREQ_STEP_MIN) || (_data.freq_tx > LIM_FREQ_STEP_MAX))
    {
        _data.freq_step = LIM_FREQ_STEP_MIN;
    }

    _write_to_eeprom();
}

long Data::getFreqTX ()
{
    return _data.freq_tx;
}

long Data::getFreqRX ()
{
    return _data.freq_rx;
}

long Data::getFreqStep ()
{
    return _data.freq_step;
}

bool Data::getRepMode ()
{
    return _data.rep_mode;
}

bool Data::getRevMode ()
{
    return _data.rev_mode;
}

void Data::setFreqTX (const long& pFreqTX)
{
    if (pFreqTX < LIM_FREQ_B)
    {
        _data.freq_tx = LIM_FREQ_B;
    }
    else if (pFreqTX > LIM_FREQ_H)
    {
        _data.freq_tx = LIM_FREQ_H;
    }
    else _data.freq_tx = pFreqTX;
    _write_to_eeprom ();
}

void Data::setFreqRX (const long& pFreqRX)
{
    if (pFreqRX < LIM_FREQ_B)
    {
        _data.freq_rx = LIM_FREQ_B;
    }
    else if (pFreqRX > LIM_FREQ_H)
    {
        _data.freq_rx = LIM_FREQ_H;
    }
    else _data.freq_rx = pFreqRX;
    _write_to_eeprom ();
}

void Data::setFreqStep (const long& pFreqStep)
{
    if (pFreqStep < LIM_FREQ_STEP_MIN)
    {
        _data.freq_step = LIM_FREQ_STEP_MIN;
    }
    else if (pFreqStep > LIM_FREQ_STEP_MAX)
    {
        _data.freq_step = LIM_FREQ_STEP_MAX;
    }
    else _data.freq_step = pFreqStep;
    _write_to_eeprom ();
}

void Data::setRepMode (const bool& pRepMode)
{
    _data.rep_mode = pRepMode;
    _write_to_eeprom ();
}

void Data::setRevMode (const bool& pRevMode)
{
    _data.rev_mode = pRevMode;
    _write_to_eeprom ();
}

void Data::_read_from_eeprom ()
{
    EEPROM.get(0,_data);
}

void Data::_write_to_eeprom ()
{
    EEPROM.put(0,_data);
}