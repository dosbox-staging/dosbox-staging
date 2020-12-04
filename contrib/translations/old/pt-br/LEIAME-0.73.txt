DOSBox v0.73


=====
NOTA: 
=====

Enquanto nós estamos torcendo para que um dia o DOSBox rode todos os programas
já feitos para PC, ainda não estamos lá. Hoje o DOSBox, rodando
numa máquina potente, dificilmente será equivalente a um PC 486.
O DOSBox pode ser configurado para rodar um leque variado de jogos DOS,
desde clássicos CGA/Tandy/PCjr até jogos da era Quake.


Arquivo LEIA-ME traduzido por: Lex Leite   / ableite@msn.com
                               Sael Aran   / lord.sael@hokkaido-fansubber.com

                 com ajuda de: Mosca-Ninja / - -
                               David Tuka  / david_tuka@hotmail.com



=======
ÍNDICE:
=======
1. Início Rápido
2. Perguntas & Respostas
3. Uso
4. Programas Internos
5. Teclas Especiais
6. Mapeador de Teclas
7. Layout do teclado
8. Função Multi-Jogador via Serial
9. Como rodar jogos que necessitam mais recursos
10. Solução de Problemas
11. O arquivo das configurações
12. O arquivo do idioma
13. Fazendo sua própria versão do DOSBox
14. Agradecimentos especiais
15. Contato


=================
1. Início Rápido:
=================

Digite INTRO no DOSBox para um tour rápido. 
É essencial que você se familiarize com a ideia de montar
diretórios. O DOSBox não faz com que qualquer unidade
(ou parte dela) seja automaticamente acessível para emulação.
Veja a parte de P&R "Eu tenho um diretório Z ao invés de
um C no prompt." assim como a explicação do comando MOUNT
(seção 4)



=======
2. P&R:
=======

Algumas perguntas feitas frequentemente:

P: Aparece um diretório Z ao invés de um C no prompt.
P: Sempre deverei digitar esses comandos? Não existe uma forma automática?
P: Como eu coloco tela cheia?
P: Meu CD-ROM não funciona.
P: O jogo/aplicativo não consegue achar seu CD-ROM.
P: O mouse não funciona.
P: O aplicativo não tem som.
P: O som sai cortado ou estranho/difuso.
P: Eu não consigo digitar \ ou : no DOSBox.
P: Estou com atraso no teclado.
P: O cursor está se movendo em apenas uma direção!
P: O jogo/aplicativo roda muito lento!
P: O jogo/aplicativo não chega a rodar/trava!
P: O DOSBox pode danificar meu computador?
P: Eu queria trocar o tamanho da memória/Velocidade da CPU/ems/soundblaster IRQ.
P: Quais hardwares de som o DOSBox emula atualmente?
P: DOSBox trava no início e eu estou rodando artes/cinemática.
P: Estou tendo problemas para rodar um jogo feito com a Build Engine (Duke3D/Blood/Shadow Warrior).
P: Excelente LEIAME, mas eu ainda não entendi.




P: Aparece um diretório Z ao invés de um C no prompt.
R: Você deve fazer seus diretórios ficarem disponíveis no DOSBox usando 
   o comando "mount". Por exemplo, no Windows "mount C D:\JOGOS" vai criar
   um drive C no DOSBox que leva ao seu diretório D:\JOGOS.
   No Linux, "mount c /home/username" criará um drive C no DOSBox
   que leva para o /home/username no Linux.
   Para mudar o drive montado, digite "C:". Se tudo ocorreu
   bem, DOSBox irá mostrar no prompt "C:\>".


P: Sempre deverei digitar esses comandos? Não existe uma forma automática?
R: No arquivo de configurações do DOSBox, existe uma seção [autoexec]. Os comandos
   ali escritos são executados quando o DOSBox iniciar, então você pode usar esta seção 
   para a montagem.


P: Como eu coloco tela cheia?
R: Pressione alt-enter. Alternativamente: Edite o arquivo de configurações do DOSBox e
   mude a opção fullscreen=false para fullscreen=true. Se na tela cheia lhe parece 
   errada: Altere a opção fullresolution no arquivo de configurações do
   DOSBox. Para sair do modo tela cheia: Pressione alt-enter novamente.


P: Meu CD-ROM não funciona.
R: Para montar seu CD-ROM no DOSBox, você deve especificar algumas opções adicionais 
   ao montar o CD-ROM. 
   Para ativar o suporte ao CD-ROM (incluindo MSCDEX):
     - mount d f:\ -t cdrom (windows)
     - mount d /media/cdrom -t cdrom (linux)

   Em alguns casos você pode querer usar uma interface CD-ROM diferente,
   por exemplo, caso o áudio do CD não funcione:
     Para ativar o suporte ao SDL (não inclui acesso do CD em baixo nível!):
       - mount d f:\ -t cdrom -usecd 0 -noioctl
     Para ativar o acesso do ioctl usando extração digital de áudio para CDs de áudio
     (apenas no windows, útil para o Vista):
       - mount d f:\ -t cdrom -ioctl_dx
     Para ativar o acesso do ioctl usando MCI para CDs de áudio (apenas no windows):
       - mount d f:\ -t cdrom -ioctl_mci
     Para forçar apenas o acesso pelo ioctl (apenas no windows):
       - mount d f:\ -t cdrom -ioctl_dio
     Para ativar o suporte aspi em baixo nível (win98 com aspi-layer instalado):
       - mount d f:\ -t cdrom -aspi
   
   Nos comandos:    - d   letra da unidade que aparecerá no DOSBox
                    - f:\ localização do CD-ROM no seu PC.
                    - 0   O número da unidade de CD-ROM, reportado pelo comando "mount -cd"
                          (note que este valor só é necessário quando o SDL é utilizado
                           para CDs de áudio, de qualquer outra maneira é ignorado)
   Veja também a próxima pergunta: O jogo/aplicativo não consegue achar seu CD-ROM.


P: O jogo/aplicativo não consegue achar seu CD-ROM.
R: Tenha certeza de montar o CD-ROM com o switch -t cdrom, isso ativará a
   interface MSCDEX requerida por jogos em DOS para se comunicar com os CD-ROMs.
   Tente também adicionar o rótulo correto (-label RÓTULO) no comando mount,
   onde RÓTULO é o rótulo do CD (ID do volume).
   No Windows, você pode especificar -ioctl, -aspi ou -noioctl. Olhe a 
   descrição do comando mount na Seção 4 para entender seus significados e 
   opções adicionais, relacionadas com CDs de áudio, -ioctl_dx, ioctl_mci, ioctl_dio.
   
   Tente criar uma imagem do CD-ROM (preferivelmente o par CUE/BIN) e usar a 
   ferramenta interna IMGMOUNT do DOSBox para montar a imagem (a lista CUE).
   Isso ativa o suporte ao CD-ROM em baixo nível para qualquer sistema operacional.


P: O mouse não funciona.
R: Normalmente, o DOSBox detecta quando um jogo utiliza o mouse. Quando você clica na 
   tela, o mouse deverá ser capturado pelo DOSBox e funcionar.
   Em alguns jogos a detecção do mouse não funciona. Nesses casos 
   você deverá capturar o mouse manualmente apertando CTRL-F10.


