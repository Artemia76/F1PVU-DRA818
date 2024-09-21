#ifndef DATA_HPP
#define DATA_HPP

// Définition des constantes
// Limite de fréquence basse en Hz
#define LIM_FREQ_B              144000000

// Limite de fréquence haute en Hz
#define LIM_FREQ_H              146000000

// Limite de pas minimum
#define LIM_FREQ_STEP_MIN       12500

// Limite de pas supérieur
#define LIM_FREQ_STEP_MAX       1000000

#include <EEPROM.h>

struct mem_data
{
    long freq_tx;
    long freq_rx;
    long freq_step;
    bool rep_mode;
    bool rev_mode;
};

class Data
{
public:
    Data ();

    long getFreqTX ();
    long getFreqRX ();
    long getFreqStep ();
    bool getRepMode ();
    bool getRevMode ();

    void setFreqTX (const long& pFreqTX);
    void setFreqRX (const long& pFreqRX);
    void setFreqStep (const long& pFreqStep);
    void setRepMode (const bool& pRevMode);
    void setRevMode (const bool& pRevMode);

private:

    mem_data _data;

    void _read_from_eeprom ();
    void _write_to_eeprom ();

};

#endif //DATA_HPP