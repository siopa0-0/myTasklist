#include <string>
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <psapi.h>
#include <vector>
#include <wtsapi32.h>
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "User32.lib")

using namespace std;

BOOL getProcesses(string option);
void listServices(DWORD PID);
void listVerbose(DWORD PID);
const char* getState(WTS_CONNECTSTATE_CLASS state);
BOOL GetProcessUserName(DWORD processID, char* userName, DWORD userNameSSize, char* cpuTime, DWORD cpuTimeSize, SIZE_T* memUsage);
HWND GetMainWindowHandle(DWORD processID);
SIZE_T GetProcessMemoryUsage(HANDLE hProcess);

int main(int argc, char* argv[])
{
	//process command line arguments
	if (argc != 2)
	{
		printf("\nUsage: myTasklist [options]\nOptions:\n");
		printf("\t%-10s", "/V or /v");
		printf("Displays verbose task information.\n");
		printf("\t%-10s", "/SVC /svc");
		printf("Displays services hosted in each process.\n");
		return 1;
	}

	string flag = argv[1];
	if (flag == "/V" || flag == "/v" || flag == "/SVC" || flag == "/svc")
	{
		if (!getProcesses(flag))
		{
			return 1;
		}
	}
	else if (flag == "/?")
	{
		printf("\nUsage: myTasklist [options]\nOptions:\n");
		printf("\t%-10s", "/V or /v");
		printf("Displays verbose task information.\n");
		printf("\t%-10s", "/SVC /svc");
		printf("Displays services hosted in each process.\n");
		return 1;
	}
	else
	{
		printf("\nInvalid option. Please choose between /SVC and /V. \n");
	}
	return 0;
}

//function to list processes
BOOL getProcesses(string option) 
{
	//create a snapshot of all processes
	HANDLE snapProcesses = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapProcesses == INVALID_HANDLE_VALUE)
	{
		printf("Failed to create a snapshot of the processes. Error: %d\n", GetLastError());
		return FALSE;
	}

	//set the size of the structure using dwSize in order to use Process32First function
	PROCESSENTRY32 processEntries;
	processEntries.dwSize = sizeof(PROCESSENTRY32);
	//Retrieve info of first process in the snapshot and exit if unsuccessful
	if (!Process32First(snapProcesses, &processEntries))
	{
		printf("Process32First Error: %d\n", GetLastError());
		CloseHandle(snapProcesses);
		return FALSE;
	}
	printf("\n%-25s %8s ", "Process Name", "PID");

	if (option == "/SVC" || option == "/svc")
	{
		printf(" Services\n");
		printf("========================= ======== ====================================================================");
	}
	else
	{
		printf("%-12s %8s %-15s %-30s %12s %15s %s", "Session Name", "Session#", "Status", "User Name", "CPU Time", "Mem Usage", "Window Title\n");
		printf("========================= ======== ============ ======== =============== ============================== ============ =============== =========================================================================");
	}
	
	//Iterate over all processes and list info
	do
	{
		DWORD processID = processEntries.th32ProcessID;
		//skip [System Process] (PID 0)
		if (processID == 0)
			continue;
		printf("\n%-25.25s %8lu", processEntries.szExeFile, processID);

		if (option == "/SVC" || option == "/svc")
		{
			//print info for SVC
			listServices(processID);
		}
		else 
		{
			//print info for V
			listVerbose(processID); 
		}
	} while (Process32Next(snapProcesses, &processEntries));

	CloseHandle(snapProcesses);
	return TRUE;
}


