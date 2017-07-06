Pstring = secprop->Add_string("mt32.romdir",Property::Changeable::WhenIdle,"");
Pstring->Set_help("Name of the directory where MT-32 Control and PCM ROM files can be found. Emulation requires these files to work.\n"
	"  Accepted file names are as follows:\n"
	"    MT32_CONTROL.ROM or CM32L_CONTROL.ROM - control ROM file.\n"
	"    MT32_PCM.ROM or CM32L_PCM.ROM - PCM ROM file.");

Pbool = secprop->Add_bool("mt32.reverse.stereo",Property::Changeable::WhenIdle,false);
Pbool->Set_help("Reverse stereo channels for MT-32 output");

Pbool = secprop->Add_bool("mt32.verbose",Property::Changeable::WhenIdle,false);
Pbool->Set_help("MT-32 debug logging");

Pbool = secprop->Add_bool("mt32.thread",Property::Changeable::WhenIdle,false);
Pbool->Set_help("MT-32 rendering in separate thread");

const char *mt32chunk[] = {"2", "3", "16", "99", "100",0};
Pint = secprop->Add_int("mt32.chunk",Property::Changeable::WhenIdle,16);
Pint->Set_values(mt32chunk);
Pint->SetMinMax(2,100);
Pint->Set_help("Minimum milliseconds of data to render at once.\n"
	"Increasing this value reduces rendering overhead which may improve performance but also increases audio lag.\n"
	"Valid for rendering in separate thread only.");

const char *mt32prebuffer[] = {"3", "4", "32", "199", "200",0};
Pint = secprop->Add_int("mt32.prebuffer",Property::Changeable::WhenIdle,32);
Pint->Set_values(mt32prebuffer);
Pint->SetMinMax(3,200);
Pint->Set_help("How many milliseconds of data to render ahead.\n"
	"Increasing this value may help to avoid underruns but also increases audio lag.\n"
	"Cannot be set less than or equal to mt32.chunk value.\n"
	"Valid for rendering in separate thread only.");

const char *mt32partials[] = {"8", "9", "32", "255", "256",0};
Pint = secprop->Add_int("mt32.partials",Property::Changeable::WhenIdle,32);
Pint->Set_values(mt32partials);
Pint->SetMinMax(8,256);
Pint->Set_help("The maximum number of partials playing simultaneously.");

const char *mt32DACModes[] = {"0", "1", "2", "3",0};
Pint = secprop->Add_int("mt32.dac",Property::Changeable::WhenIdle,0);
Pint->Set_values(mt32DACModes);
Pint->Set_help("MT-32 DAC input emulation mode\n"
	"Nice = 0 - default\n"
	"Produces samples at double the volume, without tricks.\n"
	"Higher quality than the real devices\n\n"

	"Pure = 1\n"
	"Produces samples that exactly match the bits output from the emulated LA32.\n"
	"Nicer overdrive characteristics than the DAC hacks (it simply clips samples within range)\n"
	"Much less likely to overdrive than any other mode.\n"
	"Half the volume of any of the other modes.\n"
	"Perfect for developers while debugging :)\n\n"

	"GENERATION1 = 2\n"
	"Re-orders the LA32 output bits as in early generation MT-32s (according to Wikipedia).\n"
	"Bit order at DAC (where each number represents the original LA32 output bit number, and XX means the bit is always low):\n"
	"15 13 12 11 10 09 08 07 06 05 04 03 02 01 00 XX\n\n"

	"GENERATION2 = 3\n"
	"Re-orders the LA32 output bits as in later generations (personally confirmed on my CM-32L - KG).\n"
	"Bit order at DAC (where each number represents the original LA32 output bit number):\n"
	"15 13 12 11 10 09 08 07 06 05 04 03 02 01 00 14");

const char *mt32analogModes[] = {"0", "1", "2", "3",0};
Pint = secprop->Add_int("mt32.analog",Property::Changeable::WhenIdle,2);
Pint->Set_values(mt32analogModes);
Pint->Set_help("MT-32 analogue output emulation mode\n"
	"Digital = 0\n"
	"Only digital path is emulated. The output samples correspond to the digital output signal appeared at the DAC entrance.\n"
	"Fastest mode.\n\n"

	"Coarse = 1\n"
	"Coarse emulation of LPF circuit. High frequencies are boosted, sample rate remains unchanged.\n"
	"A bit better sounding but also a bit slower.\n\n"

	"Accurate = 2 - default\n"
	"Finer emulation of LPF circuit. Output signal is upsampled to 48 kHz to allow emulation of audible mirror spectra above 16 kHz,\n"
	"which is passed through the LPF circuit without significant attenuation.\n"
	"Sounding is closer to the analog output from real hardware but also slower than the modes 0 and 1.\n\n"

	"Oversampled = 3\n"
	"Same as the default mode 2 but the output signal is 2x oversampled, i.e. the output sample rate is 96 kHz.\n"
	"Even slower than all the other modes but better retains highest frequencies while further resampled in DOSBox mixer.");

Pint = secprop->Add_int("mt32.output.gain", Property::Changeable::WhenIdle, 100);
Pint->SetMinMax(0,1000);
Pint->Set_help("Output gain of MT-32 emulation in percent, 100 is the default value, the allowed maximum is 1000.");

const char *mt32reverbModes[] = {"0", "1", "2", "3", "auto",0};
Pstring = secprop->Add_string("mt32.reverb.mode",Property::Changeable::WhenIdle,"auto");
Pstring->Set_values(mt32reverbModes);
Pstring->Set_help("MT-32 reverb mode");

Pint = secprop->Add_int("mt32.reverb.output.gain", Property::Changeable::WhenIdle, 100);
Pint->SetMinMax(0,1000);
Pint->Set_help("Reverb output gain of MT-32 emulation in percent, 100 is the default value, the allowed maximum is 1000.");

const char *mt32reverbTimes[] = {"0", "1", "2", "3", "4", "5", "6", "7",0};
Pint = secprop->Add_int("mt32.reverb.time",Property::Changeable::WhenIdle,5);
Pint->Set_values(mt32reverbTimes);
Pint->Set_help("MT-32 reverb decaying time");

const char *mt32reverbLevels[] = {"0", "1", "2", "3", "4", "5", "6", "7",0};
Pint = secprop->Add_int("mt32.reverb.level",Property::Changeable::WhenIdle,3);
Pint->Set_values(mt32reverbLevels);
Pint->Set_help("MT-32 reverb level");

Pint = secprop->Add_int("mt32.rate", Property::Changeable::WhenIdle, 44100);
Pint->Set_values(rates);
Pint->Set_help("Sample rate of MT-32 emulation.");

const char *mt32srcQuality[] = {"0", "1", "2", "3",0};
Pint = secprop->Add_int("mt32.src.quality", Property::Changeable::WhenIdle, 2);
Pint->Set_values(mt32srcQuality);
Pint->Set_help("MT-32 sample rate conversion quality\n"
	"Value '0' is for the fastest conversion, value '3' provides for the best conversion quality. Default is 2.");

Pbool = secprop->Add_bool("mt32.niceampramp", Property::Changeable::WhenIdle, true);
Pbool->Set_help("Toggles \"Nice Amp Ramp\" mode that improves amplitude ramp for sustaining instruments.\n"
	"Quick changes of volume or expression on a MIDI channel may result in amp jumps on real hardware.\n"
	"When \"Nice Amp Ramp\" mode is enabled, amp changes gradually instead.\n"
	"Otherwise, the emulation accuracy is preserved.\n"
	"Default is true.");
