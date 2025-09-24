// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ethernet.h"

#include <cstring>
#include <memory>

#include "ethernet_slirp.h"

#include "config/config.h"
#include "hardware/network/ne2000.h"
#include "hardware/virtualbox.h"
#include "hardware/vmware.h"

EthernetConnection* ETHERNET_OpenConnection([[maybe_unused]] const std::string& backend)
{
	EthernetConnection* conn = nullptr;

	// Currently only slirp is supported
	if (backend == "slirp") {
		conn = new SlirpEthernetConnection;
		assert(control);

		const auto settings = control->GetSection("ethernet");
		if (!conn->Initialize(settings)) {
			delete conn;
			conn = nullptr;
		}
	}

	return conn;
}

static void init_ethernet_dosbox_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto pbool = section.AddBool("ne2000", WhenIdle, false);
	pbool->SetOptionHelp(
	        "SLIRP",
	        "Enable emulation of a Novell NE2000 network card on a software-based network\n"
	        "with the following properties ('off' by default):\n"
	        "  - 255.255.255.0   Subnet mask of the 10.0.2.0 virtual LAN.\n"
	        "  - 10.0.2.2        IP of the gateway and DHCP service.\n"
	        "  - 10.0.2.3        IP of the virtual DNS server.\n"
	        "  - 10.0.2.15       First IP provided by DHCP (this is your IP)\n"
	        "Note: Using this feature requires an NE2000 packet driver, a DHCP client, and a\n"
	        "      TCP/IP stack set up in DOS. You might need port-forwarding from your host\n"
	        "      OS into DOSBox, and from your router to your host OS when acting as the\n"
	        "      server in multiplayer games.");

	pbool->SetEnabledOptions({"SLIRP"});

	auto phex = section.AddHex("nicbase", WhenIdle, 0x300);
	phex->SetValues(
	        {"200", "220", "240", "260", "280", "2c0", "300", "320", "340", "360"});
	phex->SetOptionHelp("SLIRP",
	                    "Base address of the NE2000 card (300 by default).\n"
	                    "Note: Addresses 220 and 240 might not be available as they're assigned to the\n"
	                    "      Sound Blaster and Gravis UltraSound by default.");

	phex->SetEnabledOptions({"SLIRP"});

	auto pint = section.AddInt("nicirq", WhenIdle, 3);
	pint->SetValues({"3", "4", "5", "9", "10", "11", "12", "14", "15"});
	pint->SetOptionHelp("SLIRP",
	                    "The interrupt used by the NE2000 card (3 by default).\n"
	                    "Note: IRQs 3 and 5 might not be available as they're assigned to\n"
	                    "      'serial2' and the Gravis UltraSound by default.");

	pint->SetEnabledOptions({"SLIRP"});

	auto pstring = section.AddString("macaddr", WhenIdle, "AC:DE:48:88:99:AA");
	pstring->SetOptionHelp("SLIRP",
	                       "The MAC address of the NE2000 card ('AC:DE:48:88:99:AA' by default).");

	pstring->SetEnabledOptions({"SLIRP"});

	pstring = section.AddString("tcp_port_forwards", WhenIdle, "");
	pstring->SetOptionHelp(
	        "SLIRP",
	        "Forward one or more TCP ports from the host into the DOS guest\n"
	        "(unset by default).\n"
	        "The format is:\n"
	        "  port1  port2  port3 ... (e.g., 21 80 443)\n"
	        "  This will forward FTP, HTTP, and HTTPS into the DOS guest.\n"
	        "If the ports are privileged on the host, a mapping can be used\n"
	        "  host:guest  ..., (e.g., 8021:21 8080:80)\n"
	        "  This will forward ports 8021 and 8080 to FTP and HTTP in the guest.\n"
	        "A range of adjacent ports can be abbreviated with a dash:\n"
	        "  start-end ... (e.g., 27910-27960)\n"
	        "  This will forward ports 27910 to 27960 into the DOS guest.\n"
	        "Mappings and ranges can be combined, too:\n"
	        "  hstart-hend:gstart-gend ..., (e.g, 8040-8080:20-60)\n"
	        "  This forwards ports 8040 to 8080 into 20 to 60 in the guest.\n"
	        "Notes:\n"
	        "  - If mapped ranges differ, the shorter range is extended to fit.\n"
	        "  - If conflicting host ports are given, only the first one is setup.\n"
	        "  - If conflicting guest ports are given, the latter rule takes precedent.");

	pstring->SetEnabledOptions({"SLIRP"});

	pstring = section.AddString("udp_port_forwards", WhenIdle, "");
	pstring->SetOptionHelp(
	        "SLIRP",
	        "Forward one or more UDP ports from the host into the DOS guest\n"
	        "(unset by default). The format is the same as for TCP port forwards.");

	pstring->SetEnabledOptions({"SLIRP"});
}

void ETHERNET_Init()
{
	auto section = get_section("ethernet");
	assert(section);

	NE2K_Init(section);

	VIRTUALBOX_Init();
	VMWARE_Init();
}

void ETHERNET_Destroy()
{
	VMWARE_Destroy();
	VIRTUALBOX_Destroy();

	NE2K_Destroy();
}

static void notify_ethernet_setting_updated(SectionProp* section,
                                            [[maybe_unused]] const std::string& prop_name)
{
	NE2K_Destroy();
	NE2K_Init(section);
}

void ETHERNET_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("ethernet");
	section->AddUpdateHandler(notify_ethernet_setting_updated);

	init_ethernet_dosbox_settings(*section);
}