P: O aplicativo não tem som.
R: Tenha certeza que o som está configurado corretamente no jogo. Isso pode ser
   feito durante a instalação ou no programa setup/setsound que acompanha 
   o jogo. Primeiro, veja se existe uma opção para auto-detecção. Se não 
   existe, tente selecionar soundblaster ou soundblaster16 com as configurações
   padrões, sendo "address=220 irq=7 dma=1". Você talvez queira selecionar o
   midi no address 330 como dispositivo de música.
   Os parâmetros das placas de som emuladas podem ser alterados no arquivo de 
   configurações do DOSBox.
   Se continuar sem som, coloque o núcleo (core) em normal e use valores
   menores e fixos para os ciclos (como cycles=2000). Também assegure que 
   o seu sistema operacional está com som ativado.
   Em alguns casos, pode ser útil usar um dispositivo emulador de som diferente
   como soundblaster pro (sbtype=sbpro1 no arquivo de configurações do DOSBox) ou
   o gravis ultrasound (gus=true).


P: O som sai cortado ou estranho/difuso.
R: Você está exigindo muito da sua CPU para manter o DOSBox rodando naquela velocidade.
   Você pode diminuir os ciclos, pular quadros, reduzir a taxa de amostragem do
   dispositivo de som (veja o arquivo de configurações do DOSBox) ou
   o dispositivo mixador. Você pode também aumentar o prébuffer no arquivo de configurações do DOSBox.
   Se você está usando cycles=max ou =auto, então tenha certeza que não existe
   nenhum processo em segundo plano interferindo! (especialmente se estes acessam o disco rígido)


P: Eu não consigo digitar \ ou : no DOSBox.
R: Isso pode acontecer em várias situações, caso o layout do seu teclado não possua uma
   representação adequada no DOS (ou não foi detectada corretamente),
   ou o mapeamento das teclas está errado.
   Algumas possíveis soluções:
     1. Use /, ou ALT-58 para : e ALT-92 para \.
     2. Mude o layout do teclado do DOS (veja Seção 7: Layout do teclado).
     3. Adicione os comandos que você quer na seção [autoexec]
        do arquivo de configurações do DOSBox.
     4. Abra o arquivo de configurações do DOSBox e mude o valor do usescancodes.
     5. Mude o layout do teclado do seu sistema operacional.
   
   Note que se o layout do seu teclado não puder ser identificado, ou o keyboardlayout está ajustado
   para none no arquivo de configurações do DOSBox, o layout padrão dos EUA é usado.
   Nessa configuração tente as teclas ao redor do "enter" para a tecla \ (barra invertida),
   e para a tecla : (dois pontos) use shift e as teclas entre "enter" e "l".


P: Estou com atraso no teclado.
R: Diminua a prioridade no arquivo de configurações do DOSBox, por exemplo
   ajuste "priority=normal,normal". Você também pode tentar diminuir os ciclos
   (começe com um ciclo fixo, como cycles=10000).


P: O cursor está se movendo em apenas uma direção!
R: Veja se isso ainda acontece ao desabilitar a emulação do joystick,
   coloque joysticktype=none na seção [joystick] do arquivo de configurações
   do DOSBox. Tente também desconectar qualquer joystick/gamepad.
   Se você quiser usar o joystick no jogo, tente ajustar timed=false
   e tenha certeza de calibrar o joystick (no seu sistema operacional e 
   no jogo ou no programa de instalação do jogo).


P: O jogo/aplicativo roda muito lento!
R: Olhe a seção "Como rodar jogos que necessitam mais recursos"
   para maiores informações.   


P: O jogo/aplicativo não chega a rodar/trava!
R: Olhe a Seção 10: Solução de Problemas


P: O DOSBox pode danificar meu computador?
R: O DOSBox não pode danificar seu computador mais do que qualquer outro programa que
   necessita muitos recursos. Aumentando os ciclos não faz um overclock no seu CPU.
   Quando os ciclos estão muito altos a performance do software executado no DOSBox cai.


P: Eu queria trocar o tamanho da memória/velocidade da CPU/ems/soundblaster IRQ.
R: É possível! Apenas crie um arquivo de configurações: config -writeconf arquivodeconfigurações.
   Abra seu editor de texto favorito e modifique as configurações. Para iniciar o DOSBox
   com suas novas configurações: dosbox -conf arquivodeconfigurações
   Veja como funciona o comando config na Seção 4 para mais detalhes.


P: Quais hardwares de som o DOSBox emula atualmente?
R: DOSBox emula vários dispositivos de som:
   - Som interno do PC
     Essa emulação inclui o gerador de tons e várias formas de
     saída de som digital através da caixa de som interna.
   - Creative CMS/Gameblaster
     É a primeira placa feita pela Creative Labs(R). A localização padrão 
     da configuração está na porta 0x220. Nota-se que ao habilitar essa
     emulação com a emulação do Adlib poderá resultar em conflitos.
   - Tandy 3 voice
     A emulação deste hardware é completa, com a exceção do 
     canal de ruído. Esse canal não é muito bem documentado,
     com isso, seja talvez a melhor opção quanto a precisão do som.
   - Tandy DAC
     A emulação do Tandy DAC utiliza o emulador Soundblaster, então
     esteja certo de que o soundblaster está habilitado no arquivo de configurações
     do DOSBox. O Tandy DAC é emulado apenas em nível de BIOS.
   - Adlib
     Esta emulação é quase perfeita e inclui a habilidade do Adlib de 
     tocar quase todo som digitalizado.
   - SoundBlaster 16 / SoundBlaster Pro I & II / SoundBlaster I & II
     Por padrão, o DOSBox fornece Soundblaster 16 em 16-bit de som estéreo. 
     É possível selecionar uma versão diferente do SoundBlaster no arquivo de
     configurações do DOSBox (Ver Programas internos: CONFIG).
   - Disney Soundsource
     Usando a porta da impressora, esse dispositivo apenas gera som digital.
   - Gravis Ultrasound
     A emulação deste hardware está quase completa, apesar da capacidade
     de tocar MIDI ter sido deixada de lado, pois o MPU-401
     começou a ser emulado em outro código.
   - MPU-401
     A interface MIDI é também emulada. Esse método de saída som
     somente funcionará quando usado com um dispositivo de "General Midi" ou MT-32.


P: DOSBox trava no início e eu estou rodando artes/cinemática.
R: Esse não é realmente um problema no DOSBox, mas a solução é colocar a
   variável do ambiente SDL_AUDIODRIVER para alsa ou oss.


P: Estou tendo problemas para rodar um jogo feito com a Build Engine (Duke3D/Blood/Shadow Warrior).
R: Primeiramente, tente encontrar alguma portabilidade do jogo. Essas fornecerão uma 
   melhor velocidade. Para consertar problemas gráficos que ocorrem no  
   DOSBox em altas resoluções: Abra o arquivo de configurações do DOSBox
   e procure por machine=svga_s3. Mude de svga_s3 para vesa_nolfb


P: Excelente LEIAME, mas eu ainda não entendi.
R: O fórum "The Newbie's pictorial guide to DOSBox" localizado em 
   http://vogons.zetafleet.com/viewforum.php?f=39 poderá te ajudar.
   Tente também o wiki do DOSBox:
   http://www.dosbox.com/wiki/


Para mais perguntas, leia o restante desse LEIAME e/ou verifique o
site/fórum:
http://www.dosbox.com



=======
3. Uso:
=======

Aqui você terá uma visão ampla das opções da linha de comando que você
pode dar ao DOSBox. Usuários do Windows devem executar o cmd.exe,
command.com, ou editar um atalho para o DOSBox para isso.
Essas opções são válidas para qualquer sistema operacional,
menos quando indicado na descrição da opção:

dosbox [nome] [-exit] [-c comando] [-fullscreen] [-conf arquivodeconfig] 
       [-lang arquivodeidioma] [-machine tipodemáquina] [-noconsole]
       [-startmapper] [-noautoexec] [-securemode] 
       [-scaler escala | -forcescaler escala]
       [-version]
       
