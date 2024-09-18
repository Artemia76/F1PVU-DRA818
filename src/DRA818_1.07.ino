#include <easyRun.h>

// Version programme
String vers = "V1.09";

//Calibration des seuils pour le S mètre
//Limitation Fréquences à la bande 2 mètres ( bande passante ampli limitée )
//Limitation du S mètre
//Augmentaion du temps de présentation et modif auteurs


#include <EEPROM.h>

//Afficheur
#include "Wire.h"
#include "LiquidCrystal_I2C.h"
#include <Bouton.h>
#include "easyRun.h"

// définit le type d'écran lcd 16 x 2
LiquidCrystal_I2C LCD(0x27,16,2);

// Encodeur rotatif
volatile boolean modif;
volatile boolean up;

// Affectation des PIN
#define codeurRotatifA_3  3   // CLK Rotary
#define codeurRotatifB_2  2   // DT Rotary
#define PUCH              7   // sw Rotary
#define SHIFT             11  // Entrée SHIFT 600
#define PTT               9   // Bouton PTT
#define REV               10  // EntréeREV

#define hzPTT 6    // Sortie Tone 1750 Hz
#define sPTT 4     // sortie PTT
#define sRSSi 5     // sortie RSSi

Bouton fdPUCH(PUCH,1,1);
Bouton fSHIFT(SHIFT,1,1);
Bouton fPTT(PTT,1,1);
Bouton fREV(REV,1,1);

// Définition des constantes
// Limite de fréquence basse
#define LIMB  144000000
// Limite de fréquence haute
#define LIMH  146000000

// Définition des variables
boolean A_set = true;
boolean B_set = true;
boolean readyB = false;
boolean mSHIFT = false;
boolean mREV = false;

int Npas;
int val;
int RSSi;
int adresse_frx;
int adresse_pas;
int adresse_rpt;
int adresse_rev;
long Decaltx;
long Decalrx;
long frx;
long Pas;


// paramètres réglés pour DRA818V

// Bande passante en kHz ( 0 = 12.5 or 1 = 25 )
String bw = "0";
// fréquence de ctcss (0000 - 0038); 0000 = "no CTCSS"
String tx_ctcss = "0000";
// fréquence de ctcss (0000 - 0038); 0000 = "no CTCSS"
String rx_ctcss = "0000";

//Réglage du squelch
// niveau du silencieux (0-8); 0 = ouvert"
String squ = "0";

//tâches périodiques
asyncTask lanceur;

// Routine asynchrone  RSSI
void RSSI ()
{
  String RRSSI = "";
  Serial.println ("RSSI?"); //demande RSSI au module
  Serial.flush();
  LCD.setCursor(13, 0);
  LCD.print("   ");
  while (Serial.available() > 0)
  {
    char v = Serial.read();
    RRSSI += v;
  }
  RRSSI = RRSSI.substring(5,8);

  int vrssi =(RRSSI.toInt());
  int svrssi = map(vrssi, 0, 255, 0, 255);      //Mise a l'echelle b_source,_h_source,b_desti,h_desti Olivier
  LCD.setCursor(13, 0);                                 //Affichage RSSI Olivier
  LCD.print(svrssi);                                    //Affichage RSSI Olivier
  RRSSI = "";
                                                                //Debut reglage RSSI Olivier
  if (svrssi == 0)analogWrite(sRSSi, 0);                            
  if ((svrssi > 0) && (svrssi < 36))analogWrite(sRSSi, 0);           //S0     
  if ((svrssi > 36) && (svrssi < 41))analogWrite(sRSSi, 7);          //S1
  if ((svrssi > 41) && (svrssi < 48))analogWrite(sRSSi, 15);         //S2
  if ((svrssi > 48) && (svrssi < 54))analogWrite(sRSSi, 23);         //S3
  if ((svrssi > 54) && (svrssi < 57))analogWrite(sRSSi, 35);         //S4
  if ((svrssi > 57) && (svrssi < 61))analogWrite(sRSSi, 50);         //S5
  if ((svrssi > 61) && (svrssi < 67))analogWrite(sRSSi, 60);         //S6
  if ((svrssi > 67) && (svrssi < 72))analogWrite(sRSSi, 70);         //S7
  if ((svrssi > 72) && (svrssi < 81))analogWrite(sRSSi, 80);         //S8
  if ((svrssi > 81) && (svrssi < 92))analogWrite(sRSSi, 95);         //S9
  if ((svrssi > 92) && (svrssi < 98))analogWrite(sRSSi, 110);        //S9+10
  if ((svrssi > 98) && (svrssi < 109))analogWrite(sRSSi, 125);       //S9+20
  if ((svrssi > 110) && (svrssi < 224))analogWrite(sRSSi, 135);      //Au dela de S9+20
  //if ((svrssi > 254) && (svrssi < 255))analogWrite(sRSSi, 140);
  //Fin reglage RSSI Olivier
  
  //Frequence de mise à jour en msec RSSI Olivier
  lanceur.set(RSSI, 400);
}

