/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#if C_MODEM

#include <algorithm>
#include <ctype.h>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "string_utils.h"
#include "serialport.h"
#include "softmodem.h"
#include "misc_util.h"

class PhonebookEntry {
public:
	PhonebookEntry(const std::string &_phone, const std::string &_address) :
		phone(_phone),
		address(_address) {
	}

	bool IsMatchingPhone(const std::string &input) const {
		return (input == phone);
	}

	const std::string &GetAddress() const {
		return address;
	}

private:
	std::string phone;
	std::string address;
};

static std::vector<PhonebookEntry> phones;
static const char phoneValidChars[] = "01234567890*=,;#+>";

static bool MODEM_IsPhoneValid(const std::string &input) {
	size_t found = input.find_first_not_of(phoneValidChars);
	if (found != std::string::npos) {
		LOG_MSG("SERIAL: Phonebook %s contains invalid character %c.",
		        input.c_str(), input[found]);
		return false;
	}

	return true;
}

bool MODEM_ReadPhonebook(const std::string &filename) {
	std::ifstream loadfile(filename);
	if (!loadfile)
		return false;

	LOG_MSG("SERIAL: Phonebook loading from %s.", filename.c_str());

	std::string linein;
	while (std::getline(loadfile, linein)) {
		std::istringstream iss(linein);
		std::string phone, address;

		if (!(iss >> phone >> address)) {
			LOG_MSG("SERIAL: Phonebook skipped a bad line in %s.",
			        filename.c_str());
			continue;
		}

		// Check phone number for characters ignored by Hayes modems.
		if (!MODEM_IsPhoneValid(phone))
			continue;

		LOG_MSG("SERIAL: Phonebook mapped %s to address %s.", phone.c_str(),
		        address.c_str());
		phones.emplace_back(phone, address);
	}

	return true;
}

void MODEM_ClearPhonebook()
{
	phones.clear();
}

static const char *MODEM_GetAddressFromPhone(const char *input) {
	for (const auto &entry : phones) {
		if (entry.IsMatchingPhone(input))
			return entry.GetAddress().c_str();
	}

	return nullptr;
}

CSerialModem::CSerialModem(const uint8_t port_idx, CommandLine *cmd)
        : CSerial(port_idx, cmd),
          rqueue(MODEM_BUFFER_QUEUE_SIZE),
          tqueue(MODEM_BUFFER_QUEUE_SIZE),
          telClient({}),
          dial({})
{
	InstallationSuccessful=false;

	// Setup the listening port
	uint32_t val;
	if (getUintFromString("listenport:", val, cmd))
		listenport = val;
	// Otherwise the default listenport will be used

	// TODO: Fix dialtones if requested
	//mhd.chan=MIXER_AddChannel((MIXER_MixHandler)this->MODEM_CallBack,8000,"MODEM");
	//MIXER_Enable(mhd.chan,false);
	//MIXER_SetMode(mhd.chan,MIXER_16MONO);

	CSerial::Init_Registers();
	Reset(); // reset calls EnterIdleState
	setEvent(SERIAL_POLLING_EVENT,1);

	// Enable telnet-mode if configured
	if (getUintFromString("telnet:", val, cmd)) {
		telnet_mode = (val == 1);
		LOG_MSG("SERIAL: Port %" PRIu8 " telnet-mode %s",
		        GetPortNumber(), telnet_mode ? "enabled" : "disabled");
	}

	InstallationSuccessful=true;
}

CSerialModem::~CSerialModem() {
	// remove events
	for (uint32_t i = SERIAL_BASE_EVENT_COUNT + 1;
	     i <= SERIAL_MODEM_EVENT_COUNT; i++)
		removeEvent(i);
}