dosbox -version
dosbox -editconf programa
dosbox -opencaptures programa
dosbox -printconf
dosbox -eraseconf

  name   
        Se "nome" for um diretório, ele será montado como se fosse a unidade C:.
        Se "nome" for um executável, será montado o diretório de "name" como
        a unidade C: e então "nome" será executado.
    
  -exit  
        O DOSBox vai fechar automaticamente assim que o aplicativo "nome" for
        encerrado.

  -c comando
        Essa função executa comandos antes de "nome" ser executado.
        Vários comandos podem ser especificados, embora cada um
        deles deva ser iniciado com um "-c".
        Um comando pode ser: um Programa Interno, um comando do próprio
        DOS ou um executável que esteja em uma unidade emulada.

  -fullscreen
        Inicia o DOSBox já no modo de tela cheia.

  -conf arquivodeconfig
        Abre o DOSBox com as opções que foram especificadas em "arquivodeconfig".
        Várias opções -conf podem ser usadas.
        Para mais informações, veja a Seção 11.

  -lang arquivodeidioma
        Executa o DOSBox usando o idioma programado em "arquivodeidioma".

  -machine tipodemáquina
        Faz com que o DOSBox emule um determinado tipo de máquina. Algumas
        das opções são: hercules, cga, pcjr, tandy, svga_s3 (padrão) assim
        como os chipsets svga adicionais, que estão listadas no arquivo de
        configurações do DOSBox.
        O svga_s3 também permite uma emulação vesa.
        Para alguns efeitos especiais do vga, somente o tipo de máquina vgaonly
        poderá ser usado. Perceba que essa opção irá desabilitar as capacidades
        do svga, e poderá ficar (consideravelmente) mais lento devido à melhor
        precisão na emulação.
        A função machinetype afeta tanto as placas de vídeo como as placas de
        som disponíveis.

  -noconsole (Apenas Windows)
        Inicia o DOSBox sem mostrar a janela do console. O log de saída será
        redirecionado para stdout.txt e stderr.txt


  -startmapper
        Inicia o mapeador de teclas logo no inicio. Essa função é útil para pessoas
        que possuam algum tipo de problema no seu teclado.

  -noautoexec
        Esse comando "pula" a seção [autoexec] do arquivo de configurações
        que foi carregado.

  -securemode
        Possui a mesma função do -noautoexec, porém ele adiciona
        config.com -securemode no final do AUTOEXEC.BAT (o que, em troca,
        ignora quaisquer mudanças que foram feitas em relação à como as
        unidades são emuladas, dentro do DOSBox).

  -scaler escala
        Usa a escala que foi especificada em "escala". Abra o arquivo de
        configurações do DOSBox para verificar quais as escalas disponíveis.

  -forcescaler escala
        Função parecida com o parâmetro -scaler, mas esse força o
        uso da escala determinada, mesmo que ela não seja necessariamente
        adequada.

  -version
        Mostra informações sobre a versão e fecha o DOSBox.
        Útil para a criação de frontends.

  -editconf programa
        Chama "programa" como o primeiro parâmetro do arquivo de configurações.
        Você pode especificar esse comando mais de uma vez. Nesse caso,
        ele irá se dirigir ao segundo programa, caso o primeiro falhe na
        execução.

  -opencaptures programa
        Chama "programa" como o primeiro parâmetro a localização
        da pasta de captura.
  
  -printconf
        Imprime a localização do arquivo de configurações padrão.

  -eraseconf
        Exclui o arquivo de configurações padrão.

