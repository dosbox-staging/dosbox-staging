/*
 *  Copyright (C) 2002-2015  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include "adlib.h"
#include "vgmcapture.h"
#include "setup.h"
#include "mapper.h"
#include "mem.h"
#include "dbopl.h"

namespace OPL2 {
	#include "opl.cpp"

	struct Handler : public Adlib::Handler {
		virtual void WriteReg( Bit32u reg, Bit8u val ) {
			adlib_write(reg,val);
		}
		virtual Bit32u WriteAddr( Bit32u port, Bit8u val ) {
			return val;
		}

		virtual void Generate( MixerChannel* chan, Bitu samples ) {
			Bit16s buf[1024];
			while( samples > 0 ) {
				Bitu todo = samples > 1024 ? 1024 : samples;
				samples -= todo;
				adlib_getsample(buf, todo);
				chan->AddSamples_m16( todo, buf );
			}
		}
		virtual void Init( Bitu rate ) {
			adlib_init(rate);
		}
		~Handler() {
		}
	};
}

namespace OPL3 {
	#define OPLTYPE_IS_OPL3
	#include "opl.cpp"

	struct Handler : public Adlib::Handler {
		virtual void WriteReg( Bit32u reg, Bit8u val ) {
			adlib_write(reg,val);
		}
		virtual Bit32u WriteAddr( Bit32u port, Bit8u val ) {
			adlib_write_index(port, val);
			return opl_index;
		}
		virtual void Generate( MixerChannel* chan, Bitu samples ) {
			Bit16s buf[1024*2];
			while( samples > 0 ) {
				Bitu todo = samples > 1024 ? 1024 : samples;
				samples -= todo;
				adlib_getsample(buf, todo);
				chan->AddSamples_s16( todo, buf );
			}
		}
		virtual void Init( Bitu rate ) {
			adlib_init(rate);
		}
		~Handler() {
		}
	};
}

#define RAW_SIZE 1024


/*
	Main Adlib implementation

*/