void CSerialModem::handleUpperEvent(uint16_t type)
{
	switch (type) {
	case SERIAL_RX_EVENT: {
		// check for bytes to be sent to port
		if (CSerial::CanReceiveByte())
			if (rqueue.isUsed() &&
			    (CSerial::getRTS() || (flowcontrol != 3))) {
				uint8_t rbyte = rqueue.pop();
				// LOG_MSG("SERIAL: Port %" PRIu8 " modem sending byte %2x"
				//         " back to UART3", GetPortNumber(), rbyte);
				CSerial::receiveByte(rbyte);
			}
		if(CSerial::CanReceiveByte()) setEvent(SERIAL_RX_EVENT, bytetime*0.98f);
		break;
	}
	case MODEM_TX_EVENT: {
		if (!tqueue.isFull()) {
			tqueue.push(waiting_tx_character);
			if (tqueue.numFreeSlots() < 2) {
				CSerial::setCTS(false);
			}
		} else {
			static uint16_t lcount = 0;
			if (lcount < 1000) {
				lcount++;
				LOG_MSG("SERIAL: Port %" PRIu8 " modem TX buffer overflow!",
				        GetPortNumber());
			}
		}
		ByteTransmitted();
		break;
	}
	case SERIAL_POLLING_EVENT: {
		if (rqueue.isUsed()) {
			removeEvent(SERIAL_RX_EVENT);
			setEvent(SERIAL_RX_EVENT, (float)0.01);
		}
		Timer2();
		setEvent(SERIAL_POLLING_EVENT,1);
		break;
	}

	case MODEM_RING_EVENT: {
		break;
	}
	}
}

void CSerialModem::SendLine(const char *line) {
	rqueue.push(0xd);
	rqueue.push(0xa);
	const auto unsigned_addr = reinterpret_cast<const uint8_t *>(line);
	rqueue.pushMany(unsigned_addr, strlen(line));
	rqueue.push(0xd);
	rqueue.push(0xa);
}

// only for numbers < 1000...
void CSerialModem::SendNumber(uint32_t val)
{
	assert(val < 1000);
	rqueue.push(0xd);
	rqueue.push(0xa);
	rqueue.push(static_cast<uint8_t>(val / 100 + '0'));
	val = val % 100;
	rqueue.push(static_cast<uint8_t>(val / 10 + '0'));
	val = val%10;
	rqueue.push(static_cast<uint8_t>(val + '0'));
	rqueue.push(0xd);
	rqueue.push(0xa);
}

void CSerialModem::SendRes(const ResTypes response) {
	char const * string = nullptr;
	uint32_t code = -1;
	switch (response) {
		case ResOK:         code = 0; string = "OK"; break;
		case ResCONNECT:    code = 1; string = "CONNECT 57600"; break;
		case ResRING:       code = 2; string = "RING"; break;
		case ResNOCARRIER:  code = 3; string = "NO CARRIER"; break;
		case ResERROR:      code = 4; string = "ERROR"; break;
		case ResNODIALTONE: code = 6; string = "NO DIALTONE"; break;
		case ResBUSY:       code = 7; string = "BUSY"; break;
		case ResNOANSWER:   code = 8; string = "NO ANSWER"; break;
		case ResNONE:       return;
	}

	if(doresponse != 1) {
		if (doresponse == 2 && (response == ResRING ||
			response == ResCONNECT || response == ResNOCARRIER))
			return;
		if (numericresponse && code != static_cast<uint32_t>(-1))
			SendNumber(code);
		else if (string != nullptr)
			SendLine(string);

		// if(CSerial::CanReceiveByte())	// very fast response
		//	if(rqueue->inuse() && CSerial::getRTS())
		//	{ uint8_t rbyte =rqueue->getb();
		//		CSerial::receiveByte(rbyte);
		//	LOG_MSG("SERIAL: Port %" PRIu8 " modem sending byte %2x back to UART2",
		//	        GetPortNumber(), rbyte);
		//	}

		if (string != nullptr) {
			LOG_MSG("SERIAL: Port %" PRIu8 " modem response: %s.",
			        GetPortNumber(), string);
		}
	}
}