Nota: Se por acaso o nome/comando/arquivo de configurações/arquivo de
      idioma possuir algum espaço, ponha todo o nome/comando/arquivo
      de configurações/arquivo de idioma entre aspas:
      ("comando ou nome do arquivo"). Se você quiser usar aspas dentro
      das próprias aspas (provavelmente quando for usar -c e mount):
      Usuários do Windows e OS/2 podem usar aspas simples dentro de aspas
      duplas.
      Outros usuários poderão usar aspas com uma barra (\") dentro das aspas duplas.
      Windows: -c "mount c 'c:\program files\'" 
      Linux: -c "mount c \"/tmp/name with space\""

Aqui vai um exemplo:

dosbox c:\atlantis\atlantis.exe -c "MOUNT D C:\SAVES"
  Isso vai montar c:\atlantis como c:\ e executar atlantis.exe.
  Antes dele fazer isso ele vai montar C:\SAVES como sendo a unidade D.

No Windows, você também pode arrastar os diretórios/arquivos diretamente
para o executável do DOSBox.



======================
4. Programas Internos:
======================

O DOSBox suporta a maioria dos comandos do DOS encontrados no command.com.
Para ver uma lista dos programas internos digite "HELP" no prompt de comando.

Além desses, os seguintes comandos podem ser usados: 

MOUNT "Letra da Unidade emulada" "Unidade Real ou Diretório" 
      [-t tipo] [-aspi] [-ioctl] [-noioctl] [-usecd número] [-size tam_unidade] 
      [-label rótulo] [-freesize tam_em_mb]
      [-freesize tam_em_kb (disquetes)]
MOUNT -cd
MOUNT -u "Letra da Unidade emulada"

  Programa para montar diretórios locais como unidades dentro do DOSBox.

  "Letra da Unidade emulada"
        A letra da unidade dentro do DOSBox (ex. C).

  "Letra Real da Unidade (geralmente para CD-ROMs no Windows) ou Diretório"
        O diretório local que você deseja acessar com o DOSBox.

  -t tipo
        Tipo da unidade a ser montada. Os suportados são: dir (padrão),
        floppy (disquete), cdrom.

  -size tam_unidade
        Ajusta o tamanho da unidade, onde tam_unidade é pode ser
        "bps,spc,tcl,fcl":
           bps: bytes por setor. Por padrão, 512 para unidades convencionais
                e 2048 para unidades de CD-ROM
           spc: setores por cluster, geralmente entre 1 e 127
           tcl: total de clusters, entre 1 e 65534
           fcl: total de clusters livres, entre 1 e tcl

  -freesize tam_em_mb | tam_em_kb
        Ajusta a quantidade de espaço disponível na unidade em megabytes
        (unidades convencionais) ou kilobytes (unidades de disquete).
        Essa é uma versão mais simples de -size.	

  -label rótulo
        Ajusta o nome da unidade para "rótulo". É necessário em alguns
        sistemas caso o rótulo do CD-ROM não seja lido corretamente (útil quando
        o programa não consegue encontrar seu CD-ROM). Se o rótulo não for especificado
        ou o suporte de baixo nível não for selecionado (isto é, omitir os parâmetros
        -usecd # e/ou -aspi, ou especificando -noioctl): 
          No Windows: o rótulo será lido da "Unidade Real".
          No Linux: o rótulo será NO_LABEL.

        Se você especificar um rótulo, esse rótulo será mantido enquanto a unidade
        estiver montada. Ele não será atualizado em caso de mudança de CD, etc !!

  -aspi
        Força o uso da camada aspi. Só funciona montando a unidade de CD-ROM 
        em ambiente Windows que possuam a Camada ASPI.

  -ioctl (seleção automática da interface de CDs de áudio)
  -ioctl_dx (extração de áudio digital usado em CDs de áudio)
  -ioctl_dio (chamadas ioctl usadas em CDs de áudio)
  -ioctl_mci (MCI usado em CDs de áudio)
        Força o uso de comandos ioctl. Só é válido se a unidade de CD-ROM for montada 
        no Windows que suporte esses comandos (Win2000/XP/NT).
        As diferentes opções diferem apenas na maneira de como o CD de áudio é manuseado.
        Tenha preferência por -ioctl_dio (requer menor processamento), mas ele pode não
        funcionar em todos os sistemas, então, -ioctl_dx (ou -ioctl_mci) devem ser usados.

  -noioctl
        Força o uso da camada SDL do CD-ROM. Válido para qualquer sistema.

  -usecd número
        Válido para qualquer sistema, no Windows o parâmetro -noioctl deve
        estar presente para se poder fazer o uso do parâmetro -usecd.
        Ativa a seleção da unidade que deverá ser usada pelo SDL. Use isso se
        a unidade de CD-ROM não existir ou não estiver montada enquanto usa
        a interface de CD-ROM do SDL. "número" pode ser encontrado através de "MOUNT -cd".

  -cd
        Exibe todas as unidades de CD-ROM detectadas pelo SDL e seus respectivos
        números. Veja mais informações na seção -usecd acima.

  -u
        Desmonta a unidade. Não funciona para a Z:\.

  Observação: É possível montar um diretório local como unidade de CD-ROM. 
              Sendo assim, não existirá suporte de hardware.

  Basicamente o MOUNT permite você conectar o hardware do seu computador
  ao PC emulado pelo DOSBox.
  Então, "MOUNT C C:\JOGOS" diz ao DOSBox para usar a pasta C:\JOGOS como a unidade C:
  dentro do DOSBox. Ele também permite que você mude a letra da unidade para
  programas que necessitam unidades com letras específicas.
  
  Por exemplo: "Touche: Adventures of The Fifth Musketeer" deve ser executado
  na unidade C:. Usando o DOSBox juntamente com seu comando mount, você pode
  fazer com que o jogo pense que ele está na unidade C, enquanto ele está em
  qualquer outro lugar que você queira colocar. Por exemplo, se o jogo estiver
  em D:\JOGOSANTIGOS\TOUCHE, o comando "MOUNT C D:\JOGOSANTIGOS" permitirá você
  executar o Touche a partir da unidade D.

  Montar a sua unidade C inteira com "MOUNT C C:\" NÃO é recomendado! O mesmo
  é válido ao montar a raiz de qualquer outra unidade, exceto para CD-ROMs (devido
  sua natureza de somente leitura). Se não, caso você ou o DOSBox faça algum erro,
  você poderá perder todos os seus arquivos.
  É recomendado colocar todos os seus aplicativos/jogos em um subdiretório e montá-lo.

  Exemplos gerais de MOUNT:
    1. Para montar c:\DirX como um disquete : 
         mount a c:\DirX -t floppy
    2. Para montar a unidade de CD-ROM E: do sistema como unidade de CD-ROM D: no DOSBox:
         mount d e:\ -t cdrom
    3. Para montar a unidade de CD-ROM do sistema como pontodemontagem /mídia/cdrom como
       unidade de CD-ROM D: no DOSBox:
         mount d /media/cdrom -t cdrom -usecd 0
    4. Para montar uma unidade com ~870 MB de espaço disponível (versão simples):
         mount c d:\ -freesize 870
    5. Para montar uma unidade com ~870 MB de espaço disponível (modo avançado, controle total):
         mount c d:\ -size 512,127,16513,13500
    6. Para montar /home/user/dirY como unidade C no DOSBox:
         mount c /home/user/dirY
    7. Para montar o diretório onde o DOSBox foi iniciado como D no DOSBox:
         mount d .
         (note o . que representa a pasta onde o DOSBox foi iniciado)


MEM
  Programa para mostrar a quantidade de memória disponível.


VER
VER set versão_maior [versão_menor]
  Mostra a versão atual do DOSBox e a versão relatada do DOS
  (uso sem parâmetros).
  Altera a versão relatada do DOS com o parâmetro "set",
  por exemplo: escreva "VER set 6 22" para que a versão relatada
  do DOS pelo DOSBox seja a 6.22.


CONFIG -writeconf localdoarquivo
CONFIG -writelang localdoarquivo
CONFIG -securemode
CONFIG -set "seção propriedade=valor"
CONFIG -get "seção propriedade"

  O CONFIG pode ser usado para alterar ou obter informações de várias configurações do DOSBox 
  durante sua execução. Ele pode salvar as configurações atuais e os textos do idioma para um
  arquivo no disco rígido. Informações sobre todas as possíveis seções e propriedades podem ser
  encontradas na Seção 11 (O Arquivo de Configurações).

  -writeconf localdoarquivo
       Escreve as atuais configurações do DOSBox em um arquivo. "localdoarquivo" é
       um local do seu disco rígido, não uma unidade montada no DOSBox. 
       O arquivo de configurações contém vários ajustes do DOSBox: 
       a quantidade de memória emulada, as placas de som emuladas dentre muitas outras
       coisas. Ele também permite acesso ao AUTOEXEC.BAT.
       Veja a Seção 11 (O Arquivo de Configurações) para maiores informações.

  -writelang localdoarquivo
       Escreve os textos do idioma atual em um arquivo. "localdoarquivo" é
       um local do seu disco rígido, não uma unidade montada no DOSBox.
       O arquivo de idioma controla toda a saída visível dos comandos internos
       do DOSBox e do DOS interno.

  -securemode
       Alterna o DOSBox para um modo mais seguro. Nesse modo, os comandos internos
       MOUNT, IMGMOUNT e BOOT não funcionarão. Também não é possível criar um novo
       arquivo de configurações ou de idiomas enquanto estiver nesse modo.
       (Atenção: a única maneira de sair desse modo é reiniciando o DOSBox.)

  -set "seção propriedade=valor"
       O CONFIG tentará ajustar um novo valor para uma certa propriedade. Atualmente
       o CONFIG não tem como informar se o comando foi concluído com sucesso ou não.

  -get "seção propriedade"
       O valor atual de uma determinada propriedade é relatada e gravada na variável
       ambiente %CONFIG%. Isso pode ser usado para gravar um determinado valor quando
       estiver usando arquivos batch.

  Ambos "-set" e "-get" funcionam a partir de arquivos batch e podem ser usados para
  ajustar suas próprias configurações para cada jogo.
  
  Exemplos:
    1. Para criar um arquivo de configurações no diretório atual:
        config -writeconf dosbox.conf
    2. Para ajustar os ciclos da CPU em 10000:
        config -set "cpu cycles=10000"
    3. Para desligar a emulação da memória ems:
        config -set "dos ems=off"
    4. Para verificar qual núcleo da CPU está sendo usada.
        config -get "cpu core"


LOADFIX [-tamanho] [programa] [parâmetros-do-programa]
LOADFIX -f
  Programa usado para reduzir a quantidade de memória convencional disponível.
  Útil para programas antigos que não esperam que muita memória esteja disponível. 

  -tamanho	        
        número de kilobytes para "comer" (reduzir), padrão = 64kb
  
  -f
        libera todas as memórias alocadas anteriormente
  
  Exemplos:
    1. Para iniciar mm2.exe e alocar uma memória de 64kb 
       (mm2 terá menos 64 kb de memória disponível) :
       loadfix mm2
    2. Para iniciar mm2.exe e alocar uma memória de 32kb :
       loadfix -32 mm2
    3. Para liberar as memórias anteriormente alocadas :
       loadfix -f


RESCAN
  Faz o DOSBox reanalizar a estrutura do diretório. Útil caso você tenha
  alterado algo, em uma das unidades montadas, fora do DOSBox. (CTRL - F4 é um atalho!)
  

MIXER
  Faz o DOSBox exibir suas configurações atuais do volume. 
  Logo abaixo está como alterá-las:
  
  mixer canal esquerdo:direito [/NOSHOW] [/LISTMIDI]
  
  canal
      São opções possíveis: MASTER, DISNEY, SPKR, GUS, SB, FM [, CDAUDIO].
      CDAUDIO está disponível apenas se a interface do CD-ROM com controle
      de volume estiver ativada (imagem do CD, ioctl_dx).
  
  esquerdo:direito
      Os níveis de volume em porcentagens. Se você colocar um D na frente o
      valor será em decibéis (Exemplo: mixer gus d-10).
  
  /NOSHOW
      Previne o DOSBox de mostrar o resultado caso você tenha ajustado algum
      dos níveis de volume.

  /LISTMIDI
      Lista os dispositivos MIDI disponíveis no seu PC (Windows). Para escolher
      um dispositivo que não seja o mapeador de midi padrão do Windows, adicione
      uma linha 'midiconfig=id' na seção [midi] no arquivo de configurações,
      onde 'id' é o número do dispositivo listado pelo LISTMIDI.


IMGMOUNT
  É um utilitário para montar imagens de disco ou de CD-ROM dentro do DOSBox.
  
  IMGMOUNT UNIDADE [arquivo_imagem] -t [tipo_imagem] -fs [formato_imagem] 
            -size [setoresporbyte, setoresporcabeça, cabeças, cilindros]
  IMGMOUNT UNIDADE [arquivodeimagem1, .. ,arquivodeimagemN] -t iso -fs iso 

  arquivo_imagem
      Local do arquivo de imagem para montar no DOSBox. O local pode
      ser uma unidade montada dentro do DOSBox, ou o seu disco rígido real.
      Também é possível montar imagens de CD-ROM (ISOs ou CUE/BIN), caso
      você necessite trocar o CD em um determinado momento especifique
      todas as imagens em sucessão (veja próximo tópico).
      Os pares CUE/BIN são melhores opções para imagens de CD-ROM pois elas
      guardam informações sobre faixas de áudio em comparação com ISOs
      (que armazenam apenas dados). Para montar um par CUE/BIN sempre
      especifique a lista CUE.
      
  arquivodeimagem1, .. ,arquivodeimagemN
      Local dos arquivos de imagem para montar no DOSBox. Só é permitido
      especificar um número de imagens para imagens de CD-ROM. Os CDs
      podem ser trocados, a qualquer momento, com CTRL-F4. Isso é necessário
      para jogos que usam vários CD-ROMs e que precisem, em um determinado
      ponto do jogo, que os CDs sejam trocados.
   
  -t 
      A seguir estão os tipos de imagens válidas:
        floppy: Especifica uma imagem de disquete. O DOSBox automaticamente
                identificará a geometria do disco (360K, 1.2MB, 720K, 1.44MB, etc).
        iso:    Especifica uma imagem de CD-ROM. A geometria é automática e
                é ajustada para seu tamanho. Pode ser uma iso ou um par cue/bin.
        hdd:    Especifica uma imagem de disco rígido. A geometria CHS apropriada
                deve ser colocada para que isso funcione.

  -fs 
      A seguir estão os sistemas de arquivos válidos:
        iso:  Especifica o formato CD-ROM ISO 9660.
        fat:  Especifica que a imagem usa o sistema de arquivos FAT. O DOSBox tentará
              montar essa imagem como uma unidade no DOSBox e fazer com que seus arquivos
              estejam disponíveis dentro do DOSBox.
        none: O DOSBox não lerá o sistema de arquivos do disco.
              É útil caso você precise formatar ou caso você queira iniciar
              o disco através do comando BOOT. Quando o sistema de arquivo "none" 
              é usado, você deve especificar o número da unidade (2 ou 3, 
              onde 2 = master, 3 = slave) ao invés da letra da unidade. 
              Por exemplo, para montar uma imagem de 70MB como uma unidade slave, 
              você deverá escrever (sem as aspas):
                "imgmount 3 d:\teste.img -size 512,63,16,142 -fs none" 
                Compare isso com o mount para ser possível acessar a unidade
                pelo DOSBox, que deveria ser: 
                "imgmount e: d:\teste.img -size 512,63,16,142"

  -size 
     Os Cilindros, Cabeças e Setores (CHS) da unidade.
     Necessário para montar imagens de disco rígido.
     
  Um exemplo de como montar imagens de CD-ROM:
    1a. mount c /tmp
    1b. imgmount d c:\minhaiso.iso -t iso
    ou (que funciona igual):
    2. imgmount d /tmp/minhaiso.iso -t iso


BOOT
  O BOOT inicia imagens de disquete ou de disco rígido independente da
  emulação do sistema operacional oferecido pelo DOSBox. Isso permite
  jogar bootáveis de disquetes ou iniciar outros sistemas operacionais
  dentro do DOSBox. Se o sistema emulado for PCjr (machine=pcjr) o
  comando de boot pode ser usado para carregar cartuchos PCjr (.jrc). 

  BOOT [diskimg1.img diskimg2.img .. diskimgN.img] [-l letradaunidade]
  BOOT [cart.jrc]  (apenas PCjr)

  diskimgN.img 
     Isso é número de imagens de disquete que você deseja que sejam montadas
     depois do DOSBox iniciar a letra da unidade especificada.
     Para alternar entre as imagens, aperte CTRL-F4 para mudar do disco atual
     para o disco seguinte da lista. Quando chegar ao último disco da lista
     o próximo será o primeiro.

  [-l letradaunidade]
     Esse parâmetro permite que você especifique de qual unidade será inicializado.
     A unidade padrão é A:, a unidade de disquete. Você também pode iniciar uma
     imagem de disco rígido montada como "master" ao especificar "-l C" 
     (sem as aspas) ou como "slave" ao especificar "-l D"
     
   cart.jrc (apenas PCjr)
     Quando a emulação do PCjr está ativada, cartuchos podem ser carregados com
     o comando BOOT. O suporte ainda é limitado.


IPX

  É preciso ativar a rede IPX no arquivo de configurações do DOSBox.

  Toda a rede IPX é gerenciada pelo programa interno do DOSBox,
  IPXNET. Para obter ajuda sobre a rede IPX dentro do DOSBox, digite
  "IPXNET HELP" (sem aspas) e o programa listará os comandos com uma
  documentação relevante.

  Em relação a como ajustar uma rede, um sistema precisa ser o servidor.
  Para fazê-lo, digite "IPXNET STARTSERVER" (sem as aspas) numa sessão do
  DOSBox. A sessão onde o DOSBox está sendo servidor automaticamente
  se adicionará para a rede virtual IPX. Para cada computador adicional
  que deverá fazer parte da rede virtual IPX, você deverá digitar
  "IPXNET CONNECT <nome do computador host ou IP>". 
  Por exemplo, se o servidor estiver no joao.dosbox.com, você deverá digitar
  "IPXNET CONNECT joao.dosbox.com" em cada sistema que não seja o servidor. 
  
  Para jogar os jogos que precisam do Netbios, o arquivo NETBIOS.EXE da Novell
  é necessário. Estabeleça a conexão IPX como explicado acima, e então
  execute o "netbios.exe". 

  A seguir estão as referências do comando IPXNET: 

  IPXNET CONNECT 

     IPXNET CONNECT abre uma conexão e cria um túnel com o servidor IPX
     rodando em outra sessão do DOSBox. O parâmetro "endereço" especifica
     o endereço de IP ou nome do host do computador servidor. Você também
     pode especificar qual a porta UDP que será usada. Por padrão, o IPXNET
     usa a porta 213 - que é a porta IANA associada para túneis IPX - para
     sua conexão. 

     A sintaxe para o IPXNET CONNECT é: 
     IPXNET CONNECT endereço <porta> 

  IPXNET DISCONNECT 

     IPXNET DISCONNECT fecha o túnel da conexão com o servidor IPX. 

     A sintaxe para o IPXNET DISCONNECT é: 
     IPXNET DISCONNECT 

  IPXNET STARTSERVER 

     IPXNET STARTSERVER inicia um servidor IPX nessa sessão do DOSBox.
     Por padrão, o servidor aceitará conexões na porta 213 UDP,
     mas isso pode ser alterado. Uma vez que o servidor for iniciado, o
     DOSBox automaticamente iniciará uma conexão com o cliente através do
     túnel do servidor IPX.

     A sintaxe para o IPXNET STARTSERVER é:
     IPXNET STARTSERVER <porta>

     Se o servidor estiver por trás de um router, a porta UDP <porta> deverá ser
     encaminhada àquele computador (fazer um port-fowarding).

     Nos sistemas Linux/Unix-based: Números menores que 1023 para as portas só
     podem ser usadas com privilégios de root. Use portas maiores que 1023
     nesses sistemas.

  IPXNET STOPSERVER

     IPXNET STOPSERVER fecha o túnel do servidor IPX rodando nessa sessão do
     DOSBox. Um certo cuidado deve ser tomado; assegure-se que todas as outras
     conexões tenham sido terminadas antes, pois deixar de ser um servidor poderá
     travar as outras máquinas que ainda estão usando o túnel de conexão com o
     servidor.

     A sintaxe para o IPXNET STOPSERVER é: 
     IPXNET STOPSERVER 

  IPXNET PING

     IPXNET PING emite um pedido de ping através do túnel da rede IPX. 
     Em resposta, todos os outros computadores conectados responderão ao ping
     e reportarão o tempo que levou para receber e enviar a mensagem de ping. 

     A sintaxe para o IPXNET PING é: 
     IPXNET PING

  IPXNET STATUS

     IPXNET STATUS mostra o estado atual do túnel da rede IPX da sessão
     atual do DOSBox. Para ver a lista de todos os computadores conectados
     na rede use o comando IPXNET PING.

     A sintaxe para o IPXNET STATUS é: 
     IPXNET STATUS 


KEYB [códigodeidioma [codepage [arquivodecodepage]]]
  Altera o layout do teclado. Para informações detalhadas sobre os layouts
  do teclado veja a Seção 7.

  [códigodeidioma] são os caracteres que especificam o layout do teclado
     que será usado. Geralmente são 2 caracteres mas, em casos especiais, mais.
     Exemplos são GK (Grécia) ou IT (Itália).

  [codepage] é o número da codepage a ser usada. O layout do teclado deve
     apresentar suporte para a codepage especificada, caso contrário o
     carregamento layout falhará.
     Se nenhuma codepage for especificada, uma codepage apropriada para o
     layout escolhido é automaticamente selecionado.

  [arquivodecodepage] pode ser usado para carregar codepages que ainda não estão
     compiladas no DOSBox. Só é necessário quando o DOSBox não encontra a codepage.


  Exemplos:
    1. Para carregar o layout do teclado alemão (automaticamente usa a codepage 858):
         keyb gr
    2. Para carregar o layout do teclado russo com codepage 866:
         keyb ru 866
       Para escrever os caracteres russos aperte ALT+SHIFT DIREITO.
    3. Para carregar o layout do teclado francês com codepage 850 (onde a
       codepage está no arquivo EGACPI.DAT):
         keyb fr 850 EGACPI.DAT
    4. Para carregar a codepage 858 (sem o layout do teclado):
         keyb none 858
       Isso pode ser usado para mudar a codepage do utilitário FreeDOS keyb2.
    5. Para exibir a codepage atual e, se carregado, o layout do teclado:
         keyb



Para maiores informações, use o switch /? na linha de comando com os programas.



====================
5. Teclas Especiais:
====================


ALT-ENTER   : Alterna entre o modo janela e tela-cheia.
ALT-PAUSE   : Pausa o DOSBox. (aperte ALT-PAUSE novamente para continuar)
CTRL-F1     : Executa o mapeador de teclas.
CTRL-F4     : Alterna entre as imagens dos discos montadas.
              Atualiza a cache dos diretórios de todas as unidades.            
CTRL-ALT-F5 : Inicia/Para a gravação de vídeo à um arquivo. (formato AVI)
CTRL-F5     : Captura e salva a imagem da tela. (formato PNG)
CTRL-F6     : Inicia/Para a gravação do áudio à um arquivo wave.
CTRL-ALT-F7 : Inicia/Para a gravação de comandos OPL. (formato DRO)
CTRL-ALT-F8 : Inicia/Para a gravação de comandos MIDI sem processar.
CTRL-F7     : Diminui o frameskip.
CTRL-F8     : Aumenta o frameskip.
CTRL-F9     : Encerra o DOSBox.
CTRL-F10    : Captura/Libera o mouse.
CTRL-F11    : Desacelera a emulação (Diminui os ciclos do DOSBox).
CTRL-F12    : Acelera a emulação (Aumenta os ciclos do DOSBox).
ALT-F12     : Desativa o limitador de velocidade (Botão turbo).


(NOTA: Uma vez que você ultrapassar o máximo de ciclos que o computador
suporta, a velocidade começará a diminuir. Esse máximo varia para cada
computador.)

Essas são atribuições de teclas padrão. Elas podem ser alteradas através
do mapeador de teclas. (Veja seção 6: Mapeador de teclas)

Arquivos salvos/gravados podem ser encontrados em diretório_atual/capture 
(isso pode ser alterado no arquivo de configurações do DOSBox). 
Essa pasta deverá existir antes do DOSBox ser iniciado, se não nada será
salvo/gravado ! 



======================
6. Mapeador de Teclas:
======================

Quando o mapeador de teclas do DOSBox é iniciado (Pressionando CTRL+F1 ou
escrevendo o parâmetro -startmapper na linha de comando do executável)
na tela será exibido um teclado virtual e um joystick virtual.

Esses dispositivos virtuais corresponderão às teclas e eventos que o
DOSBox irá relatar para as aplicações DOS (que está sendo emulado).
Quando você clicar em um botão com o mouse, no canto inferior esquerdo
aparecerá o evento a que está associado (EVENT) e com que tecla está
atualmente atribuída.

Event: EVENT
BIND: BIND
                        Add   Del
mod1  hold                    Next
mod2
mod3


EVENT
    A tecla ou direção/botão do joystick que será relatada pelo DOSBox
    às aplicações DOS.
BIND
    Que tecla do seu teclado ou botão/direção do seu joystick (os de verdade)
    (o mostrado pelo SDL) que estão associadas ao EVENT.

mod1,2,3 
    Modificadores. Essas são as teclas que também deverão ser pressionadas
    enquanto você aperta a tecla associada (BIND). mod1 = CTRL e mod2 = ALT.
    Elas normalmente só são usadas quando você quer mudar as teclas especiais
    do DOSBox.
Add 
    Adiciona uma nova BIND ao EVENT. Basicamente adiciona uma tecla do seu
    teclado ou um botão/direção do seu joystick que irá resultar num EVENT
    no DOSBox.
Del 
    Apaga o BIND associado a esse EVENT. Se o EVENT não possui nenhum BIND,
    então não é possível ativá-lo no DOSBox (ou seja, não há como usar
    a tecla ou usar a respectiva ação no joystick).
Next
    Mostra quais as BINDS (teclas) estão atribuídas ao determinado EVENT.


Exemplo:
P1. Você quer que o X do seu teclado digite um Z no DOSBox.
    R. Clique no Z do mapeador de teclas. Clique em "Add". Agora
       pressione o X do seu teclado.

P2. Se você clicar em "Next" algumas vezes, verá que o Z do seu teclado
    também faz aparecer um Z no DOSBox.
    R. Sendo assim, selecione o Z novamente e clique em "Next" até que o BIND
       seja o Z do seu teclado. Agora clique em "Del".

P3. Se você apertar X no DOSBox perceberá o aparecimento de XZ.
    R. O X do seu teclado também está mapeado como X!! Clique no X do mapeador
    de teclas e procure com o "Next" até que você ache o X mapeado. Após isso,
    clique em "Del".


Exemplos sobre o remapeamento do joystick:
  Você tem um joystick conectado, que funciona muito bem com o DOSBox, mas você
  quer jogar um jogo, que só aceita o teclado, com o joystick (assume-se que o
  jogo seja controlado pelas setinhas do teclado):
    1. Inicie o mapeador, clique em uma das setas localizadas no
       centro-esquerdo da tela (logo acima dos botões Mod1/Mod2).
       O EVENT deve ser key_left. Agora clique em Add e mova o seu joystick na
       respectiva direção. Com isso, você deve ter adicionado uma BIND ao evento.
    2. Repita os passos anteriores para a adição das três direções restantes.
       É claro que os botões do joystick também podem ser remapeados (atirar,
       pular, etc.)
    3. Clique em Save, depois em Exit e teste sua nova configuração com algum jogo.

  Você quer inverter o eixo y do joystick porque alguns simuladores de vôo usam
  esses movimentos de uma maneira que você não gosta e não é configurável no jogo:
    1. Inicie o mapeador e clique no Y- no campo superior direito
       (essa opção é para o primeiro joystick, caso você tenha dois deles
       conectados), ou no campo logo abaixo do anterior (essa opção é para
       o segundo joystick ou, caso você só tenha um encaixado, o segundo
       eixo se inverte).
       O EVENT deverá ser jaxis_0_1- (ou jaxis_1_1-).
    2. Clique em Del para remover a BIND atual, e depois clique em Add e mova o
       seu joystick para baixo. Feito isso, uma nova bind deverá criada.
    3. Repita essa operação para configurar o Y+, salve sua configuração e
       teste-a em algum jogo.



Se você modificar o mapeamento padrão, poderá salvar suas alterações clicando
em "Save". O DOSBox vai salvar esse mapeamento no local especificado no arquivo
de configurações (opção mapperfile=). Na inicialização, o DOSBox vai
carregar o seu arquivo de mapeamento, se o mesmo estiver preenchido
no arquivo de configurações.



=====================
7. Layout do teclado:
=====================

Para mudar o layout do teclado basta preencher a configuração "keyboardlayout"
na seção [dos] do arquivo de configurações do DOSBox ou utilizar o programa
interno keyb.com do DOSBox. Ambos aceitam códigos de idioma (veja abaixo), mas
somente é possível especificar uma codepage customizada através do programa
keyb.com.

A opção "keyboardlayout=auto" atualmente só funciona em ambiente Windows.
O layout é escolhido baseado no qual o sistema operacional está usando.

Mudando o Layout
  Por padrão, o DOSBox já suporta um certo número de layouts de teclado
  e codepages. Nesse caso, apenas o identificador do layout precisa ser
  especificado (ex: keyboardlayout=br no arquivo de configuração do
  DOSBox, ou escrevendo "keyb br" no prompt de comando do DOSBox).
  
  Alguns layouts de teclado (por exemplo, o layout GK codepage 869 e o layout RU
  codepage 808) têm suporte para layout duplo que pode ser ativado pressionando
  ALT ESQUERDO+SHIFT DIREITO e desativado usando ALT ESQUERDO+SHIFT ESQUERDO.

Arquivos externos suportados
  Os arquivos .kl do FreeDOS são suportados (arquivos de layout do FreeDOS keyb2)
  assim como suas bibliotecas, (keyboard.sys/keybrd.sys/keybrd3.sys) que consistem
  na união dos arquivos .kl existentes.
  Caso os layouts integrados do DOSBox não funcionem, acesse o site
  http://projects.freedos.net/keyb/ para obter layouts de teclado precompilados
  com suas devidas atualizações, caso estejam disponíveis.

  Os arquivos .CPI (Arquivos MS-DOS e de codepages compatíveis) e .CPX (Arquivos de
  codepage UPX-condesadas do FreeDOS) podem ser usados. Algumas codepages já foram
  pré-compiladas no DOSBox. Não há necessidade de se preocupar com arquivo de
  codepage externo. Se você necessitar de um arquivo de codepage diferente
  (ou customizado), copie ele para o diretório do arquivo de configuração do DOSBox
  para que o DOSBox possa acessá-lo.

  Layouts adicionais podem ser aplicados copiando o arquivo .kl para o
  diretório do arquivo de configurações do DOSBox e usando a primeira parte do
  nome do arquivo como o código do idioma.
  Exemplo: Para o arquivo UZ.KL (layout de teclado para o Uzbequistão), especifique
           "keyboardlayout=uz" no arquivo de configurações do DOSBox.
  A integração das bibliotecas de layouts de teclado (ex: keybrd2.sys) funciona da
  mesma maneira.


Observe que o layout do teclado faz com que os caracteres estrangeiros apareçam, mas
atualmente NÃO há suporte para eles nos nomes dos arquivos. Evite-os tanto no DOSBox
quanto nos arquivos do seu computador que estão sendo acessados pelo DOSBox.



===================================
8. Função Multi-Jogador via Serial:
===================================

O DOSBox pode emular cabos seriais e nullmodem através da rede local e da internet.
Essa opção pode ser configurada através da seção "Porta Serial" no arquivo de
configuração do DOSBox.

Para estabelecer uma conexão Null Modem, uma pessoa deverá ser o servidor
e o outra deverá ser o cliente.

Quem decidir ser o servidor terá que ajustar o arquivo de configuração
do DOSBox, da seguinte maneira:
   serial1=nullmodem

E o arquivo de configuração do DOSBox no cliente deverá ficar assim:
   serial1=nullmodem server:<IP ou o nome do servidor>

Agora inicie o jogo e escolha, na COM1, uma dentre essas opções de multiplayer:
Nullmodem / Cabo Serial / Já conectado. Lembre-se de deixar as baudrates iguais
nos dois computadores.

Além disso, outros parâmetros podem ser ajustados para melhorar a conexão
do Null Modem. Estes são os parâmetros disponíveis:

 * port:         - Número da porta TCP. Padrão: 23
 * rxdelay:      - Em quanto tempo (milissegundos) os dados recebidos serão atrasados,
                   caso a interface ainda não esteja pronta. Aumente esse valor
                   se você encontrar erros na Janela de Status do DOSBox. Padrão: 100
 * txdelay:      - Quanto tempo levar para que as informações sejam
                   acumuladas, antes de serem enviadas de uma vez em um pacote.
                   Padrão:12
                   (Isso diminui a sobrecarga do servidor)
 * server:       - Esse Null Modem será um cliente se conectando à um servidor
                   previamente especificado.
                   (Se não houver servidor: Seja um.)
 * transparent:1 - Apenas enviar as informações do serial, sem nenhum handshake
                   de RTS/DTR. Use essa opção quando estiver se conectando via
                   algo que não seja um Null Modem.
 * telnet:1      - Interpreta informações de Telnet provenientes do site remoto.
                   Automaticamente ativa a transparência.
 * usedtr:1      - A conexão não será estabelecida até que o DTR seja ligado pelo
                   programa DOS. Útil para terminais de modem.
 * inhsocket:1   - Usar um socket que será enviado ao DOSBox através de uma linha
                   de comando. Automaticamente ativa a transparência.
                   (Compatibilidade do Socket: É usado para rodar jogos do DOS
                   antigos em softwares da BBS novos.)

Exemplo: Para ser um servidor conectado à Porta TCP 5000
   serial1=nullmodem server:<IP ou nome do servidor> port:5000 rxdelay:1000



============================================================
9. Como rodar jogos que requerem mais recursos do computador 
============================================================

O DOSBox emula a CPU, as placas de som e de vídeos, e outros periféricos
de um computador, tudo de uma só vez. A velocidade do aplicativo emulado
no DOS depende de quantas instruções podem ser emuladas.
Esse valor é ajustável (número de ciclos).

Ciclos da CPU
  Por padrão (cycles=auto). O DOSBox tentará descobrir quanto um jogo
  necessita para executar as tantas instruções emuladas por intervalo de
  tempo possível. É possível forçar esse comportamento ajustando os ciclos
  para cycles=max no arquivo de configurações do DOSBox.
  A janela do DOSBox mostrará uma linha "Cpu Cycles: max" na barra de título.
  Nesse modo é possível reduzir a quantidade de ciclos por percentual
  (CTRL-F11) ou aumentar novamente (CTRL-F12).
  
  As vezes obtem-se melhores resultados ajustando manualmente o número de ciclos.
  Ajuste, por exemplo, no arquivo de configurações do DOSBox cycles=30000.
  É possível aumentar ainda mais o número de ciclos, enquanto você roda algum
  programa do DOS, apertando CTRL-F12 mas esse valor será limitado pela potência
  da sua CPU. Você pode ver o quanto de carga está sendo processado pela sua CPU
  pelo Gerenciador de Tarefas no Windows 2000/XP/Vista e pelo Monitor de Sistema
  no Windows 95/98/ME. Uma vez que 100% do seu processador real esteja em uso não
  será mais possível aumentar a velocidade do DOSBox, a menos que você reduza a
  carga de processamento de outros aplicativos que não sejam o do DOSBox.

Núcleos da CPU
  Em arquiteturas x86 (32 bits) você pode tentar forçar o uso de um núcleo
  recompilador dinâmico (coloque core=dynamic no arquivo de configurações).
  Isso geralmente trás melhores resultados se a auto detecção (core=auto) falhar.
  A melhor opção é colocar cycles=max. Mas note que poderão existir jogos
  que funcionem pior do que com o núcleo dinâmico, ou que até nem funcionem.

Emulação de gráficos
  A emulação do VGA é uma parte bastante delicada do DOSBox em termos de
  uso da CPU. Aumentar o número de quadros que não serão emulados (frameskip)
  apertando CTRL-F8 (incrementos de um) poderão ajudar no aumento da velocidade.
  A carga sobre a CPU deverá diminuir quando o número de ciclos está fixo.
  Repita esse procedimento até que o jogo tenha uma velocidade agradável para
  você. Note que isso é uma troca: o que você perde em fluidez do vídeo você ganha
  em velocidade.

Emulação de som
  Você pode tentar desativar o som através do instalador do jogo para reduzir
  a quantidade de informações que deverão ser processadas. Colocar nosound=true
  NÃO desativa a emulação dos dispositivos de som, apenas a saída de som será
  desativada.

Tente fechar todos os programas em execução, exceto o DOSBox, para aumentar ainda
mais os recursos para o DOSBox.


Configurações avançadas dos ciclos:
As opções cycles=auto e cycles=max podem ser parametrizadas para possuírem
diferentes padrões de inicialização. A sintaxe é
  cycles=auto ["padrão modoreal"] ["padrão modo protegido"%] 
              [limite do "limite do ciclo"]
  cycles=max ["padrão modo protegido"%] [limite do "limite do ciclo"]
Exemplo:
  cycles=auto 1000 80% limit 20000
  utilizará cycles=1000 para jogos em modo real, 80% da aceleração da CPU
  para jogos no modo protegido juntamente com um limite rígido de 20000 ciclos



=========================
10. Solução de Problemas:
=========================

O DOSBox não inicia:
  - use valores diferentes para a opção output= no arquivo de configuração do DOSBox
  - atualize os drivers da sua placa de vídeo e o DirectX

Executar certos tipos de jogos faz o DOSBox fechar, mostrar mensagens de erro ou parar
de responder:
  - verifique se o jogo funciona com a instalação padrão do DOSBox
    (com arquivo de configuração não modificado)
  - tente jogar com o som desativado (altere as configurações do som
    no instalador do jogo. Tente também usar sbtype=none e gus=false no arquivo
    de configurações do DOSBox)
  - tente alterar algumas opções no arquivo de configurações, especialmente:
      core=normal
      ciclos fixos (por exemplo cycles=10000)
      ems=false
      xms=false
    ou combinações para as opções acima,
    use configurações similares a da máquina que controla
    o chipset emulado e funcionalidade:
      machine=vesa_nolfb
    ou
      machine=vgaonly
  - use o loadfix antes de executar o jogo

O jogo é finalizado e retorna ao prompt do DOSBox com uma mensagem de erro:
  - leia a mensagem com atenção e tente localizar o erro
  - utilize as dicas nas seções acima descritas
  - monte em outros diretórios pois alguns jogos são chatos quanto a localização,
    por exemplo, se você utilizou "mount d d:\jogosantigos\jogox" tente
    "mount c d:\jogosantigos\jogo" ou "mount c d:\jogosantigos"
  - se o jogo necessitar de uma unidade de CD-ROM verifique se você digitou "-t cdrom"
    ao montar a unidade e tente usar outros parâmetros adicionais (Os parâmetros
    ioctl, usecd e label. Veja na seção apropriada)
  - verifique as permissões dos arquivos do jogo (desmarque os atributos
    de somente-leitura e adicione permições para gravações etc.)
  - tente reinstalar o jogo pelo dosbox

===============================
11. O Arquivo de Configurações:
===============================

O arquivo com as configurações pode gerado pelo CONFIG.COM, que está localizado
na unidade interna (Z:) do dosbox. Leia a seção "Programas Internos" para
entender o funcionamento do CONFIG.COM.
É possível modificar o arquivo de configurações gerado para personalizar o DOSBox.

O arquivo é dividido em diversas seções (elas estão destacadas por [] ). 
Algumas seções possuem opções que podem ser configuradas.
Linhas de comentário são indicadas por # e %. 
O arquivo de configurações gerado contém as configurações atuais. Você pode alterá-las
e então iniciar o DOSBox com o parâmetro -conf para carregar o arquivo e consequentemente
as novas configurações.

O DOSBox buscará o arquivo de configuração especificado pelo parâmetro -conf.
Se nenhum arquivo for especificado com o parâmetro, o DOSBox irá procurar pelo
arquivo "dosbox.conf" no diretório atual.
Se não existir nenhum, o DOSBox carregará o arquivo de configurações do usuário.
Esse arquivo será criado se ele não existir.
O arquivo pode ser encontrado em ~/.dosbox (no Linux) ou "~/Library/Preferences" (no MAC OS X).
No Windows os atalhos do menu iniciar deverão ser usados para encontrá-lo.



========================
12. O Arquivo de Idioma:
========================

Um arquivo de idioma pode ser gerado pelo CONFIG.COM (CONFIG -writelang nomedoarquivo).
Leia-o, e esperamos que você entenda como mudá-lo. 
Inicie o DOSBox com o parâmetro -lang para usar seu novo arquivo de idioma.
Alternativamente, você pode colocar o nome do arquivo na seção [dosbox] do arquivo de configurações.
Lá existe uma entrada language= onde deverá ser adicionado o nome do arquivo.



=========================================
13. Criando sua própria versão do DOSBox:
=========================================

Faça o download da fonte.
Verifique o INSTALADOR na distribuição da fonte.



=============================
14. Agradecimentos Especiais:
=============================

Ver o arquivo THANKS.


============
15. Contato:
============

Acesse o site:
http://www.dosbox.com
para ver o email (Na página "Crew").