namespace Adlib {

/*
Chip
*/

bool Chip::Write( Bit32u reg, Bit8u val ) {
	switch ( reg ) {
	case 0x02:
		timer[0].counter = val;
		return true;
	case 0x03:
		timer[1].counter = val;
		return true;
	case 0x04:
		double time;
		time = PIC_FullIndex();
		if ( val & 0x80 ) {
			timer[0].Reset( time );
			timer[1].Reset( time );
		} else {
			timer[0].Update( time );
			timer[1].Update( time );
			if ( val & 0x1 ) {
				timer[0].Start( time, 80 );
			} else {
				timer[0].Stop( );
			}
			timer[0].masked = (val & 0x40) > 0;
			if ( timer[0].masked )
				timer[0].overflow = false;
			if ( val & 0x2 ) {
				timer[1].Start( time, 320 );
			} else {
				timer[1].Stop( );
			}
			timer[1].masked = (val & 0x20) > 0;
			if ( timer[1].masked )
				timer[1].overflow = false;

		}
		return true;
	}
	return false;
}


Bit8u Chip::Read( ) {
	double time( PIC_FullIndex() );
	timer[0].Update( time );
	timer[1].Update( time );
	Bit8u ret = 0;
	//Overflow won't be set if a channel is masked
	if ( timer[0].overflow ) {
		ret |= 0x40;
		ret |= 0x80;
	}
	if ( timer[1].overflow ) {
		ret |= 0x20;
		ret |= 0x80;
	}
	return ret;

}

void Module::CacheWrite( Bit32u reg, Bit8u val ) {
	//capturing?
	if (vgmCapture.get()) {
		vgmCapture->oplmode = oplmode;
		vgmCapture->ioWrite_OPL(reg>>8, reg&0xFF, val, cache);
	}
	//Store it into the cache
	cache[ reg ] = val;
}

void Module::DualWrite( Bit8u index, Bit8u reg, Bit8u val ) {
	//Make sure you don't use opl3 features
	//Don't allow write to disable opl3		
	if ( reg == 5 ) {
		return;
	}
	//Only allow 4 waveforms
	if ( reg >= 0xE0 ) {
		val &= 3;
	} 
	//Write to the timer?
	if ( chip[index].Write( reg, val ) ) 
		return;
	//Enabling panning
	if ( reg >= 0xc0 && reg <=0xc8 ) {
		val &= 0x0f;
		val |= index ? 0xA0 : 0x50;
	}
	Bit32u fullReg = reg + (index ? 0x100 : 0);
	handler->WriteReg( fullReg, val );
	CacheWrite( fullReg, val );
}

void Module::CtrlWrite( Bit8u val ) {
	switch ( ctrl.index ) {
	case 0x09: /* Left FM Volume */
		ctrl.lvol = val;
		goto setvol;
	case 0x0a: /* Right FM Volume */
		ctrl.rvol = val;
setvol:
		if ( ctrl.mixer ) {
			//Dune cdrom uses 32 volume steps in an apparent mistake, should be 128
			mixerChan->SetVolume( (float)(ctrl.lvol&0x1f)/31.0f, (float)(ctrl.rvol&0x1f)/31.0f );
		}
		break;
	}
}

Bitu Module::CtrlRead( void ) {
	switch ( ctrl.index ) {
	case 0x00: /* Board Options */
		return 0x70; //No options installed
	case 0x09: /* Left FM Volume */
		return ctrl.lvol;
	case 0x0a: /* Right FM Volume */
		return ctrl.rvol;
	case 0x15: /* Audio Relocation */
		return 0x388 >> 3; //Cryo installer detection
	}
	return 0xff;
}


void Module::PortWrite( Bitu port, Bitu val, Bitu iolen ) {
	//Keep track of last write time
	lastUsed = PIC_Ticks;
	//Maybe only enable with a keyon?
	if ( !mixerChan->enabled ) {
		mixerChan->Enable(true);
	}
	if ( port&1 ) {
		switch ( mode ) {
		case MODE_OPL3GOLD:
			if ( port == 0x38b ) {
				if ( ctrl.active ) {
					CtrlWrite( val );
					break;
				}
			}
			//Fall-through if not handled by control chip
		case MODE_OPL2:
		case MODE_OPL3:
			if ( !chip[0].Write( reg.normal, val ) ) {
				handler->WriteReg( reg.normal, val );
				CacheWrite( reg.normal, val );
			}
			break;
		case MODE_DUALOPL2:
			//Not a 0x??8 port, then write to a specific port
			if ( !(port & 0x8) ) {
				Bit8u index = ( port & 2 ) >> 1;
				DualWrite( index, reg.dual[index], val );
			} else {
				//Write to both ports
				DualWrite( 0, reg.dual[0], val );
				DualWrite( 1, reg.dual[1], val );
			}
			break;
		}
	} else {
		//Ask the handler to write the address
		//Make sure to clip them in the right range
		switch ( mode ) {
		case MODE_OPL2:
			reg.normal = handler->WriteAddr( port, val ) & 0xff;
			break;
		case MODE_OPL3GOLD:
			if ( port == 0x38a ) {
				if ( val == 0xff ) {
					ctrl.active = true;
					break;
				} else if ( val == 0xfe ) {
					ctrl.active = false;
					break;
				} else if ( ctrl.active ) {
					ctrl.index = val & 0xff;
					break;
				}
			}
			//Fall-through if not handled by control chip
		case MODE_OPL3:
			reg.normal = handler->WriteAddr( port, val ) & 0x1ff;
			break;
		case MODE_DUALOPL2:
			//Not a 0x?88 port, when write to a specific side
			if ( !(port & 0x8) ) {
				Bit8u index = ( port & 2 ) >> 1;
				reg.dual[index] = val & 0xff;
			} else {
				reg.dual[0] = val & 0xff;
				reg.dual[1] = val & 0xff;
			}
			break;
		}
	}
}


Bitu Module::PortRead( Bitu port, Bitu iolen ) {
	switch ( mode ) {
	case MODE_OPL2:
		//We allocated 4 ports, so just return -1 for the higher ones
		if ( !(port & 3 ) ) {
			//Make sure the low bits are 6 on opl2
			return chip[0].Read() | 0x6;
		} else {
			return 0xff;
		}
	case MODE_OPL3GOLD:
		if ( ctrl.active ) {
			if ( port == 0x38a ) {
				return 0; //Control status, not busy
			} else if ( port == 0x38b ) {
				return CtrlRead();
			}
		}
		//Fall-through if not handled by control chip
	case MODE_OPL3:
		//We allocated 4 ports, so just return -1 for the higher ones
		if ( !(port & 3 ) ) {
			return chip[0].Read();
		} else {
			return 0xff;
		}
	case MODE_DUALOPL2:
		//Only return for the lower ports
		if ( port & 1 ) {
			return 0xff;
		}
		//Make sure the low bits are 6 on opl2
		return chip[ (port >> 1) & 1].Read() | 0x6;
	}
	return 0;
}


void Module::Init( Mode m ) {
	mode = m;
	switch ( mode ) {
	case MODE_OPL3:
	case MODE_OPL3GOLD:
	case MODE_OPL2:
		break;
	case MODE_DUALOPL2:
		//Setup opl3 mode in the hander
		handler->WriteReg( 0x105, 1 );
		//Also set it up in the cache so the capturing will start opl3
		CacheWrite( 0x105, 1 );
		break;
	}
}

}; //namespace



