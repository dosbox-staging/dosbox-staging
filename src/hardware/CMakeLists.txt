target_sources(libdosboxcommon PRIVATE 
  audio/adlib_gold.cpp
  audio/cms.cpp
  audio/covox.cpp
  audio/disney.cpp
  audio/gus.cpp
  audio/imfc.cpp
  audio/innovation.cpp
  audio/lpt_dac.cpp
  audio/mame/saa1099.cpp
  audio/mame/sn76496.cpp
  audio/mpu401.cpp
  audio/opl.cpp
  audio/pcspeaker.cpp
  audio/pcspeaker_discrete.cpp
  audio/pcspeaker_impulse.cpp
  audio/ps1audio.cpp
  audio/soundblaster.cpp
  audio/speaker.cpp
  audio/ston1_dac.cpp
  audio/tandy_sound.cpp

  input/intel8042.cpp
  input/intel8255.cpp
  input/joystick.cpp
  input/keyboard.cpp
  input/keyboard_scancodes.cpp
  input/mouse.cpp
  input/mouse_common.cpp
  input/mouse_config.cpp
  input/mouse_interfaces.cpp
  input/mouse_manymouse.cpp
  input/mouseif_dos_driver.cpp
  input/mouseif_dos_driver_state.cpp
  input/mouseif_ps2_bios.cpp
  input/mouseif_virtual_machines.cpp

  network/ipx.cpp
  network/ipxserver.cpp
  network/ne2000.cpp

  serialport/misc_util.cpp
  serialport/nullmodem.cpp
  serialport/serialdummy.cpp
  serialport/serialmouse.cpp
  serialport/serialport.cpp
  serialport/softmodem.cpp

  video/reelmagic/driver.cpp
  video/reelmagic/player.cpp
  video/reelmagic/video_mixer.cpp
  video/vga.cpp
  video/vga_attr.cpp
  video/vga_crtc.cpp
  video/vga_dac.cpp
  video/vga_draw.cpp
  video/vga_gfx.cpp
  video/vga_memory.cpp
  video/vga_misc.cpp
  video/vga_other.cpp
  video/vga_paradise.cpp
  video/vga_s3.cpp
  video/vga_seq.cpp
  video/vga_tseng.cpp
  video/vga_xga.cpp
  video/voodoo.cpp

  cmos.cpp
  dma.cpp
  ide.cpp
  memory.cpp
  pci_bus.cpp
  pic.cpp
  port.cpp
  timer.cpp
  virtualbox.cpp
  vmware.cpp
)

target_sources(dosbox PRIVATE port_containers.cpp)

target_link_libraries(libdosboxcommon PRIVATE
  libesfmu
  libresidfp
  libym7128bemu
  libtalchorus
  $<IF:$<BOOL:${C_MANYMOUSE}>,libmanymouse,>
  libnuked
  PkgConfig::SPEEXDSP
)