// Routine Envoi message au SA818

void Envoi()
{
  String Ret = "";
  //Envoi Data au DRA818
  Serial.print ("AT+DMOSETGROUP="); // commencer un message
  Serial.print (bw);
  Serial.print (",");
  Serial.print ((frx - Decalrx)/ 1000000.0, 4);
  Serial.print (",");
  Serial.print ((frx - Decaltx)/ 1000000.0, 4);
  Serial.print (",");
  Serial.print (tx_ctcss);
  Serial.print (",");
  Serial.print (squ);
  Serial.print (",");
  Serial.println (rx_ctcss);
  Serial.flush();
  
  // On attend le code retour du SA818
  while (Serial.available() > 0)
  {
    char v = Serial.read();
    Ret += v;
  }

}

// Gestion de l'interruption codeur sans le sens horaire
void codeurA()
{
  // Changement d'état du codeur 
  if (digitalRead(codeurRotatifA_3) != A_set) 
  { 
    A_set = !A_set;

    if (A_set && !B_set && (frx <= LIMH - Pas)) 
    {
      // Incrémente la valeur 
      frx += Pas;
      readyB=true;
    }
  }
}

// Gestion de l'interruption codeur sans le sens anti-horaire
void codeurB()
{
  if (digitalRead(codeurRotatifB_2) != B_set) 
  { 
    B_set = !B_set;

    if (B_set && !A_set && (frx >= LIMB + Pas)) 
    {
      // Decrémente la valeur 
      frx -= Pas;
      readyB=true;
    }
  }
}

/// @brief Gestion du mode Repéteur / Simplex
void Config_RPT ()
{
  if (mSHIFT)                                  //Memorisation de D11
  {
    LCD.setCursor(0, 1);                                //Curseur LCD 0 ligne 1
    LCD.print("RPT");                                   //ecriture RPT sur le LCD
    Decalrx = 600000;                                   // force le decalage rx a 600
  }
  else
  {
    mREV = 0;
    LCD.setCursor(0, 1);                                //Curseur LCD 0 ligne 1
    LCD.print("           ");                           //effacement RPT sur le LCD
    Decaltx = 0;                                        // force le decalage tx a 0
    Decalrx = 0;                                        // force le decalage rx a 0
  }
}

/// @brief Gestion du mode reverse en mode répéteur
void Config_REV ()
{
  if (mREV)                                     //Memorisation de D11
    {
      LCD.setCursor(5, 1);                                //Curseur LCD 0 ligne 1
      LCD.print("REV");                                   //ecriture RPT sur le LCD
      Decaltx = 600000;                                   // force le decalage tx a 600 kHz
      Decalrx = 0;                                       // force le decalage rx a 0
    }
    else
    {
      LCD.setCursor(5, 1);                                //Curseur LCD 0 ligne 1
      LCD.print("   ");                                   //effacement RPT sur le LCD
      Decaltx = 0;                                        // force le decalage tx a 0
      Decalrx = -600000;                                  // force le decalage rx a -600 kHz
    }
}

/// @brief Configuration initiale de l'arduino

