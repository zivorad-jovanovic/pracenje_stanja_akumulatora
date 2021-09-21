# PRACENJE STANJA AKUMULATORA

# Uvod
Nas zadatak je da simuliramo punjac za akumulator

## Potrebne periferije
Led bar, 7seg displej, serijski kanal

### NACIN TESTIRANJA

Postoje 2 nacina punjenja akumulatora: kontinualno i kontrolisano.
U zavisnosti od nacina punjenja sistem se drugacije testira.
#### 1.	KONTINUALNO
U kontinualnom rezimu punjenja automobil mora biti ukljucen da bi se realizovao ovaj nacin rada sistema. U tom slucaju auto se ukljucuje pritiskom na prvu diodu prvog stupca LED bara(prvi stubac treba podesiti kao ulazni). U tom slucaju ce prva dioda drugog stupca biti aktivirana i ona signalizira da je automobil ukljucen. Nakon toga potrebno je poslati komandu putem serijske veze(kanal 1) u formatu kontinu+. Izvrsenjem ove komande ukljucuje se druga dioda na drugom stupcu LED bara sto govori da je punjac ukljucen, dok treca dioda istog stupca daje znak da je rezim punjenja KONTINUALAN(naponsko punjenje). Ponovnim pritiskom na taster(ulazna dioda) automobil se iskljucuje, a samim tim i punjac sto znaci da su sve izlazne diode iskljucene. 
#### 2.	KONTROLISANO
U kontrolisanom rezimu punjenja autombil se puni nezavisno od toga da li je ukljucen. Da bi se izvrsila ova komanda, potrebno je poslati naredbu u formatu kontrol+ putem serijske veze(kanal 1). Nakon toga se prati stanje akumulatora. Simulacija vrednosti napona na akumulatoru se vrsi manuelnim slanjem vrednosti u opsegu 0 – 1023 putem kanala 0  pomocu serijske veze.
- Ako se posalje vrednost manja od 12.5 V ukljucuju se druga i cetvrta dioda, gde druga dioda pokazuje da je ukljucen punac, a cetvrta da je ukljuceno strujno punjenje. Prva dioda moze a i ne mora biti aktivirana jer signalizira da je automobil ukljucen.
-Ako se posalje vrednost veca od 13.5 V aktivira se naponsko punjenje(tada se cetvrta dida iskljuci a treca ukljuci).
-Ako se posalje vrednost veca od 14 V iskljucuje se punjac a samim tim se iskljucuje dioda pokazuje rezim punjenja(jedino moze ostati ukljucena prva dioda u slucaju da je automobil ostao ukljucen).

#### KANAL 0
Slanje vrednosti akumulatora se vrsi koriscenjem auto opcije na kanalu 0 i slanjem triger signala u obliku slova ‘T’ svakih 100ms cime se dobijaju vrednosti kao izlaz AD konvertora u formatu cetiri cifre( 0-1023) gde svaka vrednost se mora slati sa cetiri cifre(primer: 0 se salje kao 0000, 500 kao 0500 itd.).

#### KANAL 1
Rezimi punjenja akumulatora kao i minimalna i maksimalna vrednost AD konvertora se zadaju putem kanala 1. 
Slanjem komande adminXX+ se zadaje minimalni napon.
Slanjem komande admaxXX+ se zadaje maksimalni napon.
Prethodno je objasnjeno slanje komandi kojima se odredjuje nacin punjenja(kontinualno i kontrolisano).
U prijemnom delu kanala odnosno u terminalu se ispisuju vrednost napona akumulatora i rezim punjenja. 

#### 7SEG DISPLAY
Na prve dve cifre displeja se ispisuje trenutna vrednost napona akumulatora. Na sledece dve minimalna vredsnot napona koja je zadata, a na sledece dve maksimalna vrednost.

 
