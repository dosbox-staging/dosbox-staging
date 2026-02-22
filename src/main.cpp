// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "gui/common.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <signal.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#ifdef WIN32
#include <process.h>
#include <windows.h>
#endif

#include "config/config.h"
#include "config/setup.h"
#include "debugger/debugger.h"
#include "dos/dos_locale.h"
#include "gui/mapper.h"
#include "gui/render/render.h"
#include "misc/cross.h"
#include "shell/command_line.h"
#include "shell/shell.h"
#include "utils/checks.h"
#include "utils/env_utils.h"

CHECK_NARROWING();

#ifdef WIN32

#include <winuser.h>
#define STDOUT_FILE "stdout.txt"
#define STDERR_FILE "stderr.txt"

static uint16_t original_code_page = 0;

static void switch_console_to_utf8()
{
	constexpr uint16_t CodePageUtf8 = 65001;
	if (!original_code_page) {
		original_code_page = GetConsoleOutputCP();
		// Don't do anything if we couldn't get the original code page
		if (original_code_page) {
			SetConsoleOutputCP(CodePageUtf8);
		}
	}
}

static void restore_console_encoding()
{
	if (original_code_page) {
		SetConsoleOutputCP(original_code_page);
	}
}

static BOOL WINAPI console_event_handler(DWORD event)
{
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT: raise(SIGTERM); return TRUE;
	case CTRL_C_EVENT:
	default:
		// pass to the next handler
		return FALSE;
	}
}
#endif // WIN32

// Static variable to signal whether there is a valid stdout available. Fixes
// some bugs when --noconsole is used in a read only directory.
static bool no_stdout = false;

// TODO?
void GFX_ShowMsg(const char* format, ...)
{
	char buf[512];

	va_list msg;
	va_start(msg, format);
	safe_sprintf(buf, format, msg);
	va_end(msg);

	buf[sizeof(buf) - 1] = '\0';
	if (!no_stdout) {
		puts(buf); // Else buf is parsed again. (puts adds end of line)
	}
}

static void register_command_line_help_message()
{
	MSG_Add("DOSBOX_HELP",
	        "Usage: %s [OPTION]... [PATH]\n"
	        "\n"
	        "PATH                       If PATH is a directory, it's mounted as C:.\n"
	        "                           If PATH is a bootable disk image (IMA/IMG), it's booted.\n"
	        "                           If PATH is a CD-ROM image (CUE/ISO), it's mounted as D:.\n"
	        "                           If PATH is a DOS executable (BAT/COM/EXE), it's parent\n"
	        "                           path is mounted as C: and the executable is run. When\n"
	        "                           the executable exits, DOSBox Staging quits.\n"
	        "\n"
	        "List of available options:\n"
	        "\n"
	        "  --conf <config_file>     Start with the options specified in <config_file>.\n"
	        "                           Multiple configuration files can be specified.\n"
	        "                           Example: --conf conf1.conf --conf conf2.conf\n"
	        "\n"
	        "  --printconf              Print the location of the primary configuration file.\n"
	        "\n"
	        "  --editconf               Open the primary configuration file in a text editor.\n"
	        "\n"
	        "  --eraseconf              Delete the primary configuration file.\n"
	        "\n"
	        "  --noprimaryconf          Don't load or create the primary configuration file.\n"
	        "\n"
	        "  --nolocalconf            Don't load the local 'dosbox.conf' configuration file\n"
	        "                           if present in the current working directory.\n"
	        "\n"
	        "  --set <setting>=<value>  Set a configuration setting. Multiple configuration\n"
	        "                           settings can be specified. Example:\n"
	        "                           --set mididevice=fluidsynth --set soundfont=mysoundfont.sf2\n"
	        "\n"
	        "  --working-dir <path>     Set working directory to <path>. DOSBox will act as if\n"
	        "                           started from this directory.\n"
	        "\n"
	        "  --list-countries         List all supported countries with their numeric codes.\n"
	        "                           Codes are to be used in the 'country' config setting.\n"
	        "\n"
	        "  --list-layouts           List all supported keyboard layouts with their codes.\n"
	        "                           Codes are to be used in the 'keyboard_layout' config setting.\n"
	        "\n"
	        "  --list-code-pages        List all bundled code pages (screen fonts).\n"
	        "\n"
	        "  --list-shaders           List all available shaders and their paths.\n"
	        "                           Shaders are to be used in the 'shader' config setting.\n"
	        "\n"
	        "  --fullscreen             Start in fullscreen mode.\n"
	        "\n"
	        "  --lang <lang_file>       Start with the language specified in <lang_file>. If set to\n"
	        "                           'auto', tries to detect the language from the host OS.\n"
	        "\n"
	        "  --machine <type>         Emulate a specific type of machine. The machine type has\n"
	        "                           influence on both the emulated video and sound cards.\n"
	        "                           Valid choices are: hercules, cga, cga_mono, tandy,\n"
	        "                           pcjr, ega, svga_s3 (default), svga_et3000, svga_et4000,\n"
	        "                           svga_paradise, vesa_nolfb, vesa_oldvbe.\n"
	        "\n"
	        "  -c <command>             Run the specified DOS command before handling the PATH.\n"
	        "                           Multiple commands can be specified.\n"
	        "\n"
	        "  --noautoexec             Don't run DOS commands from any [autoexec] sections.\n"
	        "\n"
	        "  --exit                   Exit after running '-c <command>'s and [autoexec] sections.\n"
	        "\n"
	        "  --startmapper            Run the mapper GUI.\n"
	        "\n"
	        "  --erasemapper            Delete the default mapper file.\n"
	        "\n"
	        "  --securemode             Enable secure mode by disabling the MOUNT and IMGMOUNT\n"
	        "                           commands.\n"
	        "\n"
	        "  --socket <num>           Run nullmodem on the specified socket number.\n"
	        "\n"
	        "  -h, -?, --help           Print help message and exit.\n"
	        "\n"
	        "  -V, --version            Print version information and exit.\n");
}

