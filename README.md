# MAME360 — SS2 save/load ve favoriler prototipi

MAME72 Release 11 kaynak ağacına mevcut MAME state API’leriyle kontrolcü kısayolları ve kalıcı favoriler ekler.

- **Oyunda:** LB+RB+Y kaydet, LB+RB+X yükle; slot 0, dört kontrolcü.
- **Oyun listesinde:** Y favori ekle/çıkar, X favori filtresi.
- **Dosyalar:** `GAME:\sta\<romname>\0.sta`, `GAME:\favorites.txt`, `GAME:\debug.log`.
- **Durum:** Save/load için kullanıcı konsol doğrulaması var; tüm oyunlar için garanti yok. Favorilerin konsol doğrulaması bekleniyor. Shinobi ses kodu değiştirilmedi.

[Derleme ve test notları](docs/BUILD-SAVESTATE-tr.md)

Temel kaynak: [mLoaDs/MAME360](https://github.com/mLoaDs/MAME360), `63a21cfce35d42333885031cf2fe221808c44016`. Aşağıdaki orijinal proje tanıtımı ve katkı sahipleri korunmuştur; tanıtımdaki genel uyumluluk iddiaları bu prototipin test garantisi değildir. Mevcut kaynak lisansları geçerlidir.

---

![380443606-7db8b933-10f1-48a5-a70e-c79f623327cb](https://github.com/user-attachments/assets/ed4078aa-2272-46c7-8320-4e892dc565fc)

# MAME360
 is a multi-arcade emulator port for XBox360

MAME0.72 is a Native port to XBox 360 by Lantus and other developers

MAME (Multiple Arcade Machine Emulator), it is a PC emulator that faithfully 
recreates how arcade systems work.

- Arcadez
- wolf3s
- Gamezfan
- iq_132
- BritneysPAIRS
- Traace


Features
========
- Easy to use interface.
- Previews.
- Perfectly emulated graphics and sounds.
- 4 player support.
- Cheat menu support.
- Hiscore.dat support.
 

Usage
=====
You need to find roms that work with MAME 0.72
By default roms go to GAME:\roms
In the mame.ini file under [Directories] you can edit it and put up to 4 paths. Supported devices are:

Usb0:
Usb1:
Usb2:
Hdd1:
Memunit0:
Memunit1:
DVD:

The Cheat menu can be enabled under [General] (in the mame.ini).

To enable it, set CheatsEnabled = 1

To enable VSync. you need to edit the mame.ini as follows:

[Video]
VSYNC = 1

Controls
========
Controls can be configured easily via the MAME menu. The default settings are:

- Back + Start to return to the menu.
- Click with the right stick to enter Configuration Mode.
- Click with the left stick for display options.

![mame1](https://github.com/user-attachments/assets/5ef70c22-0cdc-4a8f-8f5a-0121ae4745a6)

Credits and Thanks
==================
- Thanks to Artik and Logic Sunrise for their continued support.
- Some of the code is based on MameOX. Credits to the team.
