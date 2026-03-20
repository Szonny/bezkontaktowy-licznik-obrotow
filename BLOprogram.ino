#include <LiquidCrystal.h>
#include <EEPROM.h>
LiquidCrystal lcd(8 , 10,11, 12, 13,15); //2-RS, 3-Enable,4-D4, D5, D6, D7

/* 
*Wyjścia ATTINY 4313
 * 0 - RXD
 * 1 - TXD
 * 2 - XTAL2
 * 3 - XTAL1
 * 4 - INT0
 * 5 - PRZYCISK1 S2 Srodek
 * 6 - Przycisk2 S4 Prawy
 * 7 - Licznik do czujnika
 * 8 - RS
 * 9 - PRzycisk3 S1 LEwy
 * 10 - E
 * 11 - DB4
 * 12 - DB5
 * 13 - DB6
 * 14 - SDA
 * 15 - DB7
 * 16 - SCL
*/
#define pLewy 6     //Etykiety przycisków
#define pPrawy 9
#define pSrodek 5

int nrTrybu=0;        //Numer obecnie wybranego trybu który jest wykonywany w programie
int sumaM1=0;         //Część dzietna sumy kilometrów
int sumaM2=0;         //Suma metrów
int obwObreczy=25;    //Obwód obręczy
bool imperialne=0;    //Czy zmienić jednostki
int lmagnesow=1;      //Liczba magnesów na kole
int czasOdOst = 0;

void setup() {      //Ustawienie portów i licznika
  lcd.begin(16,2);

  pinMode(0, OUTPUT);
  //pinMode(1, INPUT);    //brak pamięcie, wejście i tak nie używane
  //pinMode(4, INPUT);
  pinMode(5, INPUT);
  pinMode(6, INPUT);
  pinMode(9, INPUT);
  pinMode(14, INPUT);
  pinMode(16, INPUT);

  digitalWrite(0, HIGH);

  TCCR1A=0;
  TCCR1B=B00000110;
  TCNT1=0;

  obwObreczy=pobierzLongZEEPROM(0,0,6273,obwObreczy);     //Pobranie wartości początkowych z EEPROM, o ile są poprawne
  imperialne = pobierzBajtZEEPROM(4,-1,2,imperialne);    
  lmagnesow=pobierzBajtZEEPROM(8, 0, 11, lmagnesow);
  
}

#define MAKS_TRYB 3   //Maksymalny numer trybu któy może zostać wybrany
#define CZASNACISKU 150 //Typowe opóźnienie dla podczas sprawdzania przycisku