bool CSerialModem::Dial(const char * host) {
	char buf[128] = "";
	safe_strcpy(buf, host);

	const char *destination = buf;

	// Scan host for port
	uint16_t port;
	char * hasport = strrchr(buf,':');
	if (hasport) {
		*hasport++ = 0;
		port = (uint16_t)atoi(hasport);
	}
	else {
		port = MODEM_DEFAULT_PORT;
	}

	// Resolve host we're gonna dial
	LOG_MSG("SERIAL: Port %" PRIu8 " connecting to host %s port %" PRIu16
	        ".", GetPortNumber(), destination, port);
	clientsocket.reset(new TCPClientSocket(destination, port));
	if (!clientsocket->isopen) {
		clientsocket.reset(nullptr);
		LOG_MSG("SERIAL: Port %" PRIu8 " failed to connect.", GetPortNumber());
		SendRes(ResNOCARRIER);
		EnterIdleState();
		return false;
	} else {
		EnterConnectedState();
		return true;
	}
}

void CSerialModem::AcceptIncomingCall() {
	if (waitingclientsocket) {
		clientsocket = std::move(waitingclientsocket);
		EnterConnectedState();
	} else {
		EnterIdleState();
	}
}

uint32_t CSerialModem::ScanNumber(char *&scan) const
{
	uint32_t ret = 0;
	while (char c = *scan) {
		if (c >= '0' && c <= '9') {
			ret*=10;
			ret+=c-'0';
			scan++;
		} else
			break;
	}
	return ret;
}

char CSerialModem::GetChar(char * & scan) const {
	char ch = *scan;
	scan++;
	return ch;
}

void CSerialModem::Reset(){
	EnterIdleState();
	cmdpos = 0;
	cmdbuf[0] = 0;
	flowcontrol = 0;
	plusinc = 0;
	oldDTRstate = getDTR();
	dtrmode = 2;
	clientsocket.reset(nullptr);

	memset(&reg,0,sizeof(reg));
	reg[MREG_AUTOANSWER_COUNT] = 0;  // no autoanswer
	reg[MREG_RING_COUNT]       = 1;
	reg[MREG_ESCAPE_CHAR]      = '+';
	reg[MREG_CR_CHAR]          = '\r';
	reg[MREG_LF_CHAR]          = '\n';
	reg[MREG_BACKSPACE_CHAR]   = '\b';
	reg[MREG_GUARD_TIME]       = 50;
	reg[MREG_DTR_DELAY]        = 5;

	cmdpause = 0;
	echo = true;
	doresponse = 0;
	numericresponse = false;
}

void CSerialModem::EnterIdleState(){
	connected = false;
	ringing = false;
	dtrofftimer = -1;
	clientsocket.reset(nullptr);
	waitingclientsocket.reset(nullptr);

	// get rid of everything
	if (serversocket) {
		waitingclientsocket.reset(serversocket->Accept());
		while (waitingclientsocket) {
			waitingclientsocket.reset(serversocket->Accept());
		}
	} else if (listenport) {

		serversocket.reset(new TCPServerSocket(listenport));
		if (!serversocket->isopen) {
			LOG_MSG("SERIAL: Port %" PRIu8 " modem could not open TCP port "
			        "%" PRIu16 ".",
			        GetPortNumber(), listenport);

			serversocket.reset(nullptr);
		} else
			LOG_MSG("SERIAL: Port %" PRIu8 " modem listening on TCP port "
			        "%" PRIu16 " ...",
			        GetPortNumber(), listenport);
	}
	waitingclientsocket.reset(nullptr);

	commandmode = true;
	CSerial::setCD(false);
	CSerial::setRI(false);
	CSerial::setDSR(true);
	CSerial::setCTS(true);
	tqueue.clear();
}

void CSerialModem::EnterConnectedState() {
	// we don't accept further calls
	serversocket.reset(nullptr);
	SendRes(ResCONNECT);
	commandmode = false;
	telClient = {}; // reset values
	connected = true;
	ringing = false;
	dtrofftimer = -1;
	CSerial::setCD(true);
	CSerial::setRI(false);
}

template <size_t N>
bool is_next_token(const char (&a)[N], const char *b) noexcept
{
	// Is 'b' at least as long as 'a'?
	constexpr size_t N_without_null = N - 1;
	if (strnlen(b, N) < N_without_null)
		return false;
	return (strncmp(a, b, N_without_null) == 0);
}