void setup()
{
  Serial.begin (9600);
  lanceur.set(RSSI, 0);
  pinMode(hzPTT, OUTPUT);
  pinMode(sPTT, OUTPUT);
  pinMode(sRSSi, OUTPUT);
  pinMode(codeurRotatifA_3, INPUT_PULLUP); //Encodeur rotatif
  pinMode(codeurRotatifB_2, INPUT_PULLUP); //Econdeur rotatif

  digitalWrite (sPTT, LOW);       //Sortie PTT

  attachInterrupt (digitalPinToInterrupt(codeurRotatifA_3), codeurA, CHANGE);
  attachInterrupt (digitalPinToInterrupt(codeurRotatifB_2), codeurB, CHANGE);
  //lanceur.set(Lecture, 400);      //tâches périodiques definition du temps lecture port serie
  adresse_frx = 0;
  adresse_pas = adresse_frx + sizeof(frx);
  adresse_rpt = adresse_pas + sizeof(Pas);
  adresse_rev = adresse_rpt + sizeof(mSHIFT);
  Decalrx = 0;
  Decalrx = 0;
  Pas = 12500; // 12.5 kHz
  squ = 1;
  Npas = 1;
  modif = false;
  mSHIFT = false;
  mREV = false;
  val = 0;
  RSSi = 0;

  // lecture du pas dans l'eprom
   EEPROM.get(adresse_pas,Pas);
  if ( (Pas < 12500) || (Pas > 1000000) )
  {
    Pas = 12500;
    EEPROM.put(adresse_pas,Pas);          // ecriture de frx dans l'eprom
  }

  // lecture de frx dans l'eprom
  EEPROM.get(adresse_frx,frx);
  if ( isnan(frx) || (frx < LIMB) || (frx > LIMH) )
  {
    frx = LIMB + Pas;
    EEPROM.put(adresse_frx,frx);          // ecriture de frx dans l'eprom
  }
 
  // lecture de frx dans l'eprom
  EEPROM.get(adresse_rpt,mSHIFT);


  // lecture de frx dans l'eprom
  EEPROM.get(adresse_rev,mREV);
  if (mREV && !mSHIFT)
  {
    mREV = false;
    EEPROM.put(adresse_rev,mREV); 
  }

  //-------------- Affichage lors de l'initialisation  -------------
  LCD.init(); // initialisation de l'afficheur
  LCD.backlight();
  LCD.setCursor(1, 0);
  LCD.print("Transceiver VHF ");
  LCD.print(vers);
  LCD.setCursor(0, 1);
  LCD.print("F1PVU / DL " + vers);
  delay(1000);
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.print(frx / 1000000.0, 4);  // Fréquence affichée au démarrage.
  LCD.setCursor(12, 1);
  LCD.print(Pas/1000.0,1);
  Config_RPT ();
  Config_REV ();
  Envoi();
}

/// @brief Programme principal de l'arduino

void loop()
{
  // Lancement de la tâche asynchrone 
  easyRun();
  
  //  Appel 1750 Hz : Entrée D9 en mode poussoir qui active la sortie PTT ( D5 ) et génère une tonalité à 1750 Hz sur la sortie D6
  fPTT.refresh();                                       
  if (fPTT.isOnPress())                                //Front descendant de D9
  {
    tone(hzPTT, 1750);
    digitalWrite (sPTT, HIGH);
  } //Sortie  D5 a 1750 Hz et D6 a 1
  else
  {
    noTone(hzPTT);
    digitalWrite (sPTT, LOW);
  } //Reinit  D5 et D6 a 0

  //  Shift - 600 kHz :Entrée D11 mode Flip/Flop active le mode relais
  //  ( fréquence TX = Fréquence RX - 600 kHz) et affichage " RPT" sur le display.
  fSHIFT.refresh();
  if (fSHIFT.isPressed())
  {
    mSHIFT = !mSHIFT;
    Config_RPT ();
    // Sauvegarde en eeprom du mode sélectionné
    EEPROM.put(adresse_rpt,mSHIFT);
    // Envoi des données au SA818
    Envoi();
  }

  //Inverse : Entrée D7 mode Flip/Flop qui inverse Fréquence TX et Fréquence RX en mode Shift-600
  //pour écouter la fréquence d'entrée d'un relai et affichage "REV" sur display.
  fREV.refresh();
  if (fREV.isPressed() && mSHIFT == 1 )               //Front descendant de D11 et shift
  {
    mREV = !mREV;
    Config_REV ();
    EEPROM.put(adresse_rev,mREV);
    Envoi();
  }
 
  //Pas : Entrée D10  mode poussoir qui modifie le pas d'incrément 1MHz, 100 kHz, 25 kHz ou 12.5 kHz.
  fdPUCH.refresh();
  if (fdPUCH.isReleased())
  {
    Npas++;
    LCD.setCursor(11, 1);
    LCD.print("     ");
    if (Npas>= 5)
    {
      Npas = 1;
    }
    switch (Npas)
    {
      case 1: 
        Pas = 12500;
        break;
      case 2: 
        Pas = 25000;
        break;
      case 3:
        Pas = 100000;
        break;
      case 4:
        Pas = 1000000;
        break;
      default:
        Pas = 12500;
    }
    LCD.setCursor(12, 1);
    LCD.print(Pas/1000.0,1);
    EEPROM.put(adresse_pas,Pas);          // ecriture de frx dans l'eprom
  }
 
  // Si la fréquence a changé
  if ( readyB )
  {
      LCD.setCursor(0, 0);
      LCD.print((frx / 1000000.0), 4);
      EEPROM.put(adresse_frx,frx);          // ecriture de frx dans l'eprom
      Envoi();
      readyB=false;
  }
}