void loop() {
          
    if(nrTrybu==0)                    //Przygotowanie trybu zerowego - pomiar prędkości
    {
      formatPredkoscomierza(0);
      wypiszDolnaLinie();
    }
    if(nrTrybu==1)                    //Przygotowanie trybu pierwszego - pomiar obrotów
    {
      lcd.setCursor(0,0);      
      lcd.print("Obroty:      RPM");
      wypiszDolnaLinie();
    }
    
    while(nrTrybu==0 || nrTrybu==1)   //Realizacja trybu zerowego i pierwszego
    {

      if(millis()/1000-czasOdOst >= 1) //Wypis na ekranie co 1 sekundy
      {
          char str[6];  
          //Dla 1 sek: 
          //3600(TCNT1*obwCM)/1000 => 18(TCNT1*obwCM)/5
          
          int lObrotow = 0;
          if(nrTrybu==0)                                //Pobranie i interpretacja liczby obrotów
          {
            lObrotow = TCNT1*18;
            lObrotow = lObrotow*obwObreczy/5;
            lObrotow = lObrotow/lmagnesow;
          }
          else lObrotow = TCNT1*60/lmagnesow;  

          zsumujKM(obwObreczy);
          
          TCNT1=0;
          //TCNT0=0;
          czasOdOst=millis()/1000;
          
          lcd.setCursor(7,0);
          
          if(nrTrybu==0)                      //Przeliczanie jednostek
          {
            int przelicznik = 1000;
            if(imperialne)
              przelicznik = 1610;
            
            int tmp = lObrotow/przelicznik;             //Wyświetlenie wartości
            intNaString(tmp,str);
            lcd.print(str+3);
            
            if(imperialne)
              tmp=lObrotow*1000/przelicznik;
            else tmp=lObrotow%przelicznik;
            
            intNaString(tmp,str);
            str[4]='\0';
            lcd.setCursor(10,0);
            lcd.print(str+2);
          }
          else
          {
            intNaString(lObrotow,str); //obw obreczy w cm konwersja
            lcd.print(str);
          }
           
      }
      if (zmianaTrybu() || przejscieDoOpcjiTrybu1())              //Sprawdzenie czy użytkownik nie chce zmienić trybu 
        break;                                                    //lub parametrów dzialania programu
    }

    if(nrTrybu==2)                                               //Przygotwanie trybu drugiego - miernik odległości
    {
          formatDalekosciomierza(0);
          wypiszDolnaLinie();
    }
    while(nrTrybu==2)
    {      
       if(millis()/1000-czasOdOst >= 1) //Wypis na ekranie co 1 sekundy
      {
        zsumujKM(obwObreczy);
        wyswietlKM(0);
        
        czasOdOst=millis()/1000;
        TCNT1=0;
        //TCNT0=0;
      }
      zmianaTrybu();                            //Sprawdzenie czy użytkownik nie chce zmienić trybu 
      if (przejscieDoOpcjiTrybu1())             //lub parametrów dzialania programu
        break;    
    }

    if(nrTrybu==3)
    {
      formatPredkoscomierza(0);         //Przygotowanie tryvu 3, połaczenie trybów 0 i 2
      formatDalekosciomierza(1);        // - Predkościomierz i dalekościomierz w osobnych liniach
    }
    while(nrTrybu==3)
    {
      if(millis()/1000-czasOdOst >= 1) //Wypis na ekranie co 1 sekundy
      {
        char str[6];
        int lObrotow = TCNT1*18*obwObreczy/5;
        lObrotow = lObrotow/lmagnesow;
        int czDziesietna = lObrotow;

        int przelicznik=1000;                                 //Obliczenie predkosci
        if(imperialne)
        {
          przelicznik=1610;
          czDziesietna = lObrotow*1000/przelicznik;
        }

        lcd.setCursor(7,0);                            //Wypisanie predkosci
        intNaString((lObrotow/przelicznik),str); //obw obreczy w cm konwersja
        lcd.print(str+3);
        intNaString((lObrotow),str);
        lcd.setCursor(10,0);
        str[4]='\0';
        lcd.print(str+2);

        zsumujKM(obwObreczy);                       //Wyswietlenie odleglosci
        wyswietlKM(1);

        czasOdOst=millis()/1000;
        TCNT1=0;
        //TCNT0=0;
      }
      zmianaTrybu();
      if (przejscieDoOpcjiTrybu1())
        break;
    }

}

void wyswietlKM(int linia)                          //Wysiwetlenie obliczonej sumy odległości
{
  char str[6];
  int przelicznik = 1000;
  if(imperialne)
    przelicznik = 1610;
  int l = sumaM1/przelicznik;
        intNaString(l, str);      
        lcd.setCursor(7,linia);
        lcd.print(str+3);
        lcd.setCursor(10,linia);
        l = sumaM2/przelicznik;
        intNaString(l, str);
        lcd.print(str+3);
}


bool przejscieDoOpcjiTrybu1()
{
  bool przeszlo = false;
  if(czyWcisniety(pLewy))
  {
      przeszlo = true;
      opcjeTrybu1();
  }
  return przeszlo;
}

void opcjeTrybu1()    //obrecz, jednosta, il magnesów
{
  bool wMenu=true;
  int liczba2=0;
  int liczba3=0;
  lcd.setCursor(7,0);
  lcd.print("         ");
  lcd.setCursor(0,1);
  lcd.print(F("Akceptuj   -   +"));
  lcd.setCursor(0,0);
  lcd.print("Promien(cm):");
  //int promien = obwObreczy;
  int promien = (obwObreczy*100)/628;
  if(((obwObreczy*100)%628) >= 324)
    promien++;
  promien =opcjaWMenu(1 ,999, promien);
  obwObreczy=(long)(2*314*promien)/100;
  //obwObreczy=promien;
  delay(100);
  lcd.setCursor(0,0);
  lcd.print("Typ Jednos.:");
  imperialne=opcjaWMenu(0 ,1, imperialne);
  delay(100);
  lcd.setCursor(0,0);
  lcd.print("L. Magnesow:");
  lmagnesow=opcjaWMenu(1 ,10, lmagnesow);
  lcd.setCursor(0,0);
  lcd.print(F("Zapisano        "));
  lcd.setCursor(0,1);
  lcd.print(F("Ustawienia      "));
  zapiszLongEEPROM(0,obwObreczy);
  EEPROM.write(4,imperialne);
  EEPROM.write(8,lmagnesow);
  delay(1000);
}

