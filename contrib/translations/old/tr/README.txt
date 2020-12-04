DOSBox Sürüm 0.74 Klavuzu (her zaman www.dosbox.com'dan en son sürümü kullanın)



====
NOT:
====

Biz, bir gün DOSBox'un şimdiye kadar PC için yapılan bütün programları
çalıştırmayı umut ederken, henüz orada değiliz.
Şu anda, DOSBox high-end makinede kabaca bir Pentium I PC eşdeğeri olarak
çalışıyor. DOSBox CGA/Tandy/PCjr klasiklerinden Quake döneminden oyunlara kadar
çeşitli DOS oyunlarını çalıştırmak için ayarlanabilir.



======
DİZİN:
======

1. Hızlı Başlangıç
2. Başlangıç (SSS)
3. Komut Satırı Parametreleri
4. İç Programlar
5. Özel Tuşlar
6. Oyun Çubuğu/Oyun Kolu
7. Tuş Eşleştiricisi
8. Klavye Düzeni
9. Seri Çok Oyuncu Özelliği
10. DOSBox nasıl hızlandırılır/yavaşlatılır
11. Sorun Giderme
12. DOSBox Durum Penceresi
13. Yapılandırma (seçenekler) dosyası
14. Dil dosyası
15. DOSBox'un kendi sürümünzü oluşturmak
16. Özel teşekkürler
17. İletişim



===================
1. Hızlı Başlangıç:
===================

DOSBox'ta hızlı bir tur için INTRO yazın.
Bağlama kavramı ile bilmeniz gereken, DOSBox benzetimde erişilebilen
kendiliğinden hiç sürücü (ya da bunun bir parçası) sağlamaz. SSS'da "Nasıl
başlanır?" maddesi ile birlikte MOUNT komutunun açıklamasına bakın.
(bölüm 4: "İç Programlar"). Bir CD'de oyun varsa bu klavuzu deneyin:
http://vogons.zetafleet.com/viewtopic.php?t=8933



===============
2. Start (FAQ):
===============

BAŞLANGIÇ:  Nasıl başlanır?
ÖZDEVİN:    Her zaman "mount" komutları yazmak zorunda mıyım?
TAM EKRAN:  Nasıl tam ekrana geçebilirim?
CD-ROM:     CD-ROM'um çalışmıyor.
CD-ROM:     Oyun/uygulama kendi CD-ROM'unu bulamıyor.
FARE:       Fare çalışmıyor.
SES:        Ses yok.
SES:        DOSBox'un şu anda benzetimini yaptığı ses donanımı nedir?
SES:        Ses takılıyor ya da ses gergin/tuhaf.
KLAVYE:     DOSBox'ta \ ya da : yazamıyorum.
KLAVYE:     DOSBox'ta sol üst karakter ile "\" çalışmıyor. (yalnızca Windows)
KLAVYE:     Klavye gecikiyor.
DENETİM:    Karakter/imleç/fare işaretçisi hep bir yöne ilerliyor!
HIZ:        Oyun/uygulama çok yavaş/hızlı çalışıyor!
ÇÖKME:      Oyun/uygulama hiç çalışmaz/çöküyor!
ÇÖKME:      DOSBox başlangıçta çöküyor!
OYUN:       Benim yapılı oyunumun(Duke3D/Blood/Shadow Warrior) sorunları var.
GÜVENLİK:   DOSBox benim bilgisayarıma zarar verir mi?
SEÇENEKLER: DOSBox'un seçeneklerini değiştirmek istiyorum.
YARDIM:     İyi klavuz, ama ben yine de anlamadım.



BAŞLANGIÇ: Nasıl başlanır?
    Başlangıçta istemcide C:\> yerine Z:\> vardır.
    DOSBox'ta "mount" komutunu kullanarak, dizinlerinizi, sürücüler olarak
    kullanılabilir yapmak zorundasınız. Örneğin, Windows'ta "mount C D:\GAMES",
    size DOSBox'ta Windows'taki D:\GAMES (önceden oluşturulmuş) dizinizi işaret
    eden bir C sürücüsü verecektir. Linux'te, "mount c /home/kullanıcıismi",
    size DOSBox'ta Linux'teki /home/kullanıcıismi'ni işaret eden bir C sürücüsü
    verecektir. Yukarıdaki gibi bağlanmış sürücüye geçmek için, "C:" yazın.
    Herşey iyi gitti ise, DOSBox fine, "C:\>" istemcisini görüntüler.


ÖZDEVİN: Her zaman bu komutları yazmak zorunda mıyım?
    DOSBox yapılandırma dosyasında [autoexec] bölümü vardır. DOSBox başladığında
    orada bulunan komutlar çalıştırılır, yani bu bölümü bağlama için
    kullanabilirsiniz. Bölüm 13'e bakın: Yapılandırma (seçenekler) dosyası


TAM EKRAN: Nasıl tam ekrana geçebilirim?
    Alt-Enter'e basarak. Alternatif olarak: DOSBox'un yapılandırma dosyasını
    düzenleyin ve fullscreen=false seçeneğini fullscreen=true'ya değiştirin.
    Tam ekran sizce yanlış görünüyorsa: Seçeneklerle oynayın: DOSBox
    yapılandırma dosyasında fullresolution, output ile aspect. Tam ekran
    kipinden çıkmak için: Yeniden Alt-Enter'e basın.


CD-ROM: CD-ROM'um çalışmıyor.
    DOSBox'ta CD-ROM'unuzu bağlamak için CD-ROM bağlarken bazı ek seçenekleri
    belirtmek zorundasınız.
    Windows'ta CD-ROM desteğini (MSCDEX içerir) etkinleştirmek için:
      - mount d f:\ -t cdrom
    Linux'ta:
      - mount d /media/cdrom -t cdrom

    Bazı durumlarda farklı CD-ROM arabirimi kullanmak isteyebilirsiniz,
    örneğin ses CD'si çalışmıyorsa:
      SDL desteğini etkinleştirin (alt seviye CD-ROM erişimini içermez!):
        - mount d f:\ -t cdrom -usecd 0 -noioctl
      Ses CD'si için sayısal ses alma kullanarak ioctl erişim sağlamak için
      (windows-only, useful for Vista):
        - mount d f:\ -t cdrom -ioctl_dx
      Ses CD'si için MCI kullanarak erişim sağlamak için (yalnızca Windows):
        - mount d f:\ -t cdrom -ioctl_mci
      Yalnızca ioctl erişimine zorlamak için (yalnızca Windows):
        - mount d f:\ -t cdrom -ioctl_dio
      Alt seviye aspi desteğini etkinleştirmek için (win98 ile birlikte aspi
      katmanı yüklü):
        - mount d f:\ -t cdrom -aspi

       açıklama: - d   DOSBox'ta alacağınız sürücü harfi (en iyisi d,
                          bunu değiştirmeyin)
                 - f:\ Bilgisayarında CD-ROM'unuzun konumu. Çoğu durumda d:\
                          ya da e:\ olur.
                 - 0   "mount -cd" tarafından bildirilen CD-ROM sürücünüzün
                          numarası, (dikkat edin, bu değer ses CD'si için
                          yalnızca SDL kullanıldığınızda gereksinim
                          duyarsınız, bunun dışında görmezden gelinir)
    Ayrıca, bir sonraki soruya bakın: Oyun/uygulama kendi CD-ROM'unu bulamıyor.


CD-ROM: Oyun/uygulama kendi CD-ROM'unu bulamıyor.
    CD-ROM'u -t cdrom seçeneği ile birlikte bağladığınıza emin olun, bu
    CD-ROM'ların arayüzü için DOS oyunları için gerekli MSCDEX arayüzünü
    etkinleştirecektir. Ayrıca, mount komutuna tam etiketini (-label ETİKET)
    ekleyerek deneyin, burada LABEL CD-ROM'un CD etiketidir (birim kimliği).
    Windows altında -ioctl, -aspi ya da -noioctl belirtebilirsiniz. Onların
	anlamları ile ek ses CD ilgili seçenekler -ioctl_dx, ioctl_mci, ioctl_dio
	için Ek Bölüm 4: "İç Programlar"'da mount komutunun açıklamasına bakınız.

    CD-ROM kalıbı oluşturmayı (tercihen CUE/BIN) ve DOSBox'un kalıp bağlamak 
    (CUE sayfası) için iç IMGMOUNT aracını kullanın. Bu, her işletim sisteminde
    çok iyi CD-ROM desteğini etkinleştirir.


FARE: Fare çalışmıyor.
    Çoğunlukla, DOSBox bir oyun fare denetimi kullandığında algılar. Ekrana
    tıkladığınızda kilitlenmeli (DOSBox penceresinde sınırlanmış) ve çalışmalı.
    Bazı oyunlarda, DOSBox fare algılaması çalışmaz. Bu durumda siz, CTRL-F10'a
    basarak fareyi el ile kilitlemek zorundasınız.


SES: Ses yok.
    Sesin doğru olarak yapılandırıldığından emin olun. Bu, kurulum sırasında
    yapılır ya da bir kurulum/ses ayarı aracı oyunla birliktedir. Önce
    kendiliğinden algılama seçeneğinin sağlandığına bakın. Yoksa
    Soundblaster ya da Soundblaster 16 varsayılan ayarlar olan "address=220
    irq=7 dma=1" (ara sıra highdma=5) ile birlikte seçmeyi deneyin. Ayrıca
    müzik aygıtı olarak Sound Canvas/SCC/MPU-401/General MIDI/Wave Blaster
    "address=330 IRQ=2"'da seçmek isteyebilirsiniz. Benzetimi yapılan ses
    kartlarının parametreleri DOSBox yapılandırma dosyasında değiştirilebilir.
    Yine de ses almıyorsanız DOSBox yapılandırmasında core'yi normal
    olarak ayarlayın ve biraz düşük sabit çevrim değeri (örneğin cycles=2000)
    kullanın. Ayrıca ana çalışma sesinin sesi sağladığını temin edin.
    Bazı durumlarda benzetimi yapılan farklı ses aygıtının örneğin
    soundblaster pro (DOSBox yapılandırma dosyasında sbtype=sbpro1) ya da
    gravis ultrasound (gus=true) kullanılması kullanışlı olabilir.