static Adlib::Module* module = 0;

static void OPL_CallBack(Bitu len) {
	module->handler->Generate( module->mixerChan, len );
	//Disable the sound generation after 30 seconds of silence
	if ((PIC_Ticks - module->lastUsed) > 30000) {
		Bitu i;
		for (i=0xb0;i<0xb9;i++) if (module->cache[i]&0x20||module->cache[i+0x100]&0x20) break;
		if (i==0xb9) module->mixerChan->Enable(false);
		else module->lastUsed = PIC_Ticks;
	}
}

static Bitu OPL_Read(Bitu port,Bitu iolen) {
	return module->PortRead( port, iolen );
}

void OPL_Write(Bitu port,Bitu val,Bitu iolen) {
	module->PortWrite( port, val, iolen );
}

namespace Adlib {

Module::Module( Section* configuration ) : Module_base(configuration) {
	reg.dual[0] = 0;
	reg.dual[1] = 0;
	reg.normal = 0;
	ctrl.active = false;
	ctrl.index = 0;
	ctrl.lvol = 0xff;
	ctrl.rvol = 0xff;
	handler = 0;

	Section_prop * section=static_cast<Section_prop *>(configuration);
	Bitu base = section->Get_hex("sbbase");
	Bitu rate = section->Get_int("oplrate");
	//Make sure we can't select lower than 8000 to prevent fixed point issues
	if ( rate < 8000 )
		rate = 8000;
	std::string oplemu( section->Get_string( "oplemu" ) );
	ctrl.mixer = section->Get_bool("sbmixer");

	mixerChan = mixerObject.Install(OPL_CallBack,rate,"FM");
	mixerChan->SetScale( 2.0 );
	if (oplemu == "fast") {
		handler = new DBOPL::Handler();
	} else if (oplemu == "compat") {
		if ( oplmode == OPL_opl2 ) {
			handler = new OPL2::Handler();
		} else {
			handler = new OPL3::Handler();
		}
	} else {
		handler = new DBOPL::Handler();
	}
	handler->Init( rate );
	bool single = false;
	switch ( oplmode ) {
	case OPL_opl2:
		single = true;
		Init( Adlib::MODE_OPL2 );
		break;
	case OPL_dualopl2:
		Init( Adlib::MODE_DUALOPL2 );
		break;
	case OPL_opl3:
		Init( Adlib::MODE_OPL3 );
		break;
	case OPL_opl3gold:
		Init( Adlib::MODE_OPL3GOLD );
		break;
	}
	//0x388 range
	WriteHandler[0].Install(0x388,OPL_Write,IO_MB, 4 );
	ReadHandler[0].Install(0x388,OPL_Read,IO_MB, 4 );
	//0x220 range
	if ( !single ) {
		WriteHandler[1].Install(base,OPL_Write,IO_MB, 4 );
		ReadHandler[1].Install(base,OPL_Read,IO_MB, 4 );
	}
	//0x228 range
	WriteHandler[2].Install(base+8,OPL_Write,IO_MB, 2);
	ReadHandler[2].Install(base+8,OPL_Read,IO_MB, 1);

}

Module::~Module() {
	if ( handler ) {
		delete handler;
	}
}

//Initialize static members
OPL_Mode Module::oplmode=OPL_none;

};	//Adlib Namespace


void OPL_Init(Section* sec,OPL_Mode oplmode) {
	Adlib::Module::oplmode = oplmode;
	module = new Adlib::Module( sec );
}

void OPL_ShutDown(Section* sec){
	delete module;
	module = 0;

}
