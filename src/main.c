/* *********************************************************************
 * Copyright (c) 2014 Marc Abbyad
 * *********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>

#include "vmware\vix.h"

#define POLLINTERVAL 1000	// milliseconds
#define STARTUPWAIT 0	// milliseconds

/*
 * Certain arguments differ when using VIX with VMware Server 2.0
 * and VMware Workstation.
 *
 * Comment out this definition to use this code with VMware Server 2.0.
 */
#define USE_WORKSTATION

#ifdef USE_WORKSTATION

#define  CONNTYPE    VIX_SERVICEPROVIDER_VMWARE_PLAYER	// Set the VMware product type

#define  HOSTNAME ""
#define  HOSTPORT 0
#define  USERNAME ""
#define  PASSWORD ""

#define VMXPATH_INFO "absolute path to the .vmx file for the virtual machine"

#else    // USE_WORKSTATION

/*
 * For VMware Server 2.0
 */

#define CONNTYPE VIX_SERVICEPROVIDER_VMWARE_VI_SERVER

#define HOSTNAME "https://192.2.3.4:8333/sdk"
/*
 * NOTE: HOSTPORT is ignored, so the port should be specified as part
 * of the URL.
 */
#define HOSTPORT 0
#define USERNAME "root"
#define PASSWORD "hideme"

#define  VMPOWEROPTIONS VIX_VMPOWEROP_NORMAL

#define VMXPATH_INFO "datastore-relative path to the .vmx file for the virtual machine, such as \"[standard] ubuntu/ubuntu.vmx\""

#endif    // USE_WORKSTATION


/*
 * Global variables.
 */

static char *progName;
VixError err;
char *vmxPath;
char *command;
VixToolsState powerState = 0;
VixVMPowerOpOptions powerOptions = VIX_VMPOWEROP_LAUNCH_GUI;	// By default launches the UI when powering on the virtual machine.

VixHandle hostHandle = VIX_INVALID_HANDLE;
VixHandle jobHandle = VIX_INVALID_HANDLE;
VixHandle vmHandle = VIX_INVALID_HANDLE;

char sTime[26];
struct tm sTm;
__int64 lTime;


/*
 * Local functions.
 */

////////////////////////////////////////////////////////////////////////////////
static void
usage()
{
   fprintf(stderr, "\n\
Usage: %s <command> <vmxpath> [options]\n\
\n\
  <command>\n\
    the desired action, either `-start`, `-suspend`, `-stop`, or `-status`\n\
  \n\
  <vmxpath>\n\
    %s\n\
  \n\
  [options]\n\
      -nogui: start virtual machine without UI\n\
      -help: shows this help\n\
\n\
Examples:\n\
  %s -start C:\\Users\\Name\\VirtualMachine.vmx\n\
  %s -stop \"C:\\Users\\User Name\\Virtual Machine.vmx\"\n\n\
", progName, VMXPATH_INFO, progName, progName);
}

static char* getTime(){
	time(&lTime);

	if (gmtime_s(&sTm, &lTime)) {
		// error in conversion
		sprintf_s(sTime, sizeof(sTime), "");
	}
	else { 
		strftime(sTime, sizeof(sTime), "%Y-%m-%d %H:%M:%S", &sTm);
	}
	return sTime;
}

static void errorAbort() {
	fprintf(stderr, "[%s] ABORTED\n", getTime());
	Vix_ReleaseHandle(jobHandle);
	Vix_ReleaseHandle(vmHandle);

	VixHost_Disconnect(hostHandle);
	exit(EXIT_FAILURE);
}

static void vmConnectOpen()
{
	jobHandle = VixHost_Connect(VIX_API_VERSION,
		CONNTYPE,
		HOSTNAME, // *hostName,
		HOSTPORT, // hostPort,
		USERNAME, // *userName,
		PASSWORD, // *password,
		0, // options,
		VIX_INVALID_HANDLE, // propertyListHandle,
		NULL, // *callbackProc,
		NULL); // *clientData);
	err = VixJob_Wait(jobHandle,
		VIX_PROPERTY_JOB_RESULT_HANDLE,
		&hostHandle,
		VIX_PROPERTY_NONE);
	if (VIX_FAILED(err)) {
		fprintf(stderr, "[%s] Failed to connect to host [%d]\n", getTime(), VIX_ERROR_CODE(err));
		errorAbort();
	}
	Vix_ReleaseHandle(jobHandle);
	
	jobHandle = VixVM_Open(hostHandle,
		vmxPath,
		NULL, // VixEventProc *callbackProc,
		NULL); // void *clientData);
	err = VixJob_Wait(jobHandle,
		VIX_PROPERTY_JOB_RESULT_HANDLE,
		&vmHandle,
		VIX_PROPERTY_NONE);
	if (VIX_FAILED(err)) {
		fprintf(stderr, "[%s] Failed to open virtual machine [%d]\n", getTime(), VIX_ERROR_CODE(err));
		errorAbort();
	}
	Vix_ReleaseHandle(jobHandle);
	jobHandle = VIX_INVALID_HANDLE;
}