SES: DOSBox'un şu anda benzetimini yaptığı ses donanımı nedir?
    DOSBox birkaç eski ses aygıtının benzetimini yapar:
    - İç sistem hoparlörü/Buzzer
      Bu benzetimde ton üreteci ile iç hoparlörden birkaç tür sayısal ses çıkış
      biçimini içerir.
    - Creative CMS/Gameblaster
      Creative Labs(R) tarafından sürülen ilk karttır. Varsayılan yapılandırma
      yeri 220 adresindedir. Bu, varsayılan olarak devre dışıdır.
    - Tandy 3 voice
      Bu ses donanımının benzetimi gürültü kanalı dışında tamdır. Gürültü
      kanalı çok iyi belgelendirilmemiş ve bu nedenle sesin doğruluğu olarak
      en iyi tahmindir. Bu, varsayılan olarak devre dışıdır.
    - Tandy DAC
      Bazı oyunlar, daha iyi tandy DAC ses desteği için sound blaster
      benzetimini (sbtype=none) kapatmayı gerektirir. Tandy sesi
      kullanmıyorsanız, sbtype'yi yeniden sb16'ya ayarlamayı unutmayın.
    - Adlib
      Bu benzetim neredeyse mükkemmel ve yaklaşık olarak sayısallaştırılmış ses
      çalmak için Adlib'in yeteneğini içerir. 220 adresine yerleştirilmiştir
      (388 adresinde de).
    - SoundBlaster 16 / SoundBlaster Pro I & II / SoundBlaster I & II
      DOSBox varsayılan olarak Soundblaster 16 seviyesinde 16 bit stereo ses
      sağlar. DOSBox yapılandırmasında farklı SoundBlaster sürümlerini
      seçebilirsiniz. AWE32 müzik benzetimi yapılmıyor, onun yerine MPU-401
      kullanabilirsiniz (aşağıya bakın).
    - Disney Sound Source and Covox Speech Thing
      Yazıcı bağlantı noktasını kullanır, bu ses aygıtı yalnızca sayısal ses
      çıkışı verir. LPT1'e yerleştirilmiştir.
    - Gravis Ultrasound
      Bu donanımın benzetimi tama yakındır, ama MPU-401 diğer kodda benzetimini
      yaptığından beri, MIDI özellikleri atlanmıştır. Ayrıca, Gravis müzik için
      DOSBox içinde Gravis sürücülerini kurmak zorundasınız. Bu, varsayılan
      olarak devre dışıdır.
    - MPU-401
      Ayrıca, MIDI geçiş arayüzünün benzetimi yapılır. Bu ses çıkışı yöntemi
      yalnızca dış aygıt/benzetici ile birlikte kullanılırsa çalışacaktır.
      Her Windows XP/Vista/7 ile MAC OS varsayılan benzetim uyumluluğu ile
      birlikte: Sound Canvas/SCC/General Standard/General MIDI/Wave Blaster.
      Roland LAPC/CM-32L/MT-32 uyumluluğu için farklı bir aygıt/bezetici
      gereklidir.


SES: Ses takılıyor ya da ses gergin/tuhaf.
    DOSBox'un mevcut hızda çalışmasını sağlamak için çok fazla CPU gücü
    kullanıyor olabilirsiniz. Çevrimi düşürebilirsiniz, çerçeve  skip frames, reduce the sampling rate of
    atlayabilirsiniz, kendi ses aygıtınızın örnekleme oranını azaltabilirsiniz,
    önbelleği arttırabilirsiniz. Bölüm 13'e bakın: "Yapılandırma (seçenekler)
    dosyası"
    cycles=max ya da =auto kullanırsanız, arka plan işlemlerinin
    karışmadığına emin olunuz! (özellikle sabit diske erişiyorlarsa)
    Ayrıca Bölüm 10'a bakın: "DOSBox'ta nasıl hız arttırılır/aşağıya düşürülür"