//function to list services associated with a process
void listServices(DWORD PID)
{
	//creates handle to connect to service control manager
	SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
	if (scManager == nullptr)
	{
		printf(" Failed to open service manager. Error: %d\n", GetLastError());
		return;
	}

	DWORD bytesNeeded, servicesReturned, resumeHandle = 0;
	//initial call to get required buffer size
	if (!EnumServicesStatusEx(scManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &bytesNeeded, &servicesReturned, &resumeHandle, NULL))
	{
		if (GetLastError() != ERROR_MORE_DATA) //Ignore expected error when buffer is too small
		{
			printf("Failed to enumerate services. Error: %d\n", GetLastError());
			CloseServiceHandle(scManager);
			return;
		}
	}
	
	if (bytesNeeded == 0)
	{
		printf(" N/A\n");
		CloseServiceHandle(scManager);
		return;
	}
	
	vector<BYTE> buffer(bytesNeeded);
	if (!EnumServicesStatusEx(scManager, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buffer.data(), bytesNeeded, &bytesNeeded, &servicesReturned, &resumeHandle, NULL))
	{
		printf("Failed to enumerate services. Error: %d\n", GetLastError());
		CloseServiceHandle(scManager);
		return;
	}

	//to get info of the services
	ENUM_SERVICE_STATUS_PROCESS* services = (ENUM_SERVICE_STATUS_PROCESS*)buffer.data();
	bool foundService = false;

	for (DWORD i = 0; i < servicesReturned; i++)
	{
		if (services[i].ServiceStatusProcess.dwProcessId == PID)
		{
			if (foundService) printf(",");
			printf(" %s", services[i].lpServiceName);
			foundService = true;
		}
	}

	if (!foundService)
	{
		printf(" N/A");
	}
	CloseServiceHandle(scManager);
}

void listVerbose(DWORD PID)
{
	DWORD sessionID = 0;
	WTSINFOA* sessionInfo = NULL;
	DWORD sessionBytes = 0;

	//Get Session ID
	if (!ProcessIdToSessionId(PID, &sessionID))
	{
		printf("Failed to get session ID. Error: %d\n", GetLastError());
	}

	if (WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, sessionID, WTSSessionInfo, (LPSTR*)&sessionInfo, &sessionBytes) && sessionInfo)
	{
		printf(" %-12s %-8lu", (*sessionInfo).WinStationName, sessionID);
		printf(" %-15s", getState((*sessionInfo).State));
		WTSFreeMemory(sessionInfo);
	}

	char resolvedUser[256] = "N/A";
	char cpuTime[50] = "N/A";
	SIZE_T memUsage = 0;
	if (!GetProcessUserName(PID, resolvedUser, sizeof(resolvedUser), cpuTime, sizeof(cpuTime), &memUsage))
	{
		strncpy(resolvedUser, "N/A", sizeof(resolvedUser) - 1);
		printf(" %-30s %12s %12zu KB", resolvedUser, cpuTime, memUsage);

	}
	printf(" %-30s %12s %12zu KB", resolvedUser, cpuTime, memUsage);

	// Get window title
	char windowTitle[256] = "N/A";
	HWND hWnd = GetMainWindowHandle(PID);
	if (hWnd)
	{
		GetWindowTextA(hWnd, windowTitle, sizeof(windowTitle));
	}
	printf(" %-30s", windowTitle);
}

string GetProcessCPUTime(HANDLE hProcess)
{
	FILETIME ftCreate, ftExit, ftKernel, ftUser;
	if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser))
	{
		ULARGE_INTEGER kernelTime, userTime, totalTime;
		kernelTime.LowPart = ftKernel.dwLowDateTime;
		kernelTime.HighPart = ftKernel.dwHighDateTime;
		userTime.LowPart = ftUser.dwLowDateTime;
		userTime.HighPart = ftUser.dwHighDateTime;

		totalTime.QuadPart = (kernelTime.QuadPart + userTime.QuadPart) / 10000000; // Convert to seconds

		int hours = totalTime.QuadPart / 3600;
		int minutes = (totalTime.QuadPart % 3600) / 60;
		int seconds = totalTime.QuadPart % 60;

		char cpuTimeStr[20];
		snprintf(cpuTimeStr, sizeof(cpuTimeStr), "%02d:%02d:%02d", hours, minutes, seconds);
		return string(cpuTimeStr);
	}
	return "N/A";
}

struct EnumData {
	DWORD processID;
	HWND hWnd;
}; 

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	DWORD processID = 0;
	GetWindowThreadProcessId(hWnd, &processID);

	char title[256];
	if (processID == ((EnumData*)lParam)->processID && IsWindowVisible(hWnd) && GetWindowTextA(hWnd, title, sizeof(title)) > 0)
	{
		((EnumData*)lParam)->hWnd = hWnd;
		return FALSE; // Stop enumeration once found
	}
	return TRUE; // Continue enumerating
}

HWND GetMainWindowHandle(DWORD processID)
{
	EnumData data = { processID, NULL };
	EnumWindows(EnumWindowsProc, (LPARAM)&data);
	return data.hWnd;
}

