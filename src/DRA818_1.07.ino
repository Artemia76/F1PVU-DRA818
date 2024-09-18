// Calibration des seuils pour le S mètre
// Limitation Fréquences à la bande 2 mètres ( bande passante ampli limitée )
// Limitation du S mètre
// Augmentaion du temps de présentation et modif auteurs
// 1.09 - Reprise Complète par F4IKZ(Gianni) pour intégrer plusieurs corrections :
//  * Librairies long support arduino (SchedTask)
//  * Migration du projet sur platformio pour automatiser les dépendances
//  * Reprise de la gestion du codeur rotatif bidirectionnel
//  * Synchronisation de l'envoi des messages de mise à jour du DRA818 pour éviter les sauts de fréquences
//  * Mémorisation dans l'eeprom des pas de fréquence, mode répéteur, mode reverse

// Inclusion des dépendances
#include <EEPROM.h>
#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include <BoutonPin.h>
#include <SchedTask.h>
#include <SoftwareSerial.h>

// Affectation des E/S
#define codeurRotatifA_3  3   // CLK du codeur Rotatif
#define codeurRotatifB_2  2   // DT du codeur Rotatif
#define PUCH              7   // Bouton poussoir du codeur rotatif
#define SHIFT             11  // Entrée SHIFT 600
#define PTT               9   // Bouton PTT
#define REV               10  // EntréeREV
#define hzPTT             6   // Sortie Tone 1750 Hz
#define sPTT              4   // sortie PTT
#define sRSSi             5   // sortie RSSi
#define DRA818_TX         8   // TX UART
#define DRA818_RX         12  // RX UART

// Définition des constantes
// Limite de fréquence basse en Hz
#define LIMB              144000000
// Limite de fréquence haute en Hz
#define LIMH              146000000

#define DEBUG             1
// Version programme
String vers = "V1.09";

// définit le type d'écran lcd 16 x 2
LiquidCrystal_I2C LCD(0x27,16,2);

SoftwareSerial DRA818 (DRA818_RX, DRA818_TX);

// Afectation des boutons poussoirs 
// Pour la gestion anti rebond 
BoutonPin fdPUCH(PUCH, true, true);
BoutonPin fSHIFT(SHIFT, true, true);
BoutonPin fPTT(PTT, true, true);
BoutonPin fREV(REV, true, true);

// Définition des variables
boolean A_set = true;
boolean B_set = true;
boolean readyB = false;
boolean mSHIFT = false;
boolean mREV = false;
boolean bModeConfig = false;
boolean bAModeConfig = bModeConfig;
boolean bDispChange = false;

int Npas = 1;
int RSSi = 0;
int squ = 1;
int smetre = 0;

long Decaltx = 0;
long Decalrx = 0;
long frx = LIMB;
long Pas = 12500;

// Définition des adresses mémoires pour le stockage de l'état
// du transceiver
int adresse_frx = 0;
int adresse_pas = adresse_frx + sizeof(frx); // Pas
int adresse_rpt = adresse_pas + sizeof(Pas); // Mode Répéteur
int adresse_rev = adresse_rpt + sizeof(mSHIFT); // Mode Reverse

// paramètres réglés pour DRA818V

// Bande passante en kHz ( 0 = 12.5 or 1 = 25 )
String bw = "0";

// fréquence de ctcss (0000 - 0038); 0000 = "no CTCSS"
String tx_ctcss = "0000";

// fréquence de ctcss (0000 - 0038); 0000 = "no CTCSS"
String rx_ctcss = "0000";

// RSSI Callback
void RSSI ();
SchedTask RSSI_Timer (0, 400, RSSI);

// Routine asynchrone RSSI
void RSSI ()
{
  String RRSSI = "";
  DRA818.println ("RSSI?"); //demande RSSI au module
  DRA818.flush();
  delay(10);
  while (DRA818.available() < 9)
  {
    delay(10);
  }
  while (DRA818.available() > 0)
  {
    char c = DRA818.read();
    RRSSI += c;
  }
  #ifdef DEBUG
  Serial.println(RRSSI);
#endif
  RRSSI = RRSSI.substring(5,8);

  RSSi =(RRSSI.toInt());
  smetre = map(RSSi,36,224,0,135);
  if (smetre < 0) smetre = 0;
  analogWrite(sRSSi, smetre);

  bDispChange = true;
}

// Routine Envoi message au DRA818

void Envoi()
{
  String buffer = "";
  //Envoi Data au DRA818
  buffer = "AT+DMOSETGROUP=";
  buffer += String(bw);
  buffer += String(",");
  buffer += String((frx + Decaltx)/ 1000000.0, 4);
  buffer += String(",");
  buffer += String((frx + Decalrx)/ 1000000.0, 4);
  buffer += String(",");
  buffer += String(tx_ctcss);
  buffer += String(",");
  buffer += String(squ);
  buffer += String(",");
  buffer += String(rx_ctcss);
  DRA818.println(buffer);
  DRA818.flush();
  
#ifdef DEBUG
  Serial.println(buffer);
#endif
  buffer = "";
  while (DRA818.available() < 9)
  {
    delay(10);
  }
  // On attend le code retour du SA818
  while (DRA818.available() > 0)
  {
    char c = DRA818.read();
    buffer += c;
  }
#ifdef DEBUG
  Serial.println(buffer);
#endif
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
    Config_REV ();
  }
  else
  {
    mREV = false;
    Decaltx = 0;
    Decalrx = 0; 
  }
  bDispChange = true;
}

