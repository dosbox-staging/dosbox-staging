add_library(libhardware STATIC
		input/intel8042.cpp
		input/intel8255.cpp
		input/keyboard.cpp
		input/keyboard_scancodes.cpp
		input/mouse.cpp
		input/mouse_common.cpp
		input/mouse_config.cpp
		input/mouse_interfaces.cpp
		input/mouse_manymouse.cpp
		input/mouseif_dos_driver.cpp
		input/mouseif_ps2_bios.cpp
		input/mouseif_virtual_machines.cpp
		mame/saa1099.cpp
		mame/sn76496.cpp
		serialport/directserial.cpp
		serialport/libserial.cpp
		serialport/misc_util.cpp
		serialport/nullmodem.cpp
		serialport/serialdummy.cpp
		serialport/serialmouse.cpp
		serialport/serialport.cpp
		serialport/softmodem.cpp
		adlib_gold.cpp
		cmos.cpp
		covox.cpp
		disney.cpp
		dma.cpp
		gameblaster.cpp
		gus.cpp
		ide.cpp
		innovation.cpp
		imfc.cpp
		iohandler.cpp
		iohandler_containers.cpp
		ipx.cpp
		ipxserver.cpp
		joystick.cpp
		lpt_dac.cpp
		memory.cpp
		mixer.cpp
		mpu401.cpp
		ne2000.cpp
		opl.cpp
		opl_capture.cpp
		pci_bus.cpp
		pcspeaker.cpp
		pcspeaker_discrete.cpp
		pcspeaker_impulse.cpp
		pic.cpp
		ps1audio.cpp
		reelmagic/driver.cpp
		reelmagic/player.cpp
		reelmagic/video_mixer.cpp
		sblaster.cpp
		ston1_dac.cpp
		tandy_sound.cpp
		timer.cpp
		vga.cpp
		vga_attr.cpp
		vga_crtc.cpp
		vga_dac.cpp
		vga_draw.cpp
		vga_gfx.cpp
		vga_memory.cpp
		vga_misc.cpp
		vga_other.cpp
		vga_paradise.cpp
		vga_s3.cpp
		vga_seq.cpp
		vga_tseng.cpp
		vga_xga.cpp
		virtualbox.cpp
		vmware.cpp
		voodoo.cpp
)

pkg_check_modules(SPEEXDSP REQUIRED IMPORTED_TARGET speexdsp)

find_package(iir REQUIRED)
target_link_libraries(libhardware PRIVATE $<IF:$<TARGET_EXISTS:iir::iir>,iir::iir,iir::iir_static>)

find_package(SDL2_net REQUIRED)
target_link_libraries(libhardware PRIVATE $<IF:$<TARGET_EXISTS:SDL2_net::SDL2_net>,SDL2_net::SDL2_net,SDL2_net::SDL2_net-static>)

target_link_libraries(libhardware PRIVATE
		libaudio
		libesfmu
		libresidfp
		libym7128bemu
		libtalchorus
		libnuked
		libints
		libgui
		libmidi
		libshell
		PkgConfig::SPEEXDSP)