BOOL GetProcessUserName(DWORD processID, char* userName, DWORD userNameSize, char* cpuTime, DWORD cpuTimeSize, SIZE_T* memUsage)
{
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
	if (!hProcess)
	{
		printf("OpenProcess failed for PID %lu with error %lu\n", processID, GetLastError());
		strncpy(userName, "N/A", userNameSize - 1);
		userName[userNameSize - 1] = '\0'; // Ensure null termination
		*memUsage = 0;
		return FALSE;
	}

	HANDLE hToken = NULL;
	if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
	{
		printf("OpenProcessToken failed for PID %lu with error %lu\n", processID, GetLastError());
		CloseHandle(hProcess);
		strncpy(userName, "N/A", userNameSize - 1);
		userName[userNameSize - 1] = '\0';
		return FALSE;
	}

	DWORD tokenInfoLength = 0;
	GetTokenInformation(hToken, TokenUser, NULL, 0, &tokenInfoLength);

	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		printf("GetTokenInformation failed to get buffer size with error %lu\n", GetLastError());
		CloseHandle(hToken);
		CloseHandle(hProcess);
		strncpy(userName, "N/A", userNameSize - 1);
		userName[userNameSize - 1] = '\0';
		return FALSE;
	}

	PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(tokenInfoLength);
	if (!pTokenUser)
	{
		printf("Memory allocation failed for token information.\n");
		CloseHandle(hToken);
		CloseHandle(hProcess);
		strncpy(userName, "N/A", userNameSize - 1);
		userName[userNameSize - 1] = '\0';
		return FALSE;
	}

	if (!GetTokenInformation(hToken, TokenUser, pTokenUser, tokenInfoLength, &tokenInfoLength))
	{
		printf("GetTokenInformation failed for PID %lu with error %lu\n", processID, GetLastError());
		free(pTokenUser);
		CloseHandle(hToken);
		CloseHandle(hProcess);
		strncpy(userName, "N/A", userNameSize - 1);
		userName[userNameSize - 1] = '\0';
		return FALSE;
	}

	// Convert SID to human-readable username
	char accountName[256] = { 0 };
	char domainName[256] = { 0 };
	DWORD accountNameLen = sizeof(accountName);
	DWORD domainNameLen = sizeof(domainName);
	SID_NAME_USE sidType;

	if (LookupAccountSidA(NULL, pTokenUser->User.Sid, accountName, &accountNameLen, domainName, &domainNameLen, &sidType))
	{
		snprintf(userName, userNameSize, "%s\\%s", domainName, accountName);
	}
	else
	{
		printf("LookupAccountSidA failed with error %lu\n", GetLastError());
		strncpy(userName, "N/A", userNameSize - 1);
		userName[userNameSize - 1] = '\0';
	}

	string cpuTimeStr = GetProcessCPUTime(hProcess);
	strncpy(cpuTime, cpuTimeStr.c_str(), cpuTimeSize - 1);
	cpuTime[cpuTimeSize - 1] = '\0';

	*memUsage = GetProcessMemoryUsage(hProcess);

	// Cleanup
	free(pTokenUser);
	CloseHandle(hToken);
	CloseHandle(hProcess);

	return TRUE;  // Ensure function always returns a value
}

SIZE_T GetProcessMemoryUsage(HANDLE hProcess)
{
	PROCESS_MEMORY_COUNTERS pmc;
	if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
	{
		//printf("Debug: Process Memory Usage: %zu KB\n", pmc.WorkingSetSize / 1024);
		return pmc.WorkingSetSize / 1024;
	}
	printf("Debug: Failed to get memory info. Error: %lu\n", GetLastError());
	return 0;
}

const char* getState(WTS_CONNECTSTATE_CLASS state)
{
	switch (state)
	{
	case WTSActive: return "Active";
	case WTSConnected: return "Connected";
	case WTSConnectQuery: return "Connect Query";
	case WTSShadow: return "Shadowing";
	case WTSDisconnected: return "Disconnected";
	case WTSIdle: return "Idle";
	case WTSListen: return "Listening";
	case WTSReset: return "Reset";
	case WTSDown: return "Down";
	case WTSInit: return "Initializing";
	default: return "Unknown";
	}
}

//psexec -s -i -d cmd.exe