void CSerialModem::DoCommand()
{
	cmdbuf[cmdpos] = 0;
	cmdpos = 0;			//Reset for next command
	upcase(cmdbuf);
	LOG_MSG("SERIAL: Port %" PRIu8 " command sent to modem: ->%s<-",
	        GetPortNumber(), cmdbuf);
	/* Check for empty line, stops dialing and autoanswer */
	if (!cmdbuf[0]) {
		reg[MREG_AUTOANSWER_COUNT] = 0;	// autoanswer off
		return;
	}
	//else {
		//MIXER_Enable(mhd.chan,false);
	//	dialing = false;
	//	SendRes(ResNOCARRIER);
	//	goto ret_none;
	//}
	/* AT command set interpretation */

	if ((cmdbuf[0] != 'A') || (cmdbuf[1] != 'T')) {
		SendRes(ResERROR);
		return;
	}
	char * scanbuf = &cmdbuf[2];
	while (1) {
		// LOG_MSG("SERIAL: Port %" PRIu8 " loopstart ->%s<-",
		//         GetPortNumber(), scanbuf);
		char chr = GetChar(scanbuf);
		switch (chr) {

		// Multi-character AT-commands are prefixed with +
		// -----------------------------------------------
		// Note: successfully finding your multi-char command
		// requires moving the scanbuf position one beyond the
		// the last character in the multi-char sequence to ensure
		// single-character detection resumes on the next character.
		// Either break if successful or fail with SendRes(ResERROR)
		// and return (halting the command sequence all together).
		case '+':
			// +NET1 enables telnet-mode and +NET0 disables it
			if (is_next_token("NET", scanbuf)) {
				// only walk the pointer ahead if the command matches
				scanbuf += 3;
				const uint32_t requested_mode = ScanNumber(scanbuf);

				// If the mode isn't valid then stop parsing
				if (requested_mode != 1 && requested_mode != 0) {
					SendRes(ResERROR);
					return;
				}
				// Inform the user on changes
				if (telnet_mode != static_cast<bool>(requested_mode)) {
					telnet_mode = requested_mode;
					LOG_MSG("SERIAL: Port %" PRIu8 " telnet-mode %s",
					        GetPortNumber(),
					        telnet_mode ? "enabled" : "disabled");
				}
				break;
			}
			// If the command wasn't recognized then stop parsing
			SendRes(ResERROR);
			return;

		case 'D': { // Dial
			char * foundstr = &scanbuf[0];
			if (*foundstr == 'T' || *foundstr == 'P')
				foundstr++;

			// Small protection against empty line or hostnames beyond the 253-char limit
			if ((!foundstr[0]) || (strlen(foundstr) > 253)) {
				SendRes(ResERROR);
				return;
			}
			// scan for and remove whitespaces; weird bug: with leading spaces in the string,
			// SDLNet_ResolveHost will return no error but not work anyway (win)
			foundstr = trim(foundstr);

			const char *mappedaddr = MODEM_GetAddressFromPhone(foundstr);
			if (mappedaddr) {
				Dial(mappedaddr);
				return;
			}

			//Large enough scope, so the buffers are still valid when reaching Dail.
			char buffer[128];
			char obuffer[128];
			if (strlen(foundstr) >= 12) {
				// Check if supplied parameter only consists of digits
				bool isNum = true;
				size_t fl = strlen(foundstr);
				for (size_t i = 0; i < fl; i++)
					if (foundstr[i] < '0' || foundstr[i] > '9')
						isNum = false;
				if (isNum) {
					// Parameter is a number with at least 12 digits => this cannot
					// be a valid IP/name
					// Transform by adding dots
					size_t j = 0;
					size_t foundlen = strlen(foundstr);
					for (size_t i = 0; i < foundlen; i++) {
						buffer[j++] = foundstr[i];
						// Add a dot after the third, sixth and ninth number
						if (i == 2 || i == 5 || i == 8)
							buffer[j++] = '.';
						// If the string is longer than 12 digits,
						// interpret the rest as port
						if (i == 11 && strlen(foundstr) > 12)
							buffer[j++] = ':';
					}
					buffer[j] = 0;
					foundstr = buffer;

					// Remove Zeros from beginning of octets
					size_t k = 0;
					size_t foundlen2 = strlen(foundstr);
					for (size_t i = 0; i < foundlen2; i++) {
						if (i == 0 && foundstr[0] == '0') continue;
						if (i == 1 && foundstr[0] == '0' && foundstr[1] == '0') continue;
						if (foundstr[i] == '0' && foundstr[i-1] == '.') continue;
						if (foundstr[i] == '0' && foundstr[i-1] == '0' && foundstr[i-2] == '.') continue;
						obuffer[k++] = foundstr[i];
						}
					obuffer[k] = 0;
					foundstr = obuffer;
				}
			}
			Dial(foundstr);
			return;
		}
		case 'I': // Some strings about firmware
			switch (ScanNumber(scanbuf)) {
			case 3: SendLine("DOSBox Emulated Modem Firmware V1.00"); break;
			case 4: SendLine("Modem compiled for DOSBox version " VERSION); break;
			}
			break;
		case 'E': // Echo on/off
			switch (ScanNumber(scanbuf)) {
			case 0: echo = false; break;
			case 1: echo = true; break;
			}
			break;
		case 'V':
			switch (ScanNumber(scanbuf)) {
			case 0: numericresponse = true; break;
			case 1: numericresponse = false; break;
			}
			break;
		case 'H': // Hang up
			switch (ScanNumber(scanbuf)) {
			case 0:
				if (connected) {
					SendRes(ResNOCARRIER);
					EnterIdleState();
					return;
				}
				// else return ok
			}
			break;
		case 'O': // Return to data mode
			switch (ScanNumber(scanbuf)) {
			case 0:
				if (clientsocket) {
					commandmode = false;
					return;
				} else {
					SendRes(ResERROR);
					return;
				}
			}
			break;
		case 'T': // Tone Dial
		case 'P': // Pulse Dial
			break;
		case 'M': // Monitor
		case 'L': // Volume
			ScanNumber(scanbuf);
			break;
		case 'A': // Answer call
			if (waitingclientsocket) {
				AcceptIncomingCall();
			} else {
				SendRes(ResERROR);
				return;
			}
			return;
		case 'Z': { // Reset and load profiles
			// scan the number away, if any
			ScanNumber(scanbuf);
			if (clientsocket)
				SendRes(ResNOCARRIER);
			Reset();
			break;
		}
		case ' ': // skip space
			break;
		case 'Q': {
			// Response options
			// 0 = all on, 1 = all off,
			// 2 = no ring and no connect/carrier in answermode
			const uint32_t val = ScanNumber(scanbuf);
			if (!(val > 2)) {
				doresponse = val;
				break;
			} else {
				SendRes(ResERROR);
				return;
			}
		}
		case 'S': { // Registers
			const uint32_t index = ScanNumber(scanbuf);
			if (index >= SREGS) {
				SendRes(ResERROR);
				return; //goto ret_none;
			}

			while (scanbuf[0] == ' ')
				scanbuf++; // skip spaces

			if (scanbuf[0] == '=') { // set register
				scanbuf++;
				while (scanbuf[0] == ' ')
					scanbuf++; // skip spaces
				const uint32_t val = ScanNumber(scanbuf);
				reg[index] = val;
				break;
			}
			else if (scanbuf[0] == '?') { // get register
				SendNumber(reg[index]);
				scanbuf++;
				break;
			}
			// else
				// LOG_MSG("SERIAL: Port %" PRIu8 " print reg %" PRIu32
				//         " with %" PRIu8 ".",
				//         GetPortNumber(), index, reg[index]);
		}
		break;
		case '&': { // & escaped commands
			char cmdchar = GetChar(scanbuf);
			switch(cmdchar) {
				case 'K': {
				        const uint32_t val = ScanNumber(scanbuf);
				        if (val < 5)
					        flowcontrol = val;
				        else {
					        SendRes(ResERROR);
					        return;
				        }
				        break;
			        }
			        case 'D': {
				        const uint32_t val = ScanNumber(scanbuf);
				        if (val < 4)
					        dtrmode = val;
				        else {
					        SendRes(ResERROR);
					        return;
				        }
				        break;
			        }
			        case '\0':
				        // end of string
				        SendRes(ResERROR);
				        return;
			        default:
				        LOG_MSG("SERIAL: Port %" PRIu8 " unhandled "
				                "modem command: &%c%" PRIu32 ".",
				                GetPortNumber(), cmdchar, ScanNumber(scanbuf));
				        break;
			        }
			        break;
		}
		case '\\': { // \ escaped commands
			char cmdchar = GetChar(scanbuf);
			switch (cmdchar) {
				case 'N':
					// error correction stuff - not emulated
					if (ScanNumber(scanbuf) > 5) {
						SendRes(ResERROR);
						return;
					}
					break;
				case '\0':
					// end of string
					SendRes(ResERROR);
					return;
				default:
				        LOG_MSG("SERIAL: Port %" PRIu8 " unhandled "
				                "modem command: \\%c%" PRIu32 ".",
				                GetPortNumber(), cmdchar, ScanNumber(scanbuf));
				        break;
			        }
			        break;
		}
		case '\0':
			SendRes(ResOK);
			return;
		default:
			LOG_MSG("SERIAL: Port %" PRIu8 " unhandled modem command: "
			        "%c%" PRIu32 ".",
			        GetPortNumber(), chr, ScanNumber(scanbuf));
			break;
		}
	}
}