static int edit_primary_config()
{
	const auto path = get_primary_config_path();

	if (!path_exists(path)) {
		printf("Primary config does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	auto replace_with_process = [&](const std::string& prog) {
#ifdef WIN32
		_execlp(prog.c_str(), prog.c_str(), path.string().c_str(), (char*)nullptr);
#else
		execlp(prog.c_str(), prog.c_str(), path.string().c_str(), (char*)nullptr);
#endif
	};

	// Loop until one succeeds
	const auto arguments = &control->arguments;

	if (arguments->editconf) {
		for (const auto& editor : *arguments->editconf) {
			replace_with_process(editor);
		}
	}

	std::string env_editor = get_env_var("EDITOR");
	if (!env_editor.empty()) {
		replace_with_process(env_editor);
	}

	replace_with_process("nano");
	replace_with_process("vim");
	replace_with_process("vi");
	replace_with_process("notepad++.exe");
	replace_with_process("notepad.exe");

	LOG_ERR("Can't find any text editors; please set the EDITOR env variable "
	        "to your preferred text editor.\n");

	return 1;
}

static void list_shaders()
{
#if C_OPENGL
	for (const auto& line : RENDER_GenerateShaderInventoryMessage()) {
		printf("%s\n", line.c_str());
	}
#else
	fprintf(stderr,
	        "OpenGL is not supported by this executable "
	        "and is missing the functionality to list shaders.");
#endif
}

static void list_countries()
{
	const auto message_utf8 = DOS_GenerateListCountriesMessage();
	printf("%s\n", message_utf8.c_str());
}

static void list_keyboard_layouts()
{
	const auto message_utf8 = DOS_GenerateListKeyboardLayoutsMessage();
	printf("%s\n", message_utf8.c_str());
}

static void list_code_pages()
{
	const auto message_utf8 = DOS_GenerateListCodePagesMessage();
	printf("%s\n", message_utf8.c_str());
}

static int print_primary_config_location()
{
	const auto path = get_primary_config_path();

	if (!path_exists(path)) {
		printf("Primary config does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	printf("%s\n", path.string().c_str());
	return 0;
}

static int erase_primary_config_file()
{
	const auto path = get_primary_config_path();

	if (!path_exists(path)) {
		printf("Primary config does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	std::error_code ec = {};
	if (!std_fs::remove(path, ec)) {
		fprintf(stderr,
		        "Cannot delete primary config '%s'",
		        path.string().c_str());
		return 1;
	}

	printf("Primary config '%s' deleted.\n", path.string().c_str());
	return 0;
}

static int erase_mapper_file()
{
	const auto path = get_config_dir() / MAPPERFILE;

	if (!path_exists(path)) {
		printf("Default mapper file does not exist at path '%s'\n",
		       path.string().c_str());
		return 1;
	}

	if (path_exists("dosbox.conf")) {
		printf("Local 'dosbox.conf' exists in current working directory; "
		       "mappings will not be reset if the local config specifies "
		       "a custom mapper file.\n");
	}

	std::error_code ec = {};
	if (!std_fs::remove(path, ec)) {
		fprintf(stderr,
		        "Cannot delete mapper file '%s'",
		        path.string().c_str());
		return 1;
	}

	printf("Mapper file '%s' deleted.\n", path.string().c_str());
	return 0;
}

static void init_logger(const CommandLineArguments& args, int argc, char* argv[])
{
	loguru::g_preamble_date    = true;
	loguru::g_preamble_time    = true;
	loguru::g_preamble_uptime  = false;
	loguru::g_preamble_thread  = false;
	loguru::g_preamble_file    = false;
	loguru::g_preamble_verbose = false;
	loguru::g_preamble_pipe    = true;

	if (args.version || args.help || args.printconf || args.editconf ||
	    args.eraseconf || args.list_countries || args.list_layouts ||
	    args.list_code_pages || args.list_shaders || args.erasemapper) {

		loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
	}

	loguru::init(argc, argv);
}

static void maybe_write_primary_config(const CommandLineArguments& args)
{
	// Before loading any configs, write the default primary config if it
	// doesn't exist when:
	//
	// - secure mode is NOT enabled with the '--securemode' option,
	//
	// - AND we're not in portable mode (portable mode is enabled when
	//   'dosbox-staging.conf' exists in the executable directory)
	//
	// - AND the primary config is NOT disabled with the
	//   '--noprimaryconf' option.
	//
	if (!args.securemode && !args.noprimaryconf) {
		const auto primary_config_path = get_primary_config_path();

		if (!config_file_is_valid(primary_config_path)) {
			// No config is loaded at this point, so we're writing
			// the default settings to the primary config.
			MSG_LoadMessages();
			if (control->WriteConfig(primary_config_path)) {
				LOG_MSG("CONFIG: Created primary config file '%s'",
				        primary_config_path.string().c_str());
			} else {
				// TODO convert to notification
				LOG_WARNING("CONFIG: Unable to create primary config file '%s'",
				            primary_config_path.string().c_str());
			}
		}
	}
}

constexpr char version_msg[] =
        R"(%s, version %s

Copyright (C) 2020-2026 The DOSBox Staging Team
License: GNU GPL-2.0-or-later <https://www.gnu.org/licenses/gpl-2.0.html>

This is free software, and you are welcome to change and redistribute it under
certain conditions; please read the COPYING file thoroughly before doing so.
There is NO WARRANTY, to the extent permitted by law.
)";

static std::optional<int> maybe_handle_command_line_output_only_actions(
        const CommandLineArguments& args, const char* program_name)
{
	if (args.version) {
		printf(version_msg, DOSBOX_PROJECT_NAME, DOSBOX_GetDetailedVersion());
		return 0;
	}
	if (args.help) {
		const auto help = format_str(MSG_GetTranslatedRaw("DOSBOX_HELP"),
		                             program_name);
		printf("%s", help.c_str());
		return 0;
	}
	if (args.editconf) {
		return edit_primary_config();
	}
	if (args.eraseconf) {
		return erase_primary_config_file();
	}
	if (args.erasemapper) {
		return erase_mapper_file();
	}
	if (args.printconf) {
		return print_primary_config_location();
	}
	if (args.list_countries) {
		list_countries();
		return 0;
	}
	if (args.list_layouts) {
		list_keyboard_layouts();
		return 0;
	}
	if (args.list_code_pages) {
		list_code_pages();
		return 0;
	}
	if (args.list_shaders) {
		list_shaders();
		return 0;
	}
	return {};
}

static void handle_cli_set_commands(const std::vector<std::string>& set_args)
{
	for (auto command : set_args) {
		trim(command);

		if (command.empty() || command[0] == '%' || command[0] == '\0' ||
		    command[0] == '#' || command[0] == '\n') {
			continue;
		}

		std::vector<std::string> parameters(1, command);

		if (const auto warning_message = control->SetPropertyFromCli(parameters);
		    !warning_message.empty()) {

			// TODO convert to notification
			LOG_WARNING("CONFIG: %s", warning_message.c_str());

		} else {
			auto section = control->GetSection(parameters[0]);
			std::string value(parameters[2]);

			// Due to parsing, there can be a '=' at the
			// start of the value.
			while (value.size() &&
			       (value.at(0) == ' ' || value.at(0) == '=')) {
				value.erase(0, 1);
			}

			for (size_t i = 3; i < parameters.size(); i++) {
				value += (std::string(" ") + parameters[i]);
			}

			auto input_line = parameters[1] + "=" + value;
			bool change_success = section->HandleInputLine(input_line);

			if (!change_success && !value.empty()) {
				// TODO convert to notification
				LOG_WARNING("CONFIG: Cannot set '%s'",
				            input_line.c_str());
			}
		}
	}
}

#if defined(WIN32) && !(C_DEBUGGER)
static void apply_windows_debugger_workaround(const bool is_console_disabled)
{
	// Can't disable the console with debugger enabled
	if (is_console_disabled) {
		FreeConsole();
		// Redirect standard input and standard output
		//
		if (freopen(STDOUT_FILE, "w", stdout) == NULL) {
			// No stdout so don't write messages
			no_stdout = true;
		}
		freopen(STDERR_FILE, "w", stderr);

		// Line buffered
		setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
		// No buffering
		setvbuf(stderr, NULL, _IONBF, BUFSIZ);

	} else {
		if (AllocConsole()) {
			fclose(stdin);
			fclose(stdout);
			fclose(stderr);
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
		}
		SetConsoleTitle("DOSBox Status Window");
	}
}
#endif

static void maybe_create_resource_directories()
{
	auto try_create_resource_dir = [](std_fs::path const& dir) {
		if (!create_dir_if_not_exist(dir)) {
			LOG_WARNING("CONFIG: Can't create directory '%s'",
						dir.string().c_str());
		}
	};
	const auto plugins_dir = get_config_dir() / PluginsDir;
	try_create_resource_dir(plugins_dir);

#if C_OPENGL
	const auto shaders_dir = get_config_dir() / ShadersDir;
	try_create_resource_dir(shaders_dir);
#endif

	const auto soundfonts_dir = get_config_dir() / DefaultSoundfontsDir;
	try_create_resource_dir(soundfonts_dir);

#if C_MT32EMU
	const auto mt32_rom_dir = get_config_dir() / DefaultMt32RomsDir;
	try_create_resource_dir(mt32_rom_dir);
#endif

	const auto soundcanvas_rom_dir = get_config_dir() / DefaultSoundCanvasRomsDir;
	try_create_resource_dir(soundcanvas_rom_dir);

	const auto webserver_dir = get_config_dir() / DefaultWebserverDir;
	try_create_resource_dir(webserver_dir);
}

static void quit_func()
{
	GFX_Quit();
#ifdef WIN32
	restore_console_encoding();
#endif
}

static void wait_for_pid(const int wait_pid)
{
#ifdef WIN32
	// Synchronize permission is all we need for WaitForSingleObject()
	constexpr DWORD DesiredAccess = SYNCHRONIZE;
	constexpr BOOL InheritHandles = FALSE;

	HANDLE process = OpenProcess(DesiredAccess, InheritHandles, wait_pid);
	if (process) {
		// Waits for the process to terminate.
		// If we failed to open it, it's probably already dead.
		constexpr DWORD Timeout = INFINITE;
		WaitForSingleObject(process, Timeout);
		CloseHandle(process);
	}
#else
	for (;;) {
		// Signal of 0 does not actually send a signal.
		// It only checks for existance and permissions of the PID.
		constexpr int Signal = 0;

		int ret = kill(wait_pid, Signal);

		// ESRCH means PID does not exist.
		if (ret == -1 && errno == ESRCH) {
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
#endif
}

int main(int argc, char* argv[])
{
	// Ensure we perform SDL cleanup and restore console settings at exit
	atexit(quit_func);

	CommandLine command_line(argc, argv);
	control = std::make_unique<Config>(&command_line);

	const auto arguments = &control->arguments;

	if (arguments->wait_pid) {
		wait_for_pid(*arguments->wait_pid);
	}

#ifdef WIN32
	switch_console_to_utf8();
#endif

	// Set up logging after command line was parsed and trivial arguments
	// have been handled.
	init_logger(*arguments, argc, argv);

	LOG_MSG("%s version %s", DOSBOX_PROJECT_NAME, DOSBOX_GetDetailedVersion());
	LOG_MSG("---");

	LOG_MSG("Loguru version %d.%d.%d initialised",
	        LOGURU_VERSION_MAJOR,
	        LOGURU_VERSION_MINOR,
	        LOGURU_VERSION_PATCH);

	int return_code = 0;

	try {
		if (!arguments->working_dir.empty()) {
			std::error_code ec;
			std_fs::current_path(arguments->working_dir, ec);
			if (ec) {
				LOG_ERR("Cannot set working directory to '%s'",
				        arguments->working_dir.c_str());
			}
		}

		// Create or determine the location of the config directory
		// (e.g., in portable mode, the config directory is the
		// executable dir).
		//
		// TODO Consider forcing portable mode in secure mode (this
		// could be accomplished by passing a flag to init_config_dir);.
		//
		init_config_dir();

		// Register essential DOS messages needed by some command line
		// switches and during startup or reboot.
		register_command_line_help_message();

		// We need to call this before initialising the modules to to
		// support the '--list-countries' and '--list-layouts' command
		// line options.
		DOS_Locale_AddMessages();

		// We need to call this before initialising the modules to to
		// support the '--list-shaders' command line option.
		RENDER_AddMessages();

		GFX_AddConfigSection();

		// Register the config sections and messages of all the other
		// modules.
		DOSBOX_InitModuleConfigsAndMessages();

		maybe_write_primary_config(*arguments);

		// After DOSBOX_InitModuleConfigsAndMessages() all the config
		// sections have been registered, so we're ready to parse the
		// config files.
		control->ParseConfigFiles(get_config_dir());

		// Handle command line options that don't start the emulator but
		// only perform some actions and print the results to the console.
		assert(argv && argv[0]);
		const auto program_name = argv[0];

		if (const auto return_code = maybe_handle_command_line_output_only_actions(
		            *arguments, program_name);
		    return_code) {

			// Exit to the command line
			return *return_code;
		}

#if defined(WIN32) && !(C_DEBUGGER)
		apply_windows_debugger_workaround(arguments->noconsole);
#endif

#if defined(WIN32)
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)console_event_handler, TRUE);
#endif

		// Handle configuration settings passed with `--set` commands
		// from the CLI.
		handle_cli_set_commands(arguments->set);

		maybe_create_resource_directories();

		GFX_InitSdl();
		DOSBOX_InitModules();
		GFX_InitAndStartGui();

		// All subsystems' hotkeys need to be registered at this point
		// to ensure their hotkeys appear in the graphical mapper.
		MAPPER_BindKeys(get_sdl_section());

		if (arguments->startmapper) {
			MAPPER_DisplayUI();
		}

		// Start emulation
		SHELL_InitAndRun();

		DOSBOX_DestroyModules();
		GFX_Destroy();

	} catch (char* error) {
		// TODO Maybe show popup dialog with the error in addition to
		// logging it (use the tiny osdialog lib).
		LOG_ERR("Unexpected error: %s", error);
		return_code = 1;

	} catch (const std::exception& e) {
		// TODO Maybe show popup dialog with the error in addition to
		// logging it (use the tiny osdialog lib).
		LOG_ERR("Standard library exception: %s", e.what());
		return_code = 1;

	} catch (...) {
		return_code = 1;
	}

	// We already do this at exit, but do clean up earlier in case of normal
	// exit; this works around problems when 'atexit()' order clashes with SDL
	// 2 cleanup order. Happens with SDL_VIDEODRIVER=wayland as of SDL 2.0.12.
	GFX_Quit();

	return return_code;
}
