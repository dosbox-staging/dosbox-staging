/*
 *  Copyright (C) 2002-2011  The DOSBox Team
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
#include <cstdint>
#include <memory>
#include <string>

#include "dosbox.h"
#include "config.h"
#include "setup.h"
#include "cross.h"

#include "paging.h"
#include "video.h"
#include "mem.h"

#include "voodoo.h"
#include "pci_bus.h"
#include "voodoo_interface.h"


class VOODOO;
static std::unique_ptr<VOODOO> voodoo_dev;

constexpr uint32_t voodoo_lfb_mask = 0xffff0000;
static uint32_t voodoo_current_lfb = (VOODOO_INITIAL_LFB & voodoo_lfb_mask);

enum class EmulationType {
    None,
    Software,
    OpenGL
};

enum class CardType : int {
    Type1 = 1
};


class VOODOO : public Module_base {
private:
    EmulationType emulation_type;

public:
    VOODOO(Section* configuration) : Module_base(configuration), emulation_type(EmulationType::None) {
        UpdateConfiguration(configuration);
    }
	
    EmulationType GetEmulationTypeFromString(const std::string& voodoo_type_str) {
        if (voodoo_type_str == "false") {
            return EmulationType::None;
        } else if ((voodoo_type_str == "software") || !OpenGL_using()) {
            return EmulationType::Software;
    #if C_OPENGL
        } else if ((voodoo_type_str == "opengl") || (voodoo_type_str == "auto")) {
            return EmulationType::OpenGL;
    #else
        } else if (voodoo_type_str == "auto") {
            return EmulationType::Software;
    #endif
        } else {
            return EmulationType::None;
        }
    }
	
	void UpdateConfiguration(Section* configuration) {
		Section_prop* section = static_cast<Section_prop*>(configuration);
		std::string voodoo_type_str(section->Get_string("voodoo_card"));
		emulation_type = GetEmulationTypeFromString(voodoo_type_str);

		CardType card_type = CardType::Type1;
		bool max_voodoomem;
		std::string voodoo_mem_str(section->Get_string("voodoo_mem"));
		max_voodoomem = (voodoo_mem_str == "max");

		bool needs_pci_device = false;

		switch (emulation_type) {
			case EmulationType::Software:
			case EmulationType::OpenGL:
				Voodoo_Initialize(static_cast<Bits>(emulation_type), static_cast<int>(card_type), max_voodoomem);
				needs_pci_device = true;
				break;
			default:
				break;
		}

		if (needs_pci_device) PCI_AddSST_Device(static_cast<int>(card_type));
	}

    ~VOODOO() {
        PCI_RemoveSST_Device();

        if (emulation_type == EmulationType::None) return;

        switch (emulation_type) {
            case EmulationType::Software:
            case EmulationType::OpenGL:
                Voodoo_Shut_Down();
                break;
            default:
                break;
        }
    }

    void PCI_InitEnable(Bitu val) {
        switch (emulation_type) {
            case EmulationType::Software:
            case EmulationType::OpenGL:
                Voodoo_PCI_InitEnable(val);
                break;
            default:
                break;
        }
    }

    void PCI_Enable(bool enable) {
        switch (emulation_type) {
            case EmulationType::Software:
            case EmulationType::OpenGL:
                Voodoo_PCI_Enable(enable);
                break;
            default:
                break;
        }
    }

    PageHandler* GetPageHandler() {
        switch (emulation_type) {
            case EmulationType::Software:
            case EmulationType::OpenGL:
                return Voodoo_GetPageHandler();
            default:
                break;
        }
        return nullptr;
    }
};

void VOODOO_PCI_InitEnable(Bitu val) {
    if (voodoo_dev != nullptr) {
        voodoo_dev->PCI_InitEnable(val);
    }
}

void VOODOO_PCI_Enable(bool enable) {
    if (voodoo_dev != nullptr) {
        voodoo_dev->PCI_Enable(enable);
    }
}

void VOODOO_PCI_SetLFB(uint32_t lfbaddr) {
    voodoo_current_lfb = (lfbaddr & 0xffff0000);
}

bool VOODOO_PCI_CheckLFBPage(Bitu page) {
    return (page >= (voodoo_current_lfb >> 12)) && (page < ((voodoo_current_lfb >> 12) + VOODOO_PAGES));
}


PageHandler* VOODOO_GetPageHandler() {
    if (voodoo_dev != nullptr) {
        return voodoo_dev->GetPageHandler();
    }
    return nullptr;
}

void VOODOO_Destroy(Section* sec) {
    if (voodoo_dev) {
        voodoo_dev.reset();
    }
}

void VOODOO_Init(Section* sec) {
    if (!voodoo_dev) {
        voodoo_current_lfb = (VOODOO_INITIAL_LFB & 0xffff0000);
        voodoo_dev = std::make_unique<VOODOO>(sec);
        sec->AddDestroyFunction(&VOODOO_Destroy, true);
    } else {
        voodoo_dev->UpdateConfiguration(sec);
    }
}