void CSerialModem::TelnetEmulation(uint8_t *data, uint32_t size)
{
	for (uint32_t i = 0; i < size; i++) {
		uint8_t c = data[i];
		if (telClient.inIAC) {
			if (telClient.recCommand) {
				if ((c != 0) && (c != 1) && (c != 3)) {
					LOG_MSG("SERIAL: Port %" PRIu8 " unrecognized "
					        "modem option %" PRIu8 ".",
					        GetPortNumber(), c);
					if (telClient.command > 250) {
						/* Reject anything we don't recognize */
						tqueue.push(0xff);
						tqueue.push(252);
						tqueue.push(c); /* We won't do crap! */
					}
			}
			switch (telClient.command) {
				case 251: /* Will */
					if (c == 0) telClient.binary[TEL_SERVER] = true;
					if (c == 1) telClient.echo[TEL_SERVER] = true;
					if (c == 3) telClient.supressGA[TEL_SERVER] = true;
					break;
				case 252: /* Won't */
					if (c == 0) telClient.binary[TEL_SERVER] = false;
					if (c == 1) telClient.echo[TEL_SERVER] = false;
					if (c == 3) telClient.supressGA[TEL_SERVER] = false;
					break;
				case 253: /* Do */
					if (c == 0) {
						telClient.binary[TEL_CLIENT] = true;
						tqueue.push(0xff);
						tqueue.push(251);
						tqueue.push(0); /* Will do binary
						                   transfer */
					}
					if (c == 1) {
						telClient.echo[TEL_CLIENT] = false;
						tqueue.push(0xff);
						tqueue.push(252);
						tqueue.push(1); /* Won't echo
						                   (too lazy) */
					}
					if (c == 3) {
						telClient.supressGA[TEL_CLIENT] = true;
						tqueue.push(0xff);
						tqueue.push(251);
						tqueue.push(3); /* Will Suppress GA */
					}
					break;
				case 254: /* Don't */
					if (c == 0) {
						telClient.binary[TEL_CLIENT] = false;
						tqueue.push(0xff);
						tqueue.push(252);
						tqueue.push(0); /* Won't do binary transfer */
					}
					if (c == 1) {
						telClient.echo[TEL_CLIENT] = false;
						tqueue.push(0xff);
						tqueue.push(252);
						tqueue.push(1); /* Won't echo (fine by me) */
					}
					if (c == 3) {
						telClient.supressGA[TEL_CLIENT] = true;
						tqueue.push(0xff);
						tqueue.push(251);
						tqueue.push(3); /* Will Suppress GA (too lazy) */
					}
					break;
				default:
					LOG_MSG("SERIAL: Port %" PRIu8 " telnet client "
					        "sent IAC %" PRIu8 ".",
					        GetPortNumber(), telClient.command);
					break;
			}
			telClient.inIAC = false;
			telClient.recCommand = false;
			continue;
		} else {
			if (c == 249) {
				/* Go Ahead received */
				telClient.inIAC = false;
				continue;
			}
			telClient.command = c;
			telClient.recCommand = true;

			if ((telClient.binary[TEL_SERVER]) && (c == 0xff)) {
				/* Binary data with value of 255 */
				telClient.inIAC = false;
				telClient.recCommand = false;
				rqueue.push(0xff);
				continue;
			}
		}
		} else {
			if (c == 0xff) {
				telClient.inIAC = true;
				continue;
			}
			rqueue.push(c);
		}
	}
}