void zapiszLongEEPROM(int adres, long wartosc)
{
  EEPROM.write(adres, (wartosc>>24));
  EEPROM.write(adres+1, (wartosc>>16) & 0xFF);
  EEPROM.write(adres+2, (wartosc>>8) & 0xFF);
  EEPROM.write(adres+3, (wartosc) & 0xFF);
}

int pobierzLongZEEPROM(int adres, long podMin, long nadMax, long domyslna)
{
    long pobrana = 0;
    pobrana=EEPROM.read(adres);
    pobrana = (pobrana << 8);
    pobrana+=EEPROM.read(adres+1);
    pobrana = (pobrana << 8);
    pobrana+=EEPROM.read(adres+2);
    pobrana = (pobrana << 8);
    pobrana+=EEPROM.read(adres+3);

    if(podMin<pobrana && pobrana<nadMax)
      return pobrana;
    return domyslna;
}

int opcjaWMenu(int min, int max,int start)
{
    bool wMenu=true;
    int liczba=start;
  
    while(wMenu)
    {
      if(czyWcisniety(pSrodek))
      {
          liczba--;
          if(liczba<min)
            liczba=min;
      }
      
      if(czyWcisniety(pPrawy))
      {   
          liczba++;
          if(liczba>max)
            liczba=max;
      }
  
      if(czyWcisniety(pLewy))
      {
          wMenu=false;
      }
      
      lcd.setCursor(13,0);
      char str[6];
      intNaString(liczba,str);
      lcd.print(str+2);
    }

    return liczba;
}

void wypiszDolnaLinie()
{
          lcd.setCursor(0,1);
          lcd.print(F("Opcje   <    >  ")); 
}

bool czyWcisniety(int nrPrzycisku)
{
    bool wcisniety = false;
    if(digitalRead(nrPrzycisku)==LOW)
    {
        delay(CZASNACISKU);
        if(digitalRead(nrPrzycisku) == LOW)
          wcisniety=true;
    }
    return wcisniety;
}

void formatPredkoscomierza(int nrLini)
{
      lcd.setCursor(0,nrLini);      
      lcd.print(F("Pred.:   ,  KM/H"));
      if(imperialne)
      {
        lcd.setCursor(12,nrLini);      
        lcd.print(F(" MpH"));
      }
}

void formatDalekosciomierza(int nrLini)
{        
          lcd.setCursor(0,nrLini);
          lcd.print(F("Droga.:  ,    km"));     
      if(imperialne)
      {
        lcd.setCursor(14,nrLini);      
        lcd.print(F("mi"));
      }
}

void zsumujKM(int obwObreczy)
{
        int drogawCM = (TCNT1*obwObreczy);
        drogawCM = drogawCM/lmagnesow;
        sumaM1+=(drogawCM)/1000;
        sumaM2+=(drogawCM)%1000;
        int tmp= (sumaM2/1000);
        sumaM1+= tmp;
        sumaM2 = sumaM2 - tmp;  
}

bool zmianaTrybu()
{
  if(czyWcisniety(pPrawy))
  {
      nrTrybu++;
      if(nrTrybu>MAKS_TRYB)
        nrTrybu=0;
      return true;
  }
  if(czyWcisniety(pSrodek))
  {
      nrTrybu--;
      if(nrTrybu<0)
        nrTrybu=MAKS_TRYB;
      return true;
  }
  return false;
}

int pobierzBajtZEEPROM(int adres, int podMin, int nadMax, int domyslna)
{
  int tmp=EEPROM.read(adres);
    if(podMin<tmp && tmp<nadMax)
      return tmp;
    return domyslna;
}



void intNaString (int liczba, char sznur[6])
{
  int dzielnik=10000;
  for(int i=0; i<5; i++)
  {
    sznur[i]=((liczba%(dzielnik*10))/(dzielnik))+48;
    dzielnik=dzielnik/10;
  }
  sznur[5]='\0';
}
