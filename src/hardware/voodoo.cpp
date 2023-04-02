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
#include <string_view>

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


class Voodoo;
static std::unique_ptr<Voodoo> voodoo_dev;

constexpr uint32_t voodoo_lfb_mask = 0xffff0000;
static uint32_t voodoo_current_lfb = (VOODOO_INITIAL_LFB & voodoo_lfb_mask);

static_assert((VOODOO_INITIAL_LFB & 0xffff0000) == VOODOO_INITIAL_LFB,
              "VOODOO_INITIAL_LFB must have its lower 16 bits set to zero");

enum class EmulationType {
    None,
    Software,
    OpenGL
};

enum class CardType : int {
    Type1 = 1
};

constexpr std::string_view NoneStr = "false";
constexpr std::string_view SoftwareStr = "software";
constexpr std::string_view OpenGLStr = "opengl";
constexpr std::string_view AutoStr = "auto";

class Voodoo {
private:
    Section* m_configuration;
    EmulationType emulation_type;

public:
    Voodoo(const Voodoo&) = delete;
    Voodoo& operator=(const Voodoo&) = delete;
	
    Voodoo(Section* configuration) : m_configuration(configuration), emulation_type(EmulationType::None) {
        UpdateConfiguration(configuration);
    }
	
	EmulationType GetEmulationTypeFromString(std::string_view voodoo_type_str) {
		if (voodoo_type_str == NoneStr) {
			return EmulationType::None;
		} else if (voodoo_type_str == SoftwareStr) {
			return EmulationType::Software;
		} else if (voodoo_type_str == OpenGLStr) {
			return C_OPENGL ? EmulationType::OpenGL : EmulationType::None;
		} else if (voodoo_type_str == AutoStr) {
			return C_OPENGL ? EmulationType::OpenGL : EmulationType::Software;
		} else {
			return !OpenGL_using() ? EmulationType::Software : EmulationType::None;
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

    ~Voodoo() {
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

void VOODOO_Destroy([[maybe_unused]] Section* sec) {
    if (voodoo_dev) {
        voodoo_dev.reset();
    }
}

void VOODOO_Init(Section* sec) {
    if (!voodoo_dev) {
        voodoo_current_lfb = VOODOO_INITIAL_LFB & voodoo_lfb_mask;
        voodoo_dev = std::make_unique<Voodoo>(sec);
        sec->AddDestroyFunction(&VOODOO_Destroy, true);
    } else {
        voodoo_dev->UpdateConfiguration(sec);
    }
}