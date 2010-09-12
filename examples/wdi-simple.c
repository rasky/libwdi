/*
* wdi-simple.c: Console Driver Installer for a single USB device
* Copyright (c) 2010 Pete Batard <pbatard@gmail.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include "getopt/getopt.h"
#else
#include <getopt.h>
#endif
#include "libwdi.h"

#define oprintf(...) do {if (!opt_silent) printf(__VA_ARGS__);} while(0)

/*
 * Change these values according to your device if
 * you don't want to provide parameters
 */
#define DESC        "Microsoft XBox Controller Type S"
#define VID         0x045E
#define PID         0x0289
#define INF_NAME    "usb_device.inf"
#define DEFAULT_DIR "usb_driver"


void usage(void)
{
	printf("\n");
	printf("-n, --name <name>          set the device name\n");
	printf("-f, --inf <name>           set the inf name\n");
	printf("-m, --manufacturer <name>  set the manufacturer name\n");
	printf("-v, --vid <id>             set the vendor ID (VID)\n");
	printf("-p, --pid <id>             set the product ID (PID)\n");
	printf("-i, --iid <id>             set the interface ID (MI)\n");
	printf("-t, --type <driver_type>   set the driver to install\n");
	printf("                           (0=WinUSB, 1=libusb0.sys, 2=custom)\n");
	printf("-d, --dest <dir>           set the extraction directory\n");
	printf("-x, --extract              extract files only (don't install)\n");
	printf("-s, --silent               silent mode\n");
	printf("-b, --progressbar          display a progress bar during install\n");
	printf("-l, --log                  set log level (0 = debug, 4 = none)\n");
	printf("-h, --help                 display usage\n");
	printf("\n");
}

// from http://support.microsoft.com/kb/124103/
HWND GetConsoleHwnd(void)
{
	HWND hwndFound;
	char pszNewWindowTitle[128];
	char pszOldWindowTitle[128];

	GetConsoleTitle(pszOldWindowTitle, 128);
	wsprintf(pszNewWindowTitle,"%d/%d", GetTickCount(), GetCurrentProcessId());
	SetConsoleTitle(pszNewWindowTitle);
	Sleep(40);
	hwndFound = FindWindow(NULL, pszNewWindowTitle);
	SetConsoleTitle(pszOldWindowTitle);
	return hwndFound;
}

int __cdecl main(int argc, char** argv)
{
	struct wdi_device_info *ldev, dev = {NULL, VID, PID, false, 0, DESC, NULL, NULL, NULL};
	struct wdi_options_create_list ocl = {true, true, true};
	struct wdi_options_prepare_driver opd = {WDI_WINUSB, NULL};
	struct wdi_options_install_driver oid = {NULL};
	int c, r;
	int opt_silent = 0, opt_extract = 0, log_level = WDI_LOG_LEVEL_WARNING;
	char *inf_name = INF_NAME;
	char *ext_dir = DEFAULT_DIR;
	bool matching_device_found;

	static struct option long_options[] = {
		{"name", required_argument, 0, 'n'},
		{"manufacturer", required_argument, 0, 'm'},
		{"dir", required_argument, 0, 'd'},
		{"vid", required_argument, 0, 'v'},
		{"pid", required_argument, 0, 'p'},
		{"iid", required_argument, 0, 'i'},
		{"type", required_argument, 0, 't'},
		{"dest", no_argument, 0, 'x'},
		{"silent", no_argument, 0, 's'},
		{"log", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	while(1)
	{
		c = getopt_long(argc, argv, "n:f:m:d:v:p:i:l:hxsb", long_options, NULL);
		if (c == -1)
			break;
		switch(c) {
		case 'n':
			dev.desc = optarg;
			break;
		case 'm':
			opd.vendor_name = optarg;
			break;
		case 'f':
			inf_name = optarg;
			break;
		case 'd':
			ext_dir = optarg;
			break;
		case 'v':
			dev.vid = (unsigned short)strtol(optarg, NULL, 0);
			break;
		case 'p':
			dev.pid = (unsigned short)strtol(optarg, NULL, 0);
			break;
		case 'i':
			dev.is_composite = true;
			dev.mi = (unsigned char)strtol(optarg, NULL, 0);
			break;
		case 't':
			opd.driver_type = (int)strtol(optarg, NULL, 0);
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 'x':
			opt_extract = 1;
			break;
		case 's':
			opt_silent = 1;
			log_level = WDI_LOG_LEVEL_NONE;
			break;
		case 'b':
			oid.hWnd =  GetConsoleHwnd();
			break;
		case 'l':
			log_level = (int)strtol(optarg, NULL, 0);
			break;
		default:
			usage();
			exit(0);
		}
	}

	wdi_set_log_level(log_level);

	oprintf("Extracting driver files...\n");
	r = wdi_prepare_driver(&dev, ext_dir, inf_name, &opd);
	oprintf("  %s\n", wdi_strerror(r));
	if ((r != WDI_SUCCESS) || (opt_extract))
		return r;

	oprintf("Installing driver(s)...\n");

	// Try to match against a plugged device to avoid device manager prompts
	matching_device_found = false;
	if (wdi_create_list(&ldev, &ocl) == WDI_SUCCESS) {
		r = WDI_SUCCESS;
		for (; (ldev != NULL) && (r == WDI_SUCCESS); ldev = ldev->next) {
			if ( (ldev->vid == dev.vid) && (ldev->pid == dev.pid) && (ldev->mi == dev.mi) ) {
				dev.hardware_id = ldev->hardware_id;
				dev.device_id = ldev->device_id;
				matching_device_found = true;
				oprintf("  %s: ", dev.hardware_id);
				fflush(stdout);
				r = wdi_install_driver(&dev, ext_dir, inf_name, &oid);
				oprintf("%s\n", wdi_strerror(r));
			}
		}
	}

	// No plugged USB device matches this one -> install driver
	if (!matching_device_found) {
		r = wdi_install_driver(&dev, ext_dir, inf_name, &oid);
		oprintf("  %s\n", wdi_strerror(r));
	}

	return r;
}