KLAVYE: DOSBox'ta \ ya da : yazamıyorum.
    Bu, çeşitli durumlarda olabilir, kendi ana klavye düzeniniz gibi eşleşen düzenini
    DOS düzeni temsili yok (ya da doğru algılanmadı), ya da tuş eşleştiriciniz
    yanlış.
    Bazı olası düzeltmeler::
      1. Yerine / kullanın ya da : için ALT-58 ile \ için ALT-92.
      2. DOS klavye düzeninizi değiştirin (Bölüm 8'e bakın: Klavye Düzeni).
      3. Çalıştırmak istediğiniz komutları DOSBox yapılandırma dosyasında
         [autoexec] bölümüne ekleyin.of the DOSBox configuration file.
      4. DOSBox yapılandırma dosyasını açın ve usescancodes girişini
      değiştirin.
      5. Switch the keyboard layout of your operating system.

    Dikkat edin, ana düzeniniz tanımlanamıyorsa ya da DOSBox yapılandırma
    dosyasında keyboardlayout hiçbirine ayarlanmamışsa, standart ABD düzeni
    kullanılır. Bu yapılandırmada \ (ters bölü işareti) tuşu için "yeni
    satır"'ın yanında olan tuşları deneyin ve : (iki nokta) tuşu için üst
    karakter ile "yeni satır" ve "L" tuşlarının arasında olan tuşları kullanın.


KLAVYE: DOSBox'ta Sağ Üst Karakter ile "\" çalışmıyor. (yalnızca Windows)
    Bu, bazı uzaktan kumanda aygıtlarını kullandığınızda Windows,
    bilgisayarınıza birden çok klavye bağlı olduğunu sanırsa bu olabilir.
    Bu sorunu doğrulamak için  cmd.exe'yi çalıştırın, DOSBox program dizinine
    gidin ve yazın:
    set sdl_videodriver=windib
    dosbox.exe
    klavyenin düzgün çalışmaya başladığını denetleyin. windib yavaş olduğu için
    en iyisi iki çözümden birini burada sağlanan iki çözümden birini kullanın:
    http://vogons.zetafleet.com/viewtopic.php?t=24072


KLAVYE: Klavye takılıyor.
    DOSBox yapılandırma dosyasında öncelik ayarı düşük, örneğin
    "priority=normal,normal" ayarlı. Daha düşük çevirim hızı denemeyiYou might also want to try lowering the
    isteyebilirsiniz (Başlangıç için sabit çevirim değeri kullanın, örneğin cycles=10000).


DENETİM: Karakter/imleç/fare işaretçisi hep bir yöne ilerliyor!
    Yine oluyorsa joystick benzetimini devre dışı bırakmayı bakın,
    DOSBox'unuzun yapılandırma dosyasında [joystick] bölümünde 
    joysticktype=none olarak ayarlayın. Belki de herhangi bir oyun çubuğu/oyun kolu sökülmüştür.
    Oyunda joystick kullanmak istiyorsanız, timed=false olarak ayarlayın
    ve joystickin ayarlandığına emin olun (hem işletim sisteminizde hem de
    oyun ya da oyunun kurulum programında).


HIZ: Oyun/uygulama çok yavaş/hızlı çalışıyor!
    Daha çok bilgi için bölüm 10'a bakın: "DOSBox nasıl hızlandırılır/
	yavaşlatılır"


ÇÖKME: Oyun/uygulama hiç çalışmaz/çöküyor!
    Bölüm 11'e bakın: Sorun Giderme


ÇÖKME: DOSBox açılışta çöküyor!
    Bölüm 11'e bakın: Sorun Giderme


OYUN: Benim yapılı oyunumun(Duke3D/Blood/Shadow Warrior) sorunları var.
    Herşeyden önce, oyunun bir portunu bulmayı çalışın. Bu, daha iyi bir 
    deneyim sunacaktır. DOSBox'ta yüksek çözünürlüklerde ortaya çıkan grafik
    sorununu çözmek için: DOSBox'un yapılandırma dosyasını açın ve
    machine=svga_s3'ü arayın. svga_s3'ten vesa_nolfb'yi değiştirin.
    memsize=16'yı memsize=63'e değiştirin.


GÜVENLİK: DOSBox benim bilgisayarıma zarar verir mi?
    DOSBox, diğer kaynak isteyen programdan daha çok zarar veremez. Çevrimi
    arttırmak, gerçek CPU'nuzu hız aşırtmak değildir. Çevirimi çok yükseğe
    ayarlamak DOSBox içinde çalışan yazılıma olumsuz başarım etkisi yapar.


SEÇENEKLER: DOSBox'un seçeneklerini düzenlemek istiyorum..
    Bölüm 13'e bakın: "Yapılandırma (seçenekler) dosyası"


HELP: İyi klavuz, ama ben yine de anlamadım.
    Daha çok soru için bu klavuzun geri kalanını okuyun. Ayrıca, bakabilirsiniz:
    klavuzlar http://vogons.zetafleet.com/viewforum.php?f=39 'da yerleşmiştir.
    DOSBox'un wikisi http://www.dosbox.com/wiki/ için
    Site/forum: http://www.dosbox.com



==============================
3. Komut Satırı Parametreleri:
==============================

DOSBox'a verebileceğiniz komut satırı seçeneklerine genel bakış. Çoğu durumda
bunun yerine DOSBox'un yapılandırma dosyasını kullanmak daha kolay olmasına karşın.
Bakınız: Bölüm 13. "Yapılandırma (Seçenekler) Dosyası"

Komut satırı parametrelerini kullanmak için:
(Windows)  cmd.exe'yi ya da command.com'u açın ya da kısayol dosbox.exe'yi
           düzenleyin
(Linux)    konsolu kullanın
(MAC OS X) start terminal.app'ı başlatın ve 
           /applications/dosbox.app/contents/macos/dosbox'a gidin.

Seçenekler açıklamasında belirtilmediği sürece bu seçenekler, bütün işletim
sistemleri için geçerlidir:

dosbox [isim] [-exit] [-c komut] [-fullscreen] [-userconf]
       [-conf yapılandırma_dosyası_konumu] [-lang dil_dosyası_konumu]
       [-machine makine_türü] [-noconsole] [-startmapper] [-noautoexec]
       [-securemode] [-scaler ölçekleyici | -forcescaler ölçekleyici] [-version]
       [-socket soket]
       
dosbox -version
dosbox -editconf program
dosbox -opencaptures program
dosbox -printconf
dosbox -eraseconf
dosbox -erasemapper

  name
        "isim" bir dizin ise C: sürücüsü olarak bağlanır.
        "isim" bir çalıştırılabilir ise "isim" dizini C: sürücüsü olarak
        bağlanır ve "isim"'i çalıştırır.

  -exit
        DOSBox, "isim" DOS uygulamas bittiğinde kendi kendine kapanacaktır.

  -c command
        "isim" çalıştırılmadan önce belirtilen komutu çalıştırır. Birden çok
        komut belirtilebilir. Fakat, her komut "-c" ile başlamalıdır.
        Bir komut: bir iç program, bir DOS komutu ya da bağlanmış sürücüde bir
        çalıştırılabilir dosya olabilir.

  -fullscreen
        DOSBox'u tam ekran kipinde başlatır.

  -userconf
        DOSBox'u kullanıcılarla özel yapılandırma dosyası ile başlatır. Birden
        çok -conf parametresi ile birlikte belirtebilirsiniz, ama -userconf
        her zaman onlardan önce yüklenir.

  -conf yapılandırma_dosyasın_konumu
        DOSBox'u "yapılandırma_dosyası_konumu" ile belirtilen seçeneklerle
        başlatır. Birden çok -conf seçenekleri mevcut olabilir.
        Daha çok ayrıntı için Bölüm 13'e bakınız.

  -lang dil_dosyası_konumu
        DOSBox'u "dil_dosyası_konumu" ile belirtilen dili kullanarak başlatır.
        Daha çok ayrıntı için Bölüm 14'e bakınız.

  -machine machinetype
        DOSBox'u belirli makine türünün benzetimini yapmak için kurar. Geçerli
        seçenekler: hercules, cga, ega, pcjr, tandy, svga_s3 (varsayılan)
        hem de DOSBox yapılandırma dosyasında listelenmiş ek SVGA yonga
        setleri.
        svga_s3 vesa benzetimini de etkinleştirir.
        Bazi belirli VGA etkileri için makine türü vgaonly kullanabilir,
        dikkat edin, bu SVGA yeteneklerini ve daha yüksek benzetim 
        duyarlılığında yavaş olabilir.
        machinetype, video kartına ile mevcut ses kartlarını etkiler.

  -noconsole (Yalnızca Windows)
        DOSBox'u DOSBox Durum Penceresi (konsol) görüntülenmeden başlatır.
        Çıktı stdout.txt ile stderr.txt'ye yönlendirilir.

  -startmapper
        Başlangıçta tuş eşleştiricisine gidilir. Klavye sorunları olan insanlar
        için kullanışlıdır.

  -noautoexec
        Yapılandırma dosyasında yüklenen [autoexec] bölümünü atlar.

  -securemode
        -noautoexec ile aynıdır, ama AUTOEXEC.BAT'ın sonuna
        config.com -securemode ekler. (bir de DOSBox içinde sürücülerin nasıl
        bağlandığına karşı her değişikliklikleri devre dışı bırakır.)

  -scaler Ölçekleyici
        "Ölçekleyici" ile belirlenmiş ölçekleyiciyi kullanır. Mevcut
        ölçekleyiciler için DOSBox yapılandırma dosyasına bakınız.

  -forcescaler Ölçekleyici
        -scaler parametresi ile benzerdir, ama uygun olmayabiliyorsa bile
        belirtilen ölçekleyiciyi kullanıma zorlar.

  -version
        Sürüm bilgisini görüntüler ve çıkar. Kullanıcı arayüzü için
        kullanışlıdır.

  -editconf program
        program'ı ilk parametresi yapılandırma dosyası olarak çağırır.
        Birden çok komut belirtebilirsiniz. Bu durumda birincisi başlatma
        başarısız ise ikinci programa geçer.

  -opencaptures program
        program'ı ilk parametresi olarak yakalama dizininin konumu ile çağırır.

  -printconf
        Varsayılan yapılandırma dosyasının konumunu yazdırır.

  -resetconf
        Varsayılan yapılandırma dosyasını siler.

  -resetmapper
        Varsayılan saf yapılandırma dosyasını kullanarak tuş eşleştiricisi
        dosyasını siler.

  -socket
        Kukla modem benzetimi için soket numarasını geçirir. Bölüm 9:
        "Seri Çok Oyuncu Özelliği"'ne bakınız:

Not:  Bir isim/komut/yapılandırma_dosyası_konumu/dil_dosyası_konumu bir
     boşluk içeriyorsa, ismin/komutun/yapılandırma_dosyası_konumu/
     dil_dosyası_konumu tamamını tırnak işaretleri arasına koyun ("komut
     ya da dosya ismi"). Çift tırnak işaretleri içinde tırnak işareti
     kullanmaya ihtiyaç duyarsanız (büyük bir olasılıkla -c ve mount ile
     birlikte):Windows ile OS/2 kullanıcıları çift tırnak işaretleri içine tek
     tırnak işareti kullanabilirsiniz. Diğer herkes çift tırnak arasında çift
     tırnak kaçış karakteri kullanabilir.
     Windows: -c "mount c 'c:\DOS oyunları dizinim\'"
     Linux: -c "mount c \"/tmp/boşluk ile isim\""

Oldukça sıradışı bir örnek, yalnızca neler yapabileceğinizi göstermek için (Windows):
dosbox D:\dizin\dosya.exe -c "MOUNT Y H:\Dizinim"
  Bu D:\dizin'i C:\ olarak bağlar ve dosya.exe'yi çalıştırır.
  Bunu yapmadan önce, H:\Dizinim'i Y sürücüsü olarak bağlar.

Windows'ta, ayrıca DOSBox çalıştırılabilir dosyanın üzerine dizinleri/dosyaları
sürükleyebilirsiniz.



=================
4. İç Programlar:
=================

DOSBox command.com'da bulunan çoğu DOS komutunu destekler.
İç komutların bir listesini almak için istemciye "HELP" yazın.

Ek olarak, aşağıdaki komutlarda mevcuttur:

MOUNT "Benzetimi yapılan sürücü harfi" "Gerçek sürücü ya da dizin"
      [-t tür] [-aspi] [-ioctl] [-noioctl] [-usecd sayı] [-size sürücü_boyutu]
      [-label sürücü_etiketi] [-freesize MB_cinsindem_boyutu]
      [-freesize KB_cinsinden_boyutu (disketler)]
MOUNT -cd
MOUNT -u "Benzetimi yapılan sürücü harfi"

  DOSBox içerisine yerel dizinleri sürücü olarak bağlayan programdır.

  "Benzetimi yapılan sürücü harfi"
        DOSBox içerisinde sürücü harfi (örneğin C).

  "Gerçek sürücü harfi(çoğunlukla Windows'ta CD-ROM'lar için) ya da dizin"
        DOSBox içerisinde erişmek istediğiniz yerel dizin.

  -t tür
        Bağlanmış dizinin türü.
        Desteklenen: dir (varsayılan), floppy, cdrom.

  -size sürücü_boyutu
    (yalnızca uzmanlar)
        "bps,spc,tcl,fcl" biçiminde olan sürücünün boyutunu ayarlar:
           bps: sektör başına bayt, sıradan sürücüelr için 512 ve CD-ROM
                sürücüleri için 2048
           spc: silindir başına sektör, çoğunlukla 1 ile 127 arasında
           tcl: toplam silindir, 1 ile 65534 arasında
           fcl: toplam boş silindir, 1 ile tcl arasında

  -freesize MB_cinsinden_boşluk | KB_cinsinden_boyut
        Sürücüde bulunan boş alanı megabayt (sıradan sürücüler) ya da kilobayt
        (disket sürücüleri) cinsinden ayarlar.
        Bu, -size'nin daha basit sürümüdür.

  -label sürücü_etiketi
        Sürücünün etiketini "sürücü_etiketi" olarak ayarlar. Bazı sistemlerde Needed on some systems
        CD-ROM etiketi doğru okunmuyorsa (program kendi CD-ROM'unu bulamazsa
        kullanışlıdır) gereklidir. Bir etiket belirtmezseniz ve alt düzey
        desteği seçilmezse (-usecd # ya da -aspi parametreleri koymadığınızda
        ya da -noioctl belirtiğinizde):
          Windows için: Gerçek sürücüden alınan etiket.
          For Linux: Etiket NO_LABEL olarak ayarlanır.

        Etiket belirtirseniz, bu etiket sürücü bağlı olduğu sürece
        korunacaktır. Bu güncellenmeyecektir !!

  -aspi
        ASPI katmanını kullanmaya zorlar. Yalnızca, Windows sistemi ile
        birlikte ASPI katmanı altında CD-ROM bağlanacaksa geçerlidir.

  -ioctl (Kendiliğinden ses CD'si arayüzü seçimi)
  -ioctl_dx (Ses CD'si için sayısal ses elde edilimi)
  -ioctl_dio (Ses CD'si için ioctl çağrısı kullanma)
  -ioctl_mci (Ses CD'si için MCI kullanma)
        ioctl komutlarını kullanmaya zorlar. Yalnızca onlara destek veren
        Windows işletim sistemlerinde CD-ROM bağlanacaksa geçerlidir.
        (Win2000/XP/NT)
        Çeşitli seçimler yalnızca ses CD'si işlendiği şeklinde farklı olur,
        öncelikli olarak -ioctl_dio kullanılır (en düşük iş yükü), ama bu bütün
        sistemlerde çalışmayabilir, bu yüzden -ioctl_dx (ya da -ioctl_mci)
        kullanılır.

  -noioctl
        SDL CD-ROM katmanını kullanmaya zorlar. Bütün sistemlerde geçerlidir.

  -usecd sayı
        Bütün sistemlerde geçerlidir, Windows altında -usecd seçeneğini
        kullanmak için -noioctl seçeneği olması zorunludur.
        SDL tarafından kullanılan sürücüleri etkinleştirir. Bunu, SDL CD-ROM
        arayüzü kullandığınızda yanlış ya da hiç CD-ROM sürücüsü bağlanmamışsa
        kullanın. "sayı", "MOUNT -cd" tarafından bulunabilir.

  -cd
        SDL tarafından algılanan bütün CD-ROM sürücülerini ile onların
        numaralarını görüntüler. Yukarıdaki -usecd maddesindeki bilgiye bakın.

  -u
        Bağı kaldırır. Z:\ için çalışmaz.

  Not: Bir yerel dizini CD-ROM sürücüsü olarak bağlamak olanaklıdır, ama
       donanım desteği eksiktir.

  Aslında MOUNT DOSBox'un benzetimini yaptığı bilgisayar için gerçek donanıma
  bağlanmanıza izin verir.
  Bu yüzden MOUNT C C:\OYUNLAR, DOSBox'a sizin C:\OYUNLAR dizininizi DOSBox
  içinde C: sürücüsü olarak kullanılacağını bildirir. MOUNT C E:\BirDizin,
  DOSBox'a sizin E:\BirDizin dizininizi DOSBox içinde C: sürücüsü olarak
  kullanılacağını bildirir.

  Bütün C sürücünüzü MOUNT C C:\ ile bağlanması önerilmez. Aynı durum,
  CD-ROM'lar dışında (yalnız okunur yapısı nedeniyle) başka bir sürücünün
  köküne bağlamak için doğrudur.
  Bunun dışında siz ya da DOSBox bir hata yaparsanız bütün dosyalarınızı
  kaybedebilirsiniz. Ayrıca hiçbir zaman Windows Vista/7'de asla "Windows"
  ya da "Program Files" dizinleri ya da onların alt dizinlerini bağlamayınız,
  çünkü DOSBox düzgün çalışmayabilir ya da düzgün çalışmadan sonra durabilir.
  Bütün DOS uygulamalarınızı/oyunlarınızı bir basit dizinde tutmanız
  (örneğin c:\dosoyunları) ve onu bağlamanız önerilir.

  Oyunlarınızı, her zaman DOSBox içinde kurmalısınız.
  Bu yüzden, siz CD'de oyuna sahipseniz her zaman (kurulumdan sonra!) ikisini
  bağlamak zorundasınız: sabit disk sürücüsü ya da CD-ROM olarak dizin.
  Sabit disk her zaman c olarak bağlanmalıdır.
  CD-ROM her zaman d olarak bağlanmalıdır.
  Disket her zaman a (ya da b) olarak bağlanmalıdır.)

  Sıradan kullanım için temel MOUNT örnekleri (Windows):

   1. Bir dizini bir sabit disk sürücüsü olarak bağlamak için:
          mount c d:\dosoyunları

   3. E: CD-ROM sürücünüzü DOSBox'ta D: CD-ROM sürücüsü olarak bağlamak için:
          mount d e:\ -t cdrom

   2. A: sürücünüzü disket olarak bağlamak için:
          mount a a:\ -t floppy

  İleri MOUNT örnekleri (Windows):

   4. Bir sabit disk sürücüsünü ~870 MB boş disk alanı ile bağlamak için (basit
   biçim):
          mount c d:\dosoyunları -freesize 870

   5. Bir sabit disk sürücüsünü ~870 MB boş disk alanı ile bağlamak için
   (yanlızca uzmanlar, tam denetim):
          mount c d:\oyunları -size 512,127,16513,13500

   1. c:\dosoyunları\disket'i bir disket olarak bağlamak için:
          mount a c:\dosoyunları\disket -t floppy


  Diğer MOUNT örnekleri:

   3. DOSBox'ta /media/cdrom bağlanma noktasındaki sistemin CD-ROM sürücüsünü
      CD-ROM sürücüsü D olarak bağlamak içinin DOSBox:
          mount d /media/cdrom -t cdrom -usecd 0

   6. DOSBox'ta /home/user/dosoyunları'i C sürücüsü olarak bağlamak için:
          mount c /home/user/dosoyunları

   7. DOSBox'ta DOSBox'un başladığı dizini C sürücüsü olarak bağlamak için:
          mount c .
          (not . DOSBox'un başladığı dizini belirtir, Windows Vista/7'de
          DOSBox'u "Program Files" dizininize kurmuşsanız bunu kullanmayın.)

  CD kalıbı ya da disket kalıbı bağlamak isterseniz IMGMOUNT'u gözden
  geçirin. Aynı zamanda, dış program kullanıyorsanız MOUNT
  kalıplarla da çalışır,örneğin (ikiside ücretsizdir):
  - Daemon Tools Lite (CD kalıpları için),
  - Virtual Floppy Drive (disket kalıpları için).
  Gerçi IMGMOUNT en iyi uyumluluğu verebilir.


MEM
  Serbest bellek miktarını ile türünü görüntülemek için programdır.


VER
VER set ana_sürüm [ikincil_sürüm]
  Geçerli DOSBox sürümünü görüntüler ve bildirir (parametresiz kullanım).
  Bildirilen DOS sürümünü "set" parametresi ile değiştirir,
  örneğin: DOSBox'un, sürüm numarası olarak DOS 6.22'yi bildirmesi için
  "VER set 6 22".

CONFIG -writeconf dosya_konumu
CONFIG -writelang dosya_konumu
CONFIG -securemode
CONFIG -set "bölüm özellik=değer"
CONFIG -get "bölüm özellik"

  CONFIG DOSBox'un ayarlarını çalışma zamanı sırasında değiştirmek ya da
  sorgulamak için kullanılabilir. Bu geçerli ayarları ile dil katarlarını diske
  kaydedebilir. Bölüm 13: "Yapılandırma (seçenekler) dosyası"'nda olası
  bölümler ile özellikler hakkında bilgi bulunabilir.

  -writeconf dosya_konumu
     Geçerli yapılandırma ayarlarını belirtilen konumda bir dosyaya yazar.
     "dosya_konumu" yerel sürücüde bulunur, DOSBox'ta bağlanmış sürücüde değil.
     Yapılandırma dosyası DOSBox'un çeşitli ayarlarını denetler:
     benzetimi yapılan bellek miktarı, benzetimi yapılan ses kartları ile çok
     şeyler. Ayrıca, bu AUTOEXEC.BAT'a erişimine olanak verir.
     Daha çok bilgi için Bölüm 13: "Yapılandırma (ayarlar) dosyası"'na bakınız.

  -writelang dosya_konumu
     Geçerli dil ayarlarını belirtilen konumda bir dosyaya yazar.
     "dosya_yolu" yerel sürücüde bulunur, DOSBox'ta bağlanmış sürücüde değil.
     Dil dosyası, iç komutların ile iç DOS'un bütün görünür çıktıyı denetler.
     Daha çok bilgi için Bölüm 14: "Dil Dosyası"'na bakınız.

  -securemode
     DOSBox daha güvenli kipe geçer. Bu kipte MOUNT, IMGMOUNT ile BOOT iç
     komutları çalışmaz. Bu kipte yeni bir yapılandırma dosyası ya da dil
     dosyası oluşturmak olanaklı değildir.
     (Uyarı: Yalnızca DOSBox'u yeniden başlatarak bu kipi geri alabilirsiniz.)

  -set "bölüm özellik=değer"
     CONFIG özelliği yeni değere ayarlamaya çalışır.
     Şu anda CONFIG komutun başarılı olmasını ya da olmamasını bildiremez.

  -get "bölüm özellik"
     Özelliğin geçerli değerini bildirir ve %CONFIG% ortam değişkenine yükler.
     Bu, toplu iş dosyaları kullanıldığı zaman değeri yüklemek için
     kullanılabilir.

  "-set" ile "-get" ikiside toplu iş dosyalarında her oyun için kendi
  özelliklerinizi düzenlemek için kullanabilirsiniz. Gerçi her oyun için bu yerine 
  DOSBox'un ayrı yapılandırma dosyaları kullanmak daha kolay olabilir.s

  Örnekler:
    1. c:/dosoyunları dizininizde bir yapılandırma dosyası oluşturmak için:
        config -writeconf c:\dosoyunları\dosbox.conf
    2. CPU çevrimini 10000'e ayarlamak için:
        config -set "cpu cycles=10000"
    3. EMS belleği benzetimini kapatmak için:
        config -set "dos ems=off"
    4. Hangi CPU çekirdeğini kullanıldığını bakmak için:
        config -get "cpu core"


LOADFIX [-size] [program] [program_parmaetreleri]
LOADFIX -f
  Geleneksel belleğin miktarını düşürmek için program.
  Çok serbest bellek beklemeyen eski programlar için kullanışlıdır.

  -size
        "Tüketilen" kilobayt sayısı, varsayılan = 64kb

  -f
        Önceden ayrılmış bütün belleği serbest bırakır.

  Örnekler:
    1. mm2.exe'yi başlatmak ve 64 KB bellek ayırmak için
       (mm2 64 KB'tan daha az kullanılabilir):
       loadfix mm2
    2. mm2.exe'yi başlatmak ve 32 KB bellek ayırmak için:
       loadfix -32 mm2
    3. Önceden ayrılmış belleği serbest bırakmak için:
       loadfix -f


RESCAN
  DOSBox'u dizin yapısını yeniden okumasını sağlar. DOSBox dışında
  bir bağlanmış sürücüde bir şeyleri değiştirdiyseniz kullanışlıdır.
  (CTRL - F4 de bunu yapabilir!)


MIXER
  DOSBox'un geçerli ses ayarlarını görüntülemesini sağlar.
  Burada onları nasıl değiştirebilirsiniz:

  mixer kanal sol:sağ [/NOSHOW] [/LISTMIDI]

  kanal
     Belirtilenlerden biri olabilir:
     MASTER, DISNEY, SPKR, GUS, SB, FM [, CDAUDIO].
     CDAUDIO yalnızca CD-ROM arayüzü ile birlikte ses denetimi etkinse
     geçerlidir (CD kalıbı, ioctl_dx).

  sol:sağ
     Yüzde cinsinden ses düzeyleri. Önde bir D koyarsanız desibel anlamına
     gelir. (Örneğin: mixer gus d-10).

  /NOSHOW
     Ses düzeylerinden birini ayarlarsanız DOSBox'un sonucu
     görüntülemesini önler.

  /LISTMIDI
     Windows'ta bilgisayarınızda mevcut midi aygıtlarını listeler. Windows'un
     varsayılan midi eşleştiricisinden başka bir aygıt seçmek için, 
     yapılandırma dosyasında [midi] bölümüde 'midiconfig=' satırını
     'midiconfig=kimlik' olarak değiştirin, burada 'kimlik' LISTMIDI tarafından
     listelenen aygıt için numaradır. Örneğin midiconfig=2

     Linux'ta bu seçenek çalışmaz, ama konsolda 'pmidi -l' kullanrak benzer
     sonuçlar elde edebilirsiniz. Ondan sonra 'midiconfig=' satırınıThen change the line 'midiconfig=' to 'midiconfig=port'
     'midiconfig=bağlantı_noktası' olarak değiştirin, burada 'bağlantı_noktası'
     'pmidi -l' tarafından listelenen aygıt için bağlantı noktasıdır.
     Örneğin midiconfig=128:0


IMGMOUNT
  DOSBox'ta disk kalıplarını ile CD-ROM kalıplarını bağlamak için araç.

  IMGMOUNT DRIVE [kalıp_dosyası] -t [kalıp_türü] -fs [kalıp_biçimi]
            -size [sektörlerin_bayt_boyutu, kafa_başına_sektör_sayısı, kafalar, silindirler]
  IMGMOUNT DRIVE [kalıp_dosyası_1 kalıp_dosyası_2 .. kalıp_dosyası_N] -t cdrom -fs iso

  imagefile
      DOSBox'ta bağlamak için kalıp dosyasının konumu. Konum, DOSBox içinde
      bağlanmış sürücüde ya da kendi gerçek diskinizde olabilir. Bu, CD-ROM
      kalıpları (ISOs ya da CUE/BIN ya da CUE/IMG) bağlamak için de
      olanaklıdır. CD takas yeteneğine gereksinim duyarssanız, bütün
      kalıpları art arda belirtin (Bir sonraki maddeye bakın).
      CUE/BIN çiftleri ile CUE/IMG, ISO'lar (yalnız veri olan) ile ses izlerini
      saklanabilir olarak karşılaştırıldığında öncelikli CD-ROM kalıbı
      türleridir. CUE/BIN bağlamak için her zaman CUE sayfasını belirtin. For
      the CUE/BIN mounting always specify the CUE sheet.

  kalıp_dosyası_1 kalıp_dosyası_2 .. kalıp_dosyası_N
      DOSBox'ta bağlamak için kalıp dosyalarının konumu. Birkaç kalıp dosyaları
      belirtmek yalnızca CD-ROM kalıplarına izin verilir.
      CD'ler her zaman CTRL-F4 ile takas yapılabilir.
      Bu birçok CD-ROM kullanan oyunlar için gereklidir ve oyun sırasında bazı noktalarda
      değişimi yapılmasını gerektirir.

  -t
      Aşağıdaki geçerli kalıp türleri:
        floppy: Bir disket kalıbı belirtir. DOSBox disk geometrisini
                kendiliğinden tanımlayacaktır(360K, 1.2MB, 720K, 1.44MB ve
                benzeri).
        cdrom:  CD-ROM kalıbı belirtilir. Geometri, kendiliğinden ve bu boyut
                için ayarlanır. Bu bir ISO ya da bir CUE/BIN çifti ya da
                CUE/IMG çifti olabilir.
        hdd:    Sabit disk kalıbı belirtir. Bunun çalışması için uygun CHS
                geometrisi ayarlanmış olmalıdır.

  -fs
      Aşağıdaki geçerli dosya sistemi biçimleri:
        iso:  ISO 9660 CD-ROM biçimi belirtir.
        fat:  Kalıbın FAT dosya sistemini kullandığını belirtir. DOSBox bu will
              kalıbı bir sürücü olarak bağlamaya çalışacak ve DOSBox içinde
              dosyaları erişilir kılar.
        none: DOSBox dosya sistemini okumak için hiçbir girişim yapmayacaktır.
              Bu onu biçimlendirmeye gereksinim duyarsanız ya da diski BOOT
              komutunu kullanarak önyüklemek isterseniz kullanışlıdır. "none"
              dosya sistemi kullanıldığında, bir sürücü harfi yerine sürücü
              numarasını (2 or 3, burada 2 = ana, 3 = bağımlı) belirtmelisiniz.
              Örneğin, 70 MB bir kalıbı bağımlı sürücü aygıtı olarak bağlamak için
              şunları yazın (tırmak işaretleri olmadan):
                "imgmount 3 d:\sınama.img -size 512,63,16,142 -fs none"
                Okumuş olduğunuzu, DOSBox içinde sürücüye erişmek için bir bağlama ile
                karşılaştırın:
                "imgmount e: d:\sınama.img -size 512,63,16,142"

  -size
     Sürücünün Silindirleri, Kafaları ile Sektörleri.
     Sabit disk kalıbı bağlamak için gereklidir.

  Nasıl CD-ROM kalıplarını bağlanılması için bir örnek (Linux'ta):
    1. imgmount d /tmp/cd_kalıbı_1.cue /tmp/cd_kalıbı_2.cue -t cdrom
  ya da (ayrıca çalışır):
    2a. mount c /tmp
    2b. imgmount d c:\cd_kalıbı_1.cue c:\cd_kalıbı_2.cue -t cdrom
  (Windows'ta):
    imgmount d f:\img\CD1.cue f:\img\CD2.cue f:\img\CD3.cue -t cdrom
    imgmount d "g:\img\7th Guest CD1.cue" "g:\img\7th Guest CD2.cue" -t cdrom
  Unutma, ayrıca kalıplar ile MOUNT kullanabilirsiniz, ama dış program
  kullanırsanız, örneğin (ikisi de ücretsiz):
  - Daemon Tools Lite (CD kalıpları için),
  - Virtual Floppy Drive (disket kalıpları için).
  Gerçi IMGMOUNT en iyi sonucu verebilir.


BOOT
  BOOT, disket kalıplarını ya da sabit disk kalıplarını DOSBox tarafından
  sunulan işletim sistemi benzetiminiden bağımsız olarak başlatacaktır. Bu
  önyüklemeli disketleri oynamanıza ya da DOSBox içinde diğer işletim
  sistemlerini önyüklemenize izin verir. Benzetimi yapılan hedef sistem
  PCjr ise (machine=pcjr) BOOT komutu PCjr kartuşlarını (.jrc) yüklemek için
  kullanılabilir.  

  BOOT [disk_kalıbı_1.img disk_kalıbı_2.img .. disk_kalıbı_N.img] [-l sürücü_harfi]
  BOOT [kart.jrc]  (yalnızca PCjr)

  diskimg1.img diskimg2.img .. diskimgN.img
     Bu, DOSBox belirtilen sürücü harfinden başlatıldıktan sonra birini
     bağlanmak istenen disket kalıpları herhangi bir sayıda olabilir.
     Kalıplar arasında geçiş yapmak için, geçerli diskten listede sonraki
     diske değiştirmek için CTRL-F4'e basın. Liste, en son disk kalıbından
     başlangıca geri dönecektir.

  [-l sürücü_harfi]
     Bu parametre, önyüklenecek sürücüyü belirlemenize izin verir.
     Varsayılan olarak A sürücüsü, disket sürücüdür. Ayrıca tırnak işaretleri
     olmadan "-l C" belirterek ana olarak ya da "-l D" belirterek bağımlı
     olarak bağlanmış bir sabit disk kalıbından başlatabilirsiniz.

   cart.jrc (PCjr only)
     PCjr benzetimi etkinleştirildiğinde, kartuşlar BOOT komutu ile
     yüklenebilir. Destek henüz sınırlıdır.


IPX

  DOSBox'un yapılandırma dosyasında IPX ağı iletişimini etkinleştirmeye
  gereksinim duyarsınız.

  Bütün IPX ağ iletişimini DOSBox iç program IPXNET aracılığıyla yönetilir.
  DOSBox içinde IPX ağ iletişimi üzerinde yardım için, "IPXNET HELP"
  (tırnak işareti olmadan) yazın ve program komutları ile konu ile ilgili
  belgeleri listeleyecektir.

  Aslında bir ağ kurmak bakımından, bir sistemin sunucu olmasına gereksinim
  duyar. Bunu kurmak için, bir DOSBox oturumunda "IPXNET STARTSERVER" (tırnak
  işaretleri olmadan) yazın. Sunucu DOSBox oturumu kendiliğinden kendini sanal
  IPX ağına ekleyecektir. Her ek bilgisayar için sanal IPX ağının parçası
  olmalıdır, "IPXNET CONNECT <Bilgisayarın alan ismi ya da IP>"
  yazmaya gereksinim duyarsınız.
  Örneğin, sunucunuz bob.dosbox.com ise, bütün sunucu olmayan sistemlerde
  "IPXNET CONNECT bob.dosbox.com" yazarsınız.

  Netbios'a gereksinim duyan oyunları oynamak için Novell'in NETBIOS.EXE isimli
  bir dosya gereklidir. Yukarıda açıklandığı gibi IPX bağlantısı kurun, sonra
  "netbios.exe"'yi çalıştırın.

  Aşağıdaki bir IPXNET komutu referansı:

  IPXNET CONNECT

     IPXNET CONNECT, başka DOSBox oturumunda çalışan bir IPX tünelleme sunucusu
     için bir bağlantı açar. "Adres" parametresi sunucu bilgisayarın adresini
     ya da alan ismini belirtir. Ayrıca kullanmak için UDP bağlantı noktası
     belirtebilirsiniz. Varsayılan olarak IPXNET kendi bağlantısı için bağlantı
     noktası 213'ü - IPX tünelleme için ayrılmış IANA bağlantı noktası -
     kullanır.

     The syntax for IPXNET CONNECT için sözdizimi:
     IPXNET CONNECT adres <bağlantı noktası>

  IPXNET DISCONNECT

     IPXNET DISCONNECT, IPX tünelleme sunucusuna bğlantıyı kapatır.

     IPXNET DISCONNECT için sözdizimi:
     IPXNET DISCONNECT

  IPXNET STARTSERVER

     IPXNET STARTSERVER, bu DOSBox oturumunda IPX tünelleme sunucusunu
     başlatır. Varsayılan olarak, sunucu UDP 213 bağlantı noktasında
     bağlantıyı onaylar, gerçi bu değiştirilebilir. Sunucu başlatıldıktan
     sonra, DOSBox, IPX tünelleme sunucusu için kendiliğinden bir istemci
     bağlantısı başlayacaktır.

     IPXNET STARTSERVER için sözdizimi:
     IPXNET STARTSERVER <bağlantı noktası>

     Sunucu, bir yönlendiricinin arkasında ise, UDP <bağlantı noktası>
     bağlantı noktası o bilgisayar için yönlendirmeye gerekir.

     Linux/Unix tabanlı sistemlerde 1023'ten küçük bağlantı noktası numaraları
     yalnızca yönetici hakları ile birlikte kullanılabilir. O sistemlerde
     1023'ten büyük bağlantı noktaları kullanın.

  IPXNET STOPSERVER

     IPXNET STOPSERVER, bu DOSBox oturumunda IPX tünelleme sunucusunu durdurur.
     Diğer bütün bağlantıların doğru şekilde sona ermesine emin olmak için
     özen gösterilmelidir, sunucuyu durdurduktan sonra IPX tünelleme sunucusunu
     yine kullanan diğer makinelerde kilitlenmelere neden olabilir.

     IPXNET STOPSERVER için sözdizimi:
     IPXNET STOPSERVER

  IPXNET PING

     IPXNET PING, IPX tünelleme sunucusu üzerinden bir ping atar.
     Yanıt olarak, diğer bütün bağlı bilgisayarlar pinge karşılık verir ve
     ping iletisinin alınma ile gönderilme süresini raporlar.

     IPXNET PING için sözdizimi:
     IPXNET PING

  IPXNET STATUS

     IPXNET STATUS, bu DOSBox outrumunun IPX tünelleme ağının geçerli durumunu
     rapor eder. Ağa bağlı bütün bilgisayarların bir listesi için IPXNET PING
     komutunu kullanın.

     IPXNET STATUS için sözdizimi:
     IPXNET STATUS


KEYB [klavye_düzeni_kodu [kod_sayfası [kod_sayfası_dosyası]]]

  Klavye düzenini değiştirir. Klavyeler ile ilgili ayrıntılı bilgi için lütfen
  Bölüm 8: "Klavye Düzeni"'ne bakınız.

  [klavye_düzeni_kodu] beş ya da daha az karakterden oluşan katardır,
     örnekler PL214 (Lehçe daktilocular) ya da PL457 (Lehçe programcılar).
     Bu kullanılacak klavye düzenini belirtir.
     DOSBox içine yapılı bütün düzenlerin listesi burada:
     http://vogons.zetafleet.com/viewtopic.php?t=21824

  [kod_sayfası] kullanılacak kod sayfasının numarasıdır. Klavye düzeni
     belirtilen kod sayfası için destek sağlamak zorundadır, bunun dışında
     düzen yükleme başarısız olur.
     Hiçbir kod sayfası belirtilmemişse, istenen düzen için uygun kod sayfası
     kendiliğinden seçilir.

  [kod_sayfası_dosyası] henüz DOSBox içine derlenmemiş kod sayfalarını yüklemek
     için kullanılabilir. Bu yalnızca DOSBox kod sayfasını bulamadığında
     gereklidir. Hiçbir kod sayfası dosyası belirtilmemişse, ama on ega.cpx
     dosyalarını (FreeDOS'tan) DOSBox program dizinine yerleştirdiyseniz,
     istenen düzen/kod sayfası için uygun kod sayfası dosyası kendiliğinden
     seçilir.

  Örnekler:
    1. Lehçe daktilocu tuşları düzenini yüklemek için (kendiliğinden kod
       sayfası 852 kullanılır):
         keyb pl214
    2. Bir Rus klavye düzenlerinden birini kod sayfası 866 ile birlikte
       yüklemek için:
         keyb ru441 866
       Rus karakterleri yazmak için ALT+RIGHT-SHIFT'e basın.
    3. Fransız klavye düzenlerinden birini kod sayfası 850 ile birlikte
       yüklemek için (kod sayfası EGACPI.DAT'ta tanımlı):
         keyb fr189 850 EGACPI.DAT
    4. Kod sayfası 858'i yüklemek için (bir klayve düzeni olmadan):
         keyb none 858
       Bu, FreeDOS keyb2 aracına uygun kod sayfasını değiştirmek için
       kullanılabilir.
    5. Geçerli kod sayfasını ile yüklü ise klavye düzenini görüntülemek için:
         keyb



Daha çok bilgi için programlar ile birlikte /? komut satırı anahtarını kullananın.



===============
5. Özel Tuşlar:
===============

ALT-ENTER     Tam ekrana geçer ve vazgeçer.
ALT-PAUSE     Benzetimi duraklatır (yeniden sürdürmek için ALT-PAUSE'ye basın).
CTRL-F1       Tuş eşleştiricisini başlatır.
CTRL-F4       Bağlanmış disket/CD kalıplarının arasında aktarma yapar. Bütün
              sürücüler için dizin önbelleğini günceller.
CTRL-ALT-F5   Ekranın bir filmini başlatır/durdurur. (avi video yakalama)
CTRL-F5       Bir ekran görüntüsü kaydeder. (PNG biçimi)
CTRL-F6       Sesi bir ses dosyasında kaydedilmesini başlatır/durdurur.
CTRL-ALT-F7   OPL komutlarının kaydedilmesini başlatır/durdurur. (DRO biçimi)
CTRL-ALT-F8   Saf MIDI komutlarının kaydedilmesini başlatır/durdurur.
CTRL-F7       Çerçeve atlamayı azaltır.
CTRL-F8       Çerçeve atlamayı arttırır.
CTRL-F9       DOSBox'u öldürür.
CTRL-F10      Fareyi yakalar/bırakır.
CTRL-F11      Benzetimi yavaşlatır (DOSBox çevrimlerini azaltır).
CTRL-F12      Benzetimi hızlandırır (DOSBox çevrimlerini arttırır)*.
ALT-F12       Hız kilidini açar (turbo düğmesi/hızlı ileri alma)**.
F11, ALT-F11  (machine=cga) NTSC çıktı kipini değiştirir***
F11           (machine=hercules) kehribar, yeşil, beyaz renklendirme arasında***
devir yapar.

*NOT: Bir kez DOSBox'unuzun çevirimini bilgisayarınızın CPU kaynaklarının
      ötesinde arttırmak, bezetimi yavaşlatmak gibi aynı etki yapacaktır.
      Bu en yüksek derece bilgisayardan bilgisayara değişir.

**NOT: Bu için CPU kaynaklarını serbest bırakmaya gereksinim duyarsınız (daha
       çok elde ederseniz, daha hızlı gider), bu yüzden cycles=max ya da çok
       yüksek bir sabit çevirim ile hiç çalışmaz. Bunun çalışması için tuşları
       basılı tutmak zorundasınız.

***NOT: Bu tuşlar faklı bir makine türü ile daha önce bir eşleştirici dosyasını
        kaydettiyseniz çalışmaz. Bu yüzden ya onları yeniden atayın ya da
        eşleştiriciyi sıfırlayın.

Bunlar varsayılan tuş bağlarıdır. Onları tuş eşleştiricisinde
değiştirebilirsiniz (Bölüm 7: Tuş Eşleştiricisi'ne bakınız). 

MAC OS'ta Ctrl ile birlikte cmd(apple tuşu)'yi kullanmayı denerseniz tuşyou can try using cmd(applekey) together with Ctrl if the key doesn't
çalışmaz örneğin cmd-ctrl-F1, ama bazı tuşlar yine de eşleştirmeye gereksinim duyabilir (Linux'te de).

İçinde kaydedilmiş dosyalar bulunabilir:
   (Windows)    "Başlat/WinLogo Menüsü"->"Tüm Programlar"->DOSBox-0.74->Extras
   (Linux)      ~/.dosbox/capture
   (MAC OS X)   "~/Library/Preferences/capture"
Bu DOSBox yapılandırma dosyasında değiştirebilir.



=========================
6. Oyun Çubuğu/Oyun Kolu:
=========================

DOS altında standart joystick bağlantı noktası en çok 4 eksen ile 4 düğmeyi
destekler. Daha çok için, o yapılandırmanın farklı değişiklikleri kullanıldı.

DOSBox'u farklı bir oyun çubuğunu/oyun kolunu kullanmaya zorlamak için,
DOSBox yapılandırma dosyasında [joystick] bölümünde "joysticktype" girişi
kullanılabilir.

none  - denetleyici desteğini devre dışı bırakır.
auto  - (varsayılan) Bir ya da iki denetleyici bağlı olup olmadığını
        kendiliğinden algılar:
          Bir varsa - '4axis' ayarı kullanılır,
          İki varsa - '2axis' ayarı kullanılır.
2axis - İki denetleyici bağladıysanız, her biri 2 eksenli ile 2 düğmeli bir
        bir joystickin benzetimini yapacaktır. Yalnız bir denetleyici bağladıysanız
        yalnızca 2 eksenli ile 2 düğmeli bir joystickin benzetimini yapacaktır.
4axis - Yalnızca birinci denetleyiciyi destekler, 4 eksenli ile 4 düğmeli bir
        joystickin ya da 2 eksenli ile 6 düğmeli bir oyun kolunun benzetimini
        yapar.
4axis_2 - Yalnızca ikinci denetleyiciyi destekler.
fcs   - Yalnızca birinci denetleyiciyi destekler, 3 eksenli, 
        4 düğmeli ile 1 başlıklı ThrustMaster Flight Control System'in
        benzetimini yapar.
ch    - Yalnızca birinci denetleyiciyi destekler, 4 eksenli, 6 düğmeli ile
        1 şapkalı CH Flightstick'in benzetimini yapar, ama aynı anda birden
        çok düğmeye basamazsınız.

Ayrıca, oyun içinde denetleyicileri uygun şekilde yapılandırmanız gerekir.

Bağlı joystick olmadan ya da farklı bir joystick ayarı ile tuş eşleştiricisini
kaydetmişseniz, DOSBox'un tuş eşleştiricisini sıfırlayana kadar yeni
değişiklikleriniz düzgün çalışmamasını ya da hiç çalışmamasını hatırlamak
önemlidir.


Denetleyici DOSBox dışında düzgün çalışıyorsa, ama DOSBox içinde düzgün
ayarlanmıyorsa, DOSBox yapılandırma dosyasında farklı 'timed' ayarı
deneyin.



=====================
7. Tuş Eşleştiricisi:
=====================

DOSBox eşleştiricisini CTRL-F1 (Bölüm 5. Özel Tuşlar'a bakınız) ya da
-startmapper ikisinden biri(Bölüm 3. Komut Satırı Parametreleri'ne bakınız) ile başlatın.
Size bir sanal klavye ile bir sanal joystick sunulmaktadır.

Bu sanal aygıtların tuşlarına ile olaylarına karşılık DOSBox DOS uygulamalarına
bildirecektir. Farenizde bir tuşa basarsanız, sağ aşağı köşede ilişkilendirilen
olay (EVENT) ile şu anda bağlı olayları görebilirsiniz.

EVENT: Olay
BIND: Bağlayan şey (sizin parmağınız/eliniz ile bastığınızgerçek tuş/düğme/eksen)

                                    Add   Del
mod1  hold                                Next
mod2
mod3


EVENT
    DOSBox'un DOS uygulamasına bildireceği tuş ya da joystick
    ekseni/düğmesi/şapkası. (Oyun sırasında gerçekleştirilecek olay [örneğin
    ateş etme/zıplama/yürüme])
BIND
    EVENT'te bağlı gerçek klavyenizde tuş ya da gerçek joystick(ler)inizde
    eksen/düğme/şapka (SDL tarafından bildirilen).
mod1,2,3
    Tanımlayıcılar. Bunlar BIND'a bastığınız zaman basılmasına gereksinim
    duyduklarınız tuşlardır. mod1 = CTRL ile mod2 = ALT. Bunlar çoğunlukla
    yalnızca DOSBox'un özel tuşlarını değiştirmek istediğinizde kullanılır.
Add
    Bu EVENT'e yeni BIND ekler. Aslında DOSBox'ta EVENT üretecek klavyenizde
    bir tuş ya da joystickinizde yeni bir olay (düğmeye baılması, eksen/şapka
    hareketi) eklemektir.
Del
    EVENT'te BIND'i siler. Bir EVENT hiç BIND'lara sahip değilse, bu DOSBox'ta
    bu olayı tetiklemek için olanaklı değildir (Bunun tuşu tuşlamak ya da
     joystickte ilgili eylemi kullanmak olanağı yok).
Next
    Bu EVENT'te eşleştirilen bağlamalar listesine baştan başa gider.


Örnek:
Q1. DOSBox'ta bir Z yazmanız için klavyenizde X'in bulunmasını istiyorsunuz.
    A. Tuş eşleştiricinizdeki Z'ye tıklayın. "Add"'e tıklayın.
       Şimdi, klavyenizdeki X'e basın.

Q2. "Next"'e birkaç kez tıklarsanız, siz DOSBox'ta klavyenizdeki Z'nin bir Z
    ürettiğini de farkettiniz.
    A. Bu nedenle Z'yi yeniden seçin ve klavyenizdeki Z'yi buluncaya kadar
       "Next"'e tıklayın. Şimdi "Del"'e tıklayın.

Q3. DOSBox'ta ondan çıkmayı denerseniz, X'e bastığınızda ZX ortaya çıktığını
    farkedeceksiniz.
     A. Klavyenizdeki X'in yine X'e de eşleşmiştir! Tuş eşleştiricinizdeki X'e
        tıklayın ve X'i eşleşmiş tuş X'i buluncaya kadar "Next" ile arayın.
        "Del"'e basın.


Joysticki yeniden eşleştirme ile ilgili örnekler:
  Bir joystick bağladığınızda, DOSBox altında iyi çalışır ve yalnız klavye ile
  oynanan bazı oyunları joystick ile oynamak isterseniz (bu, klavyedeki oklar
  ile yönetilen oyunu sanır.):
    1. Eşleştiriciyi başlatın, sonra sol klavye oklarından birine tıklayın.
       EVENT, key_left olmalıdır. Şimdi "Add"'e tıklayın ve joystickinizi
       ilgili yöne hareket ettirin, bu BIND'a olay eklemelidir.
    2. Eksik 3 yön için yukarıdakini yineleyin, ayrıca joystickin
       düğmelerini de yeniden eşleştirebilirsiniz (ateş/zıplama).
    3. Save'ye tıklayın, sonra Exit'e tıklayın ve bazı oyunlarda deneyin.

  Joystickin y-eksenini değiştirmek istersiniz, çünkü bazı uçuş benzetimcileri
  sevmediğiniz bir biçimde yukarı/aşağı joystick hareketleri kullanır ve bu
  oyunda kendini yapılandırılamaz:
    1. Eşleştiriciyi başlatın ve ilk joystick alanında Y-'ye tıklayın.
       EVENT, jaxis_0_1- olmalıdır.
    2. Geçerli bağlayıcıyı kaldırmak için Del'e tıklayın, sonra Add'e tıklayın
       ve joystickinizi aşağıya doğru hareket ettirin. Yeni bir bağ oluşmalıdır.
    3. Bunu Y+ için yineleyin, düzeni kaydedin ve en sonunda bunu bazı
	oyunlarla sınayın.

  D-padinizi/Şapkanızı herhangi bir şeye yeniden eşleştirmek isterseniz,
  yapılandırma dosyasında 'joysticktype=auto''yu 'joysticktype=fcs''ye
  değiştirmek zorundasınız. Belki bu, ileriki DOSBox sürümünde
  geliştirilebilir.


Varasyılan eşleşmeyi değiştirmek isterseniz, "Save"'ye tıklayarak
değişikliklerinizi kaydedebilirsiniz. DOSBox, yapılandırma dosyasında
belirtilen bir konuma (mapperfile= kayıt) kaydedecektir. Başlangıçta, DOSBox
yapılandırma dosyasında belirtmişseniz, DOSBox, eşleştirme dosyasını
yükleyecektir.



=================
8. Klavye Düzeni:
=================

Farklı bir klavye düzenine değiştirmek için, ya DOSBox yapılandırma dosyasında
bölümünde "keyboardlayout" girişi kullanılabilir ya da DOSBox iç programı
keyb.com (Bölüm 4: İç Programlar)
Her ikisi de DOS uyumlu dil kodlarını kabul eder (aşağıya bakınız), ama
yalnızca keyb.com'u kullanarak bir özel kod sayfası tanımlayabilirsiniz.

Varsayılan keyboardlayout=auto şu anda yalnızca Windows altında çalışır. Dil,
işletim sisteminin diline göre seçilir, ama klavye düzeni algılanmaz.

Düzen değiştirmek
  DOSBox varsayılan olarak bir kaç klavye düzeni ile kod sayfasını destekler,
  bu durumda yalnızca düzen tanımlayıcısının tanımlanmasına (DOSBox
  yapılandırma dosyasında keyboardlayout=PL214 ya da DOSBox komut isteminde
  "keyb PL214"kullanma gibi) gereklidir. DOSBox içine yapılı bütün düzenlerin
  listesi burada: http://vogons.zetafleet.com/viewtopic.php?t=21824

  Bazı klavye düzenleri (örneğin GK319 düzeni 869 kod sayfası ile RU441 düzeni)
  808 kod sayfası) bir düzen için SolALT+SağSHIFT'e ile diğeri
  SolALT+SolSHIFT'e basılarak erişilebilen çift düzen için desteğe sahiptir.
  Bazı klavye düzenleri (örneğin LT456 düzeni 771 kod sayfası) üç düzen için
  desteğe sahiptir, üçüncü SolALT+SolCTRL'ye basılarak erişilebilir.

Desteklenen dış dosyalar
  FreeDOS .kl dosyaları desteklenir (FreeDOS keyb2 Klavye Düzen Dosyaları)
  hem de bütün mevcut .kl dosyalarndan oluşan FreeDOS keyboard.sys/keybrd2.sys/
  keybrd3.sys kütüphaneleri.
  DOSBox'a tümleşik düzenler bazı nedenlerden dolayı çalışmıyorsa ya da
  güncellenmişse ya da yeni düzenler kullanılabilir duruma gelirse önceden
  derlenmiş klavye düzenleri için http://www.freedos.org/'a bakınız.

  .CPI (MS-DOS ile uyumlu kod sayfası dosyaları) ile .CPX (FreeDOS
  UPX-sıkıştırılmış kod sayfası dosyaları) her ikisi de kullanılabilir. Bazı
  kod sayfaları DOSBox içinde derlenmiştir, bu yüzden çoğunlukla dış kod
  sayfası dosyalarını önemsemek gerekli değildir. Bir farklı (ya da özel) kod
  sayfası dosyasına gereksinim duyuyorsanız, bunu DOSBox dizinine kopyalayın
  böylece DOSBox için erişilebilir. DOSBox dizinine on ega.cpx dosyalarını
  (FreeDOS'tan) yerleştirirseniz, istenen düzen/kod sayfası için bir uygun kod
  sayfası kendiliğinden seçilir.

  Ek düzenler uygun .kl dosyasının DOSBox yapılandırma dosyasının dizinine
  kopyalanma ve dosya isminin ilk bölümü dil kodu olarak kullanılma ile
  eklenebilir.
  Örnek: For the file UZ.KL dosyası için (Özbekistan için klavye dosyası)
         DOSBox yapılandırma dosyasında "keyboardlayout=uz" belirtin.
  Klavye düzeni paketlerinin (keybrd2.sys gibi) tümleştirilmesi benzer şekilde
  çalışır.

Dikkat edin, bu klavye düzeni yabancı karakterlerin girilmesine izin verir, ama
dosya adlarında onlar için orada destek yoktur. Hem DOSBox içinde hem de DOSBox
tarafından erişilebilen ana bilgisayar işletim sisteminizdeki dosyalarda
kullanmaktan kaçınmaya çalışınız.



============================
9. Seri Çok Oyuncu Özelliği:
============================

DOSBox ağ ile internet üzerinde bir seri kukla modem kablosu benzetimi
yapabilir. Bu, DOSBox yapılandırma dosyasında [serialports] bölümünün üzerinden
yapılandırılabilir.

Bir kukla modem bağlantısı kurmak için, bir taraf sunucu olarak ve biri
istemci olarak davranmaya gereksinim duyar.

Sunucu, DOSBox yapılandırma dosyasında bunun gibi kuruluma gereksinim duyar:
   serial1=nullmodem

İstemci:
   serial1=nullmodem server:<IP ya da sunucunun ismi>

Şimdi oyunu başlatın ve çok oyunculu yöntemi ile COM1 üzerinden kukla modem /
seri kablo / şimdiden bağlanmış seçin. Her iki bilgisayara aynı baud hızını
ayarlayın.

Ayrıca, kukla modemin bağlantı davranışını denetlemek için ek parametreler
belirtilebilir. Bunlar bütün parametrelerdir:

 * port:         - TCP bağlantı noktası numarası. Varsayılan: 23
 * rxdelay:      - Arayüz hazır değilse alınan veriyi geciktirmek için
                   ne kadar süre (milisaniye). DOSBox Durum Penceresi'nde taşma
                   hataları ile karşılaşırsanız bu değeri arttırın.
                   Varsayılan: 100
 * txdelay:      - Bir paket gönderiminden önce veriyi toplamak için ne kadar
                   süre. Varsayılan: 12 (ağ ek yükünü azaltır)
 * server:       - Bu kukla modemin belirtilen sunucuya bir istemci bağlama.
                   (server argümanı yok: bir sunucu olur)
 * transparent:1 - Yalnızca seri veriyi gönderir, RTS/DTR el sıkışması yok. Bu,
                   bir kukla modem dışında bir şey bağlandığında kullanılır.
 * telnet:1      - Uzak siteden Telnet verisini yorumlar. Kendiliğinden şeffaf
                   ayarlar.
 * usedtr:1      - Bağlantı, DOS programı tarafından DTR ayarlanıncaya kadar
                   kurulmuş olmayacaktır. Modem uçbirimleri için kullanışlıdır.
                   Kendiliğinden şeffaf ayarlar.
 * inhsocket:1   - Komut satırı tarafından DOSBox'a geçirilen bir yuva
                   kullanılır. Kendiliğinden şeffaf ayarlar. (Yuva Kalıtımı: BuIt is used for
                   yeni BBS yazılımlarında eski kapı oyunlarını oynamak için
                   kullanılır.)

Örnek: TCP bağlantı nıktası 5000'i dinleyen bir sunucu anlamına gelir.
   serial1=nullmodem server:<IP ya da sunucunun ismi> port:5000 rxdelay:1000



=====================================
10. How to speed up/slow down DOSBox:
=====================================

DOSBox emulates the CPU, the sound and graphic cards, and other peripherals
of a PC, all at the same time. The speed of an emulated DOS application
depends on how many instructions can be emulated, which is adjustable
(number of cycles).

CPU Cycles (speed up/slow down)
  By default (cycles=auto) DOSBox tries to detect whether a game needs to
  be run with as many instructions emulated per time interval as possible
  (cycles=max, sometimes this results in game working too fast or unstable),
  or whether to use fixed amount of cycles (cycles=3000, sometimes this results
  in game working too slow or too fast). But you can always manually force
  a different setting in the DOSBox's configuration file.

  You can force the slow or fast behavior by setting a fixed amount of cycles
  in the DOSBox's configuration file. If you for example set cycles=10000, then
  DOSBox window will display a line "Cpu Speed: fixed 10000 cycles" at the top.
  In this mode you can reduce the amount of cycles even more by hitting CTRL-F11
  (you can go as low as you want) or raise it by hitting CTRL-F12 as much as you
  want, but you will be limited by the power of one core of your computer's CPU.
  You can see how much free time your real CPU's cores have by looking at
  the Task Manager in Windows 2000/XP/Vista/7 and the System Monitor
  in Windows 95/98/ME. Once 100% of the power of your computer's real CPU's one
  core is used, there is no further way to speed up DOSBox (it will actually
  start to slow down), unless you reduce the load generated by the non-CPU parts
  of DOSBox. DOSBox can use only one core of your CPU, so If you have
  for example a CPU with 4 cores, DOSBox will not be able to use the power
  of three other cores.

  You can also force the fast behavior by setting cycles=max in the DOSBox
  configuration file. The DOSBox window will display a line
  "Cpu Speed: max 100% cycles" at the top then. This time you won't have to care
  how much free time your real CPU's cores have, because DOSBox will always use
  100% of your real CPU's one core. In this mode you can reduce the amount
  of your real CPU's core usage by CTRL-F11 or raise it with CTRL-F12.

CPU Core (speed up)
  On x86 architectures you can try to force the usage of a dynamically
  recompiling core (set core=dynamic in the DOSBox configuration file).
  This usually gives better results if the auto detection (core=auto) fails.
  It is best accompanied by cycles=max. But you may also try using it with
  high amounts of cycles (for example 20000 or more). Note that there might be
  games that work worse/crash with the dynamic core (so save your game often),
  or do not work at all!

Graphics emulation (speed up)
  VGA emulation is a demanding part of DOSBox in terms of actual CPU usage.
  Increase the number of frames skipped (in increments of one) by pressing
  CTRL-F8. Your CPU usage should decrease when using a fixed cycle setting,
  and you will be able to increase cycles with CTRL-F12.
  You can repeat this until the game runs fast enough for you.
  Please note that this is a trade-off: you lose in fluidity of video what
  you gain in speed.

Sound emulation (speed up)
  You can also try to disable the sound through the setup utility of the game
  to reduce load on your CPU further. Setting nosound=true in DOSBox's
  configuration does NOT disable the emulation of sound devices, just
  the output of sound will be disabled.

Also try to close every program but DOSBox to reserve as much resources
as possible for DOSBox.


Advanced cycles configuration:
The cycles=auto and cycles=max settings can be parameterized to have
different startup defaults. The syntax is
  cycles=auto ["realmode default"] ["protected mode default"%]
              [limit "cycle limit"]
  cycles=max ["protected mode default"%] [limit "cycle limit"]
Example:
  cycles=auto 5000 80% limit 20000
  will use cycles=5000 for real mode games, 80% CPU throttling for
  protected mode games along with a hard cycle limit of 20000



====================
11. Troubleshooting:
====================

General tip:
  Check messages in DOSBox Status Window. See section 12. "DOSBox Status Window"

DOSBox crashes right after starting it:
  - use different values for the output= entry in your DOSBox
    configuration file
  - try to update your graphics card driver and DirectX
  - (Linux) set the environment variable SDL_AUDIODRIVER to alsa or oss.

Running a certain game closes DOSBox, crashes with some message or hangs:
  - see if it works with a default DOSBox installation
    (unmodified configuration file)
  - try it with sound disabled (use the sound configuration
    program that comes with the game, additionally you can
    set sbtype=none and gus=false in the DOSBox configuration file)
  - change some entries of the DOSBox configuration file, especially try:
      core=normal
      fixed cycles (for example cycles=10000)
      ems=false
      xms=false
    or combinations of the above settings,
    similar the machine settings that control the emulated chipset and
    functionality:
      machine=vesa_nolfb
    or
      machine=vgaonly
  - use loadfix before starting the game

The game exits to the DOSBox prompt with some error message:
  - read the error message closely and try to locate the error
  - try the hints at the above sections
  - mount differently as some games are picky about the locations,
    for example if you used "mount d d:\oldgames\game" try
    "mount c d:\oldgames\game" and "mount c d:\oldgames"
  - if the game requires a CD-ROM be sure you used "-t cdrom" when
    mounting and try different additional parameters (the ioctl,
    usecd and label switches, see the appropriate section)
  - check the file permissions of the game files (remove read-only
    attributes, add write permissions etc.)
  - try reinstalling the game within DOSBox



=========================
12. DOSBox Status Window:
=========================

DOSBox's Staus window contains many useful information about your currant
configuration, your actions in DOSBox, errors that happened and more.
Whenever you have any problem with DOSBox check these messages.

To start DOSBox Status Window:
  (Windows)  Status Window is being started together with main DOSBox window.
  (Linux)    You may have to start DOSBox from a console to see Status Window.
  (MAC OS X) Right click on DOSBox.app, choose "Show Package Contents"->
             ->enter "Contents"->enter "MacOS"->run "DOSBox"



=====================================
13. The configuration (options) file:
=====================================

The configuration file is automatically created the first time you run DOSBox.
The file can be found in:
   (Windows)  "Start/WinLogo Menu"->"All Programs"->DOSBox-0.74->Options
   (Linux)    ~/.dosbox/dosbox-0.74.conf
   (MAC OS X) "~/Library/Preferences/DOSBox 0.74 Preferences"
The file is divided into several sections. Each section starts with a
[section name] line. The settings are the property=value lines where value can
be altered to customize DOSBox.
# and % indicate comment-lines.


An extra configuration file can be generated by CONFIG.COM, which can be found
on the internal DOSBox Z: drive when you start up DOSBox. Look in the Section 4:
"Internal programs" for usage of CONFIG.COM. You can start DOSBox with
the -conf switch to load the generated file and use its settings.

DOSBox will load configuration files that are specified with -conf. If none were
specified, it will try to load "dosbox.conf" from the local directory.
If there is none, DOSBox will load the user configuration file.
This file will be created if it doesn't exist.

Important!: In Windows Vista/7 the configuration file won't work correctly
if it is located in "Windows" or "Program Files" folder or their subfolders,
or directly on c:\, so the best place for storing extra configuration files is
for example: C:\oldgames



======================
14. The Language File:
======================

A language file can be generated by CONFIG.COM, which can be found on the
internal DOSBox Z: drive when you start up DOSBox. Look in the Section 4:
"Internal programs" for usage of CONFIG.COM.
Read the language file, and you will hopefully understand how to change it.
Start DOSBox with the -lang switch to use your new language file.
Alternatively, you can setup the filename in the configuration file
in the [dosbox] section. There's a language= entry that can be changed with
the filelocation.



========================================
15. Building your own version of DOSBox:
========================================

Download the source.
Check the INSTALL in the source distribution.



===================
16. Special thanks:
===================

See the THANKS file.



============
17. Contact:
============

See the site: 
http://www.dosbox.com
for an email address (The Crew-page).