/// @brief Gestion du mode reverse en mode répéteur
void Config_REV ()
{
  if (mREV)                                     //Memorisation de D11
  {
    Decaltx = 0;                                   // force le decalage tx a 600 kHz
    Decalrx = 600000;                                       // force le decalage rx a 0
  }
  else
  {
    Decaltx = 600000;                                        // force le decalage tx a 0
    Decalrx = 0;                                  // force le decalage rx a -600 kHz
  }
  bDispChange = true;
}

/// @brief Configuration initiale de l'arduino

void setup()
{
  Serial.begin (9600);
  pinMode(DRA818_RX, INPUT);
  pinMode(DRA818_TX, OUTPUT);
  DRA818.begin (9600);
  
  // Affectation de la sortie pour activer le ton 1750 Hz
  pinMode(hzPTT, OUTPUT);
  
  // Affectation de la sortie pour piloter le PTT
  pinMode(sPTT, OUTPUT);
  digitalWrite (sPTT, LOW);

  // Affectation de la sortie analogique pour l'indication du signal
  pinMode(sRSSi, OUTPUT);

  // Affectation des interruptions matérielles pour le codeur VFO
  attachInterrupt (digitalPinToInterrupt(codeurRotatifA_3), codeurA, CHANGE);
  attachInterrupt (digitalPinToInterrupt(codeurRotatifB_2), codeurB, CHANGE);

  fdPUCH.setLongPressDelay(3000);
  fdPUCH.setLongPressInterval(3000);

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

  LCD.init(); // initialisation de l'afficheur
  LCD.backlight();
  LCD.setCursor(1, 0);
  LCD.print("Transceiver VHF ");
  LCD.print(vers);
  LCD.setCursor(0, 1);
  LCD.print("F1PVU / DL " + vers);
  delay(1000);
  LCD.clear();
  Config_RPT ();
  Envoi();
  bDispChange = true;
}

void Update_Display()
{
  if (bAModeConfig != bModeConfig)
  {
    bAModeConfig = bModeConfig;
    LCD.clear();
    bDispChange = true;
  }
  if (bDispChange)
  {
    bDispChange = false;
    if (bModeConfig)
    {
      LCD.setCursor(0,0);
      LCD.print("CONFIGURATION");
    }
    else
    {
      // Affichage de la fréquence de réception
      LCD.setCursor(0, 0);
      LCD.print((frx + Decalrx) / 1000000.0, 4);

      if (mSHIFT)
      {
        LCD.setCursor(8, 0);
        LCD.print("R");
        LCD.setCursor(0, 1);
        LCD.print((frx + Decaltx) / 1000000.0, 4);
        LCD.setCursor(8, 1);
        LCD.print("T");
      }
      else
      {
        LCD.setCursor(8, 0);
        LCD.print(" ");
        LCD.setCursor(0, 1);
        LCD.print("         ");
      }
      // Affichage du pas de fréquence en kHz
      LCD.setCursor(12, 1);
      LCD.print(Pas/1000.0,1);
      if (mSHIFT)
      {
        LCD.setCursor(10, 0);                                //Curseur LCD 0 ligne 1
        LCD.print("RP");                                   //ecriture RPT sur le LCD    Decaltx = 600000;                                   // force le decalage tx a 600 kHz
      }
      else
      {
        LCD.setCursor(10, 0);                                //Curseur LCD 0 ligne 1
        LCD.print("  ");                                   //effacement RPT sur le LCD
      }
      if (mREV)
      {
        LCD.setCursor(10, 1);                              //Curseur LCD 0 ligne 1
        LCD.print("R");                                 //ecriture RPT sur le LCD
      }
      else
      {
        LCD.setCursor(10, 1);                                //Curseur LCD 0 ligne 1
        LCD.print(" ");                                   //effacement RPT sur le LCD
      }
      LCD.setCursor(13, 0);                                 //Affichage RSSI Olivier
      char buffer[4];
      snprintf(buffer , sizeof buffer, "%03d", RSSi);
      LCD.print(buffer);                                    //Affichage RSSI Olivier
    }
  }

}

/// @brief Gestion du poste en mode traffic
void mode_traffic()
{
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
    EEPROM.put(adresse_pas,Pas);          // ecriture de frx dans l'eprom
    bDispChange = true;
  }
 
  if(fdPUCH.isLongPressed())
  {
    bModeConfig = true;
  }

  // Si la fréquence a changé
  if ( readyB )
  {
      readyB=false;
      EEPROM.put(adresse_frx,frx);          // ecriture de frx dans l'eprom
      Envoi();
      bDispChange=true;
  }
}

/// @brief Gestion du poste en mode configuration
void mode_config()
{
  fdPUCH.refresh();
  if(fdPUCH.isLongPressed())
  {
    bModeConfig = false;
  }
}

/// @brief Programme principal de l'arduino

void loop()
{
  if (bModeConfig)
  {
    mode_config();
  }
  else
  {
    mode_traffic();
  }
  // Execution des tâches asynchrones
  SchedBase::dispatcher();
  Update_Display ();
}
