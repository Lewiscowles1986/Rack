#include "common.hpp"
#include "random.hpp"
#include "asset.hpp"
#include "midi.hpp"
#include "rtmidi.hpp"
#include "keyboard.hpp"
#include "gamepad.hpp"
#include "bridge.hpp"
#include "settings.hpp"
#include "engine/Engine.hpp"
#include "app/Scene.hpp"
#include "plugin.hpp"
#include "app.hpp"
#include "patch.hpp"
#include "ui.hpp"
#include "system.hpp"

#include <osdialog.h>
#include <unistd.h> // for getopt
#include <signal.h> // for signal

#if defined ARCH_WIN
	#include <windows.h> // for CreateMutex
#endif

using namespace rack;


static void fatalSignalHandler(int sig) {
	// Only catch one signal
	static bool caught = false;
	bool localCaught = caught;
	caught = true;
	if (localCaught)
		exit(1);

	FATAL("Fatal signal %d. Stack trace:\n%s", sig, system::getStackTrace().c_str());

	osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, "Rack has crashed. See log.txt for details.");

	exit(1);
}


int main(int argc, char *argv[]) {
#if defined ARCH_WIN
	// Windows global mutex to prevent multiple instances
	// Handle will be closed by Windows when the process ends
	HANDLE instanceMutex = CreateMutex(NULL, true, app::APP_NAME);
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, "Rack is already running. Multiple Rack instances are not supported.");
		exit(1);
	}
	(void) instanceMutex;
#endif

	bool devMode = false;
	std::string patchPath;

	// Parse command line arguments
	int c;
	opterr = 0;
	while ((c = getopt(argc, argv, "ds:u:")) != -1) {
		switch (c) {
			case 'd': {
				devMode = true;
			} break;
			case 's': {
				asset::systemDir = optarg;
			} break;
			case 'u': {
				asset::userDir = optarg;
			} break;
			default: break;
		}
	}
	if (optind < argc) {
		patchPath = argv[optind];
	}

	// Initialize environment
	asset::init(devMode);
	logger::init(devMode);
	// We can now install a signal handler and log the output
	// Mac has its own decent crash handler
#if defined ARCH_LIN || defined ARCH_WIN
	signal(SIGABRT, fatalSignalHandler);
	signal(SIGFPE, fatalSignalHandler);
	signal(SIGILL, fatalSignalHandler);
	signal(SIGSEGV, fatalSignalHandler);
	signal(SIGTERM, fatalSignalHandler);
#endif

	// Log environment
	INFO("%s v%s", app::APP_NAME, app::APP_VERSION);
	if (devMode)
		INFO("Development mode");
	INFO("System directory: %s", asset::systemDir.c_str());
	INFO("User directory: %s", asset::userDir.c_str());

	random::init();
	midi::init();
	rtmidiInit();
	bridgeInit();
	keyboard::init();
	gamepad::init();
	ui::init();
	plugin::init(devMode);
	windowInit();
	INFO("Initialized environment");

	// Initialize app
	settings.load(asset::user("settings.json"));
	app::init();
	APP->scene->devMode = devMode;
	APP->patch->init(patchPath);

	INFO("Initialized app");

	APP->engine->start();
	APP->window->run();
	INFO("Window closed");
	APP->engine->stop();

	// Destroy app
	APP->patch->save(asset::user("autosave.vcv"));
	app::destroy();
	settings.save(asset::user("settings.json"));
	INFO("Cleaned up app");

	// Destroy environment
	windowDestroy();
	plugin::destroy();
	ui::destroy();
	bridgeDestroy();
	midi::destroy();
	INFO("Cleaned up environment");
	logger::destroy();

	return 0;
}
