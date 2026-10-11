# SS2 save/load ve favoriler — teknik notlar

Temel: mLoaDs/MAME360 `63a21cfce35d42333885031cf2fe221808c44016`.

## Durum

Save/load ve favori listesi konsolda test edildi, çalıştığı görüldü.  

## Xbox derlemesi

1. VS2010 SP1 C++ araçları ve uyumlu Xbox 360 XDK kurulu olmalıdır; `XEDK` SDK kökünü göstermelidir.
2. Depodaki Xbox projesini kendi kurulumunuza göre hazırlayın. ATG Framework proje referansı kaynak projede eski/makineye özel bir konuma işaret edebilir; kendi XDK'nızın uyumlu ATG örnek projesine veya yerel kopyasına yönlendirin.
3. `Release_LTCG | Xbox 360` yapılandırmasını derleyin. Otomatik konsola aktarım istemiyorsanız ilgili `Deploy` grubunda `ExcludedFromBuild=true` kullanın. Konsol bağlantı hatasını derleme hatasıyla karıştırmayın.
4. Beklenen ana çıktı `mame_0.72/Release_LTCG/mame.xex` dosyasıdır. Denemede bu dosya `mame-ss2-favorites.xex` adıyla ayrıldı.
5. Mevcut XEX'yi yedekleyin; yeni dosyayı mevcut media/config/ROM yapısıyla başlatın. `SS2+Fav` ve oyun açılışındaki `SS2 ready` mesajları sürümü ayırt eder.

Geliştirmede kurulu MSBuild 14, VS2010 `VCTargetsPath` ve XDK araçları kullanıldı. Yerel `savestate-build.vcxproj` ATG referansını yerel kopyaya yönlendiriyordu. Bu yardımcı proje makineye özel olduğu için commit edilmez. Bu belge sıfırdan kurulmuş her XDK sürümüyle tekrarlanmış bir derleme garantisi değildir.

## Kullanım

- LB+RB+Y: kaydet; LB+RB+X: yükle. Dört kontrolcü portu, tek slot 0.
- `GAME:\sta\<romname>\0.sta`; log `GAME:\debug.log`.
- Liste ekranında Y: favori ekle/çıkar; X: filtre.
- Favoriler `GAME:\favorites.txt`, geçici yazma dosyası `GAME:\favorites.tmp`.
- `GAME:` yazılabilir olmalı. Kombinasyonu yeni işlemden önce bırakın.

## Değişen emülatör dosyaları

| Dosya (`mame_0.72/src` altında) | Değişiklik |
|---|---|
| `cpuexec.c` | Kare başına istek, otomatik ROM/slot adı, mevcut zamanlayıcı, sonuç logları. |
| `osdepend.h` | Xbox kontrolcü sorgusu bildirimi. |
| `state.c`, `state.h` | Mevcut biçimi koruyan hata/uzunluk/başlık kontrolleri ve save başarı dönüşü. |
| `xbox/xbox_JoystickMouse.c` | Port başına kenar algılama ve çakışan istekleri tüketme. |
| `xbox/xbox_FileIO.cpp` | Ayrı state kökü, dizin hatası düzeltmesi, yazma/flush ve hata logları. |
| `xbox/xbox_Miscelaneous.c` | Release logları, sınırlı uzunlukta MAME ekran mesajı. |
| `xbox/RomList.cpp`, `xbox/RomList.h` | Favori saklama/filtre, sürücü kimliğiyle seçim, boş liste/metin sınırı kontrolleri. |

## Bilgisayar testleri

Python 3, GCC ve G++ PATH üzerinde olmalıdır. Testler emülatör ROM'u gerektirmez; mevcut zlib kaynaklarını kullanır. Depo kökünde:

```text
python tests/verify_ss2.py
python tests/verify_favorites.py
```

Üretilen test dosyaları `tests/generated/` altına yazılır. Eski serializer karşılaştırması yukarıdaki sabit temel commit'i kullanır. Donanım ve Windows/XUI çağrılarının bir kısmı taklittir; testler XDK derlemesi ve Xbox'ta oynanış testi yerine geçmez. Bu testler geliştirme bilgisayarında MinGW GCC/G++ ile çalıştırıldı.

## Geri alma

Emülatör prototipi ayrı bir commit olarak tutulur. Sonraki kişisel değişiklikleri koruyarak bu commit'i `git revert <commit>` ile geri alabilirsiniz. `.sta` ve `favorites.txt` dosyaları kaynak koddan bağımsızdır. Eski XEX'ye dönmek için yedeklediğiniz ikiliyi kullanın.

Kayıtlı alanların kapsamadığı cihaz/sürücü durumları nedeniyle oyun uyumluluğu ayrı test edilir. State yazması atomik değildir; slot bozulabilir. Aynı oyun/derleme/ayarlar tavsiye edilir. Bootleg Shinobi ses sorunu bu değişiklikle giderilmiş değildir.