void CSerialModem::Timer2() {
	uint32_t txbuffersize = 0;

	// Check for eventual break command
	if (!commandmode) {
		cmdpause++;
		if (cmdpause > (20 * reg[MREG_GUARD_TIME])) {
			if (plusinc == 0) {
				plusinc = 1;
			}
			else if (plusinc == 4) {
				LOG_MSG("SERIAL: Port %" PRIu8 " modem entering "
				        "command mode (escape sequence).",
				        GetPortNumber());
				commandmode = true;
				SendRes(ResOK);
				plusinc = 0;
			}
		}
	}

	// Handle incoming data from serial port, read as much as available
	CSerial::setCTS(true);	// buffer will get 'emptier', new data can be received
	while (tqueue.isUsed()) {
		const uint8_t txval = tqueue.pop();
		if (commandmode) {
			if (echo) {
				rqueue.push(txval);
				// LOG_MSG("SERIAL: Port %" PRIu8 " echo back to queue: %x.",
				//         GetPortNumber(), txval);
			}

			if (txval == '\n')
				continue; // Real modem doesn't seem to skip this?

			if (txval == '\b') {
				if (cmdpos > 0)
					cmdpos--;
			} else if (txval == '\r') {
				DoCommand();
			} else if (cmdpos < 99) {
				cmdbuf[cmdpos] = static_cast<char>(txval);
				cmdpos++;
			}
		}
		else {// + character
			if (plusinc >= 1 && plusinc <= 3 && txval == reg[MREG_ESCAPE_CHAR]) // +
				plusinc++;
			else {
				plusinc = 0;
			}
			cmdpause = 0;
			tmpbuf[txbuffersize] = txval;
			txbuffersize++;
		}
	} // while loop

	if (clientsocket && txbuffersize) {
		// down here it saves a lot of network traffic
		if (!clientsocket->SendArray(tmpbuf,txbuffersize)) {
			SendRes(ResNOCARRIER);
			EnterIdleState();
		}
	}
	// Handle incoming to the serial port
	if (!commandmode && clientsocket && !rqueue.isFull()) {
		constexpr size_t max_recv_size = 16;
		size_t bytes_to_recv = std::min(rqueue.numFreeSlots(), max_recv_size);
		if (!clientsocket->ReceiveArray(tmpbuf, bytes_to_recv)) {
			SendRes(ResNOCARRIER);
			EnterIdleState();
		} else if (bytes_to_recv) {
			// Filter telnet commands
			if (telnet_mode) {
				assert(bytes_to_recv <= UINT32_MAX);
				TelnetEmulation(tmpbuf, static_cast<uint32_t>(
				                                bytes_to_recv));
			} else {
				rqueue.pushMany(tmpbuf, bytes_to_recv);
			}
		}
	}
	// Check for incoming calls
	if (!connected && !waitingclientsocket && serversocket) {
		waitingclientsocket.reset(serversocket->Accept());
		if (waitingclientsocket) {
			if (!CSerial::getDTR() && dtrmode != 0) {
				// accept no calls with DTR off; TODO: AT &Dn
				EnterIdleState();
			} else {
				ringing = true;
				SendRes(ResRING);
				CSerial::setRI(!CSerial::getRI());
				//MIXER_Enable(mhd.chan,true);
				ringtimer = 3000;
				reg[MREG_RING_COUNT] = 0; //Reset ring counter reg
			}
		}
	}
	if (ringing) {
		if (ringtimer <= 0) {
			reg[MREG_RING_COUNT]++;
			if ((reg[MREG_AUTOANSWER_COUNT] > 0) &&
				(reg[MREG_RING_COUNT] >= reg[MREG_AUTOANSWER_COUNT])) {
				AcceptIncomingCall();
				return;
			}
			SendRes(ResRING);
			CSerial::setRI(!CSerial::getRI());

			//MIXER_Enable(mhd.chan,true);
			ringtimer = 3000;
		}
		--ringtimer;
	}

	if (connected && !getDTR()) {
		if (dtrofftimer == 0) {
			switch (dtrmode) {
				case 0:
					// Do nothing.
					//LOG_MSG("SERIAL: Port %" PRIu8 " modem dropped DTR.", GetPortNumber());
					break;
				case 1:
					// Go back to command mode.
				        LOG_MSG("SERIAL: Port %" PRIu8 " modem "
				                "entering command mode due to "
				                "dropped DTR.",
				                GetPortNumber());
				        commandmode = true;
					SendRes(ResOK);
					break;
				case 2:
					// Hang up.
				        LOG_MSG("SERIAL: Port %" PRIu8 " modem hanging "
				                "up due to dropped DTR.",
				                GetPortNumber());
				        SendRes(ResNOCARRIER);
					EnterIdleState();
					break;
				case 3:
					// Reset.
				        LOG_MSG("SERIAL: Port %" PRIu8 " modem "
				                "resetting due to dropped DTR.",
				                GetPortNumber());
				        SendRes(ResNOCARRIER);
					Reset();
					break;
			}
		}

		// Set the timer to -1 once it's expired to turn it off.
		if (dtrofftimer >= 0) {
			dtrofftimer--;
		}
	}
}


