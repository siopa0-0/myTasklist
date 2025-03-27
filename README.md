# myTasklist
myTasklist is a command-line tool that emulates the functionality of tasklist.exe, allowing users to list information about running processes on a Windows machine. It provides various options to display verbose task information or list services hosted by each process.

Usage: myTasklist [option]

Options:

Verbose Mode (/V) 
    When you use the /V or /v option, myTasklist shows detailed information about each running process, including:

        Session Name: The name of the session the process is associated with.
        Session #: The session number.
        Status: The current status of the session.
        User Name: The user under which the process is running.
        CPU Time: The amount of CPU time consumed by the process.
        Memory Usage: The memory used by the process.
        Window Title: The title of the main window of the process (if applicable).

    Example Output:
            Process Name                   PID Session Name Session# Status          User Name                          CPU Time       Mem Usage Window Title
            ========================= ======== ============ ======== =============== ============================== ============ =============== =========================================================================
            System                           4 Services     0        Disconnected    NT AUTHORITY\SYSTEM                00:00:39         1864 KB N/A


Service Mode (/SVC)
	  When you use the /SVC or /svc option, myTasklist lists the services that are hosted within each process.

    Example Output:
            Process Name                   PID  Services
            ========================= ======== ====================================================================
            svchost.exe                   1552 BrokerInfrastructure, DcomLaunch, PlugPlay, Power, SystemEventsBroker

Help (/?)
Displays help information and lists available options.

Error Handling:
If any operation fails (such as accessing process data, memory, or services), the program will print an error message with a relevant error code for debugging.

Requirements:
Windows OS: myTasklist is designed to run on Windows platforms and requires the appropriate permissions to list processes (admin privileges may not be sufficient to open certain processes). Use PsTools and spawn a shell with elevated privileges by running:
.\PsExec -s -i -d cmd.exe on a command line prompt.