static void vmGetStatus()
{
	// Test if already powered
	err = Vix_GetProperties(vmHandle,
		VIX_PROPERTY_VM_POWER_STATE,
		&powerState,
		VIX_PROPERTY_NONE);
	if (VIX_FAILED(err)) {
		fprintf(stderr, "[%s] Failed to get virtual machine status [%d]\n", getTime(), VIX_ERROR_CODE(err));
		errorAbort();
	}
}

static void vmStatus()
{
	fprintf(stderr, "[%s] Checking status for \"%s\"\n", getTime(), vmxPath);
	vmGetStatus();
	if (VIX_POWERSTATE_POWERED_ON & powerState) {
		fprintf(stderr, "[%s] Virtual machine is powered on\n", getTime());
	}
	else {
		fprintf(stderr, "[%s] Virtual machine is powered off\n", getTime());
	}
}
static void vmStart()
{
	vmGetStatus();
	if (VIX_POWERSTATE_POWERED_ON & powerState) {
		// virtual machine is powered on
		fprintf(stderr, "[%s] Virtual machine already running\n", getTime());
	}
	else {
		jobHandle = VixVM_PowerOn(vmHandle,
			powerOptions,
			VIX_INVALID_HANDLE,
			NULL, // *callbackProc,
			NULL); // *clientData);
		err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
		if (VIX_FAILED(err)) {
			fprintf(stderr, "[%s] Failed to start virtual machine [%d]\n", getTime(), VIX_ERROR_CODE(err));
			errorAbort();
		}
	}
	Sleep(STARTUPWAIT);
	do {
		// Test the power state.
		vmGetStatus(); 
		if (VIX_POWERSTATE_POWERED_ON & powerState) {
			// virtual machine is powered on
			fprintf(stderr, "[%s] Virtual machine running\n", getTime());
		}
		else if (VIX_POWERSTATE_POWERED_OFF & powerState) {
			// virtual machine is powered off
			break;
		}
		else {
			// only checked for Power State, so will never get here
			fprintf(stderr, "[%s] Virtual machine in transition state [%d]\n", getTime(), powerState);
		}
		Sleep(POLLINTERVAL);
	} while (VIX_POWERSTATE_POWERED_ON & powerState);

	Vix_ReleaseHandle(jobHandle);
	jobHandle = VIX_INVALID_HANDLE;
}

static void vmStop()
{
	jobHandle = VixVM_PowerOff(vmHandle,
		VIX_VMPOWEROP_NORMAL,
		NULL, // *callbackProc,
		NULL); // *clientData);
	err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
	if (VIX_FAILED(err)) {
		fprintf(stderr, "[%s] Failed to stop virtual machine, may have already been stopped [%d]\n", getTime(), VIX_ERROR_CODE(err));
		errorAbort();
	}
	else {
		fprintf(stderr, "[%s] Stopped virtual machine [%d]\n", getTime(), VIX_ERROR_CODE(err));
	}
	Vix_ReleaseHandle(jobHandle);
	jobHandle = VIX_INVALID_HANDLE;
}

static void vmSuspend(){
	jobHandle = VixVM_Suspend(vmHandle,
		VIX_VMPOWEROP_NORMAL,
		NULL, // *callbackProc,
		NULL); // *clientData);
	err = VixJob_Wait(jobHandle, VIX_PROPERTY_NONE);
	if (VIX_FAILED(err)) {
		fprintf(stderr, "[%s] Failed to suspend virtual machine, may have already been stopped [%d]\n", getTime(), VIX_ERROR_CODE(err));
		errorAbort();
	}
	else {
		fprintf(stderr, "[%s] Suspended virtual machine [%d]\n", getTime(), VIX_ERROR_CODE(err));
	}
	Vix_ReleaseHandle(jobHandle);
	jobHandle = VIX_INVALID_HANDLE;
}

////////////////////////////////////////////////////////////////////////////////
int
main(int argc, char **argv)
{
    progName = argv[0];
	if (argc >= 3) {
		command = argv[1];
		vmxPath = argv[2];
    } else {
        usage();
        exit(EXIT_FAILURE);
    }

	// check for flags
	for (int i = 1; i < argc; i++){
		if ((strcmp(argv[i], "nogui") == 0) || (strcmp(argv[i], "-nogui") == 0)) {
			powerOptions = VIX_VMPOWEROP_NORMAL;
		}
		if ((strcmp(argv[i], "--h") == 0) || (strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "help") == 0)) {
			usage();
			exit(EXIT_FAILURE);
		}
	}

	vmConnectOpen();
	
	if ((strcmp(command, "start") == 0) || (strcmp(command, "-start") == 0)) {
		vmStart();
	}
	else if ((strcmp(command, "stop") == 0) || (strcmp(command, "-stop") == 0))	{
		vmStop();
	}
	else if ((strcmp(command, "suspend") == 0) || (strcmp(command, "-suspend") == 0)) {
		vmSuspend();
	}
	else if ((strcmp(command, "status") == 0) || (strcmp(command, "-status") == 0)) {
		vmStatus();
	}
	else {
		usage();
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "[%s] Finished\n", getTime());
	
	exit(EXIT_SUCCESS);
}