//TODO
void CSerialModem::RXBufferEmpty() {
	// see if rqueue has some more bytes
	if (rqueue.isUsed() && (CSerial::getRTS() || (flowcontrol != 3))) {
		uint8_t rbyte = rqueue.pop();
		// LOG_MSG("SERIAL: Port %" PRIu8 " modem sending byte %2x back to UART1.",
		//         GetPortNumber(), rbyte);
		CSerial::receiveByte(rbyte);
	}
}

void CSerialModem::transmitByte(uint8_t val, bool first)
{
	waiting_tx_character = val;
	setEvent(MODEM_TX_EVENT, bytetime); // TX event
	if (first)
		ByteTransmitting();
	// LOG_MSG("SERIAL: Port %" PRIu8 " modem byte %x to be transmitted.",
	// GetPortNumber(), val);
}

void CSerialModem::updatePortConfig(uint16_t, uint8_t lcr)
{
	(void)lcr; // deliberately unused but needed for API compliance
}

void CSerialModem::updateMSR() {
	// think it is not needed
}

void CSerialModem::setBreak(bool) {
	// TODO: handle this
}

void CSerialModem::setRTSDTR(bool rts_state, bool dtr_state) {
	(void) rts_state; // deliberately unused but needed for API compliance
	setDTR(dtr_state);
}
void CSerialModem::setRTS(bool val) {
	(void) val; // deliberately unused but but needed for API compliance
}
void CSerialModem::setDTR(bool val) {
	if (val != oldDTRstate) {
		if (connected && !val) {
			// Start the timer upon losing DTR.
			dtrofftimer = reg[MREG_DTR_DELAY];
		} else {
			dtrofftimer = -1;
		}
	}

	oldDTRstate = val;
}
/*
void CSerialModem::updateModemControlLines() {
        //bool txrdy=tqueue->left();
        //if(CSerial::getRTS() && txrdy) CSerial::setCTS(true);
        //else CSerial::setCTS(tqueue->left());

        // If DTR goes low, hang up.
        if(connected)
                if(oldDTRstate)
                        if(!getDTR()) {
                                SendRes(ResNOCARRIER);
                                EnterIdleState();
                                LOG_MSG("SERIAL: Port %" PRIu8 " modem hung up due to "
                                        "dropped DTR.", GetPortNumber());
                        }

        oldDTRstate = getDTR();
}
*/

#endif // C_MODEM
