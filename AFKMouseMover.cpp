#include <windows.h> // Master include file for Windows applications
#include <string> // For string manipulation
#include <thread> // For multithreading
#include <chrono> // For time manipulation
#include <sstream> // For string streams

#pragma comment(lib, "Comctl32.lib") // link with common controls library

#define ID_INPUT 101 // ID for the input field
#define ID_START 102 // ID for the start button
#define ID_STOP  103 // ID for the stop button
#define ID_STATUS 104 // ID for the status label

constexpr int kMinAFKSeconds = 5; // minimum AFK timeout in seconds
constexpr int kLoopSleepMs = 200; // balanced between responsiveness and CPU usage, 50ms CPU usage is high, 500ms is too slow.
constexpr int kConfirmDelayMs = 100; // delay to confirm real user input after AFK detection

HWND hInput, hStartBtn, hStopBtn, hStatus; // handles for UI elements, HWND is a handle to a window in the Windows API
bool isRunning = false; // flag to control the monitoring state
DWORD afkTimeoutMs = 0; // AFK timeout in milliseconds, DWORD is a 32-bit unsigned integer type

DWORD GetLastInputTick() { // Get the tick count of the last user input
	LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) }; // structure to hold last input info
	return GetLastInputInfo(&lii) ? lii.dwTime : GetTickCount(); // if true return the last input tick, otherwise return current tick count
}

void MoveMouseInSquare() { // Function to move the mouse in a square pattern
	INPUT input = { 0 }; // Initialize the INPUT structure, INPUT is from the Windows API to simulate input events
	input.type = INPUT_MOUSE; // Set the type to mouse input
	input.mi.dwFlags = MOUSEEVENTF_MOVE; // Set the mouse event flag to move

	int dx[] = { 10, 0, -10, 0 }; // Define the x offsets for the square movement
	int dy[] = { 0, 10, 0, -10 }; // Define the y offsets for the square movement

	for (int i = 0; i < 4; ++i) { // Loop through the offsets to create a square movement
		input.mi.dx = dx[i]; // Set the x offset
		input.mi.dy = dy[i]; // Set the y offset
		SendInput(1, &input, sizeof(INPUT)); // Send the input event to the system, SendInput is a Windows API function to synthesize mouse and keyboard input
		std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for a short duration to simulate a realistic movement, sleep_for is from the <thread> library
    }
}

void UpdateStatus(const std::wstring& text) { // Function to update the status label
	SetWindowTextW(hStatus, text.c_str()); // Set the text of the status label, SetWindowTextW is a Windows API function to set the text of a window
}

bool ConfirmRealUserInput(DWORD lastTick) { // Function to confirm if the user is real after AFK detection
	DWORD currentTick = GetLastInputTick(); // Get the current tick count of the last user input
	if (currentTick != lastTick) { // If the current tick is different from the last tick
		std::this_thread::sleep_for(std::chrono::milliseconds(kConfirmDelayMs)); // Wait for a short delay to allow for real user input
		return GetLastInputTick() != currentTick; // If the last input tick has changed after the delay, return true indicating real user input
    }
	return false; // If the current tick is the same as the last tick, return false indicating no real user input
}

void MonitorAFK() { // Function to monitor user activity and handle AFK detection
	DWORD lastRealInputTick = GetLastInputTick(); // Store the last tick count of real user input
	DWORD lastMoveTick = GetTickCount(); // Store the last tick count when the mouse was moved
	bool isAFK = false; // Flag to indicate if the user is AFK, false means the user is active

	while (isRunning) { // Main loop to monitor user activity
		DWORD now = GetTickCount(); // Get the current tick count
		DWORD idleTime = now - GetLastInputTick(); // Calculate the idle time since the last user input

		if (!isAFK && idleTime >= afkTimeoutMs) { // If the user is not AFK and the idle time exceeds the AFK timeout
			isAFK = true; // Set the AFK flag to true
			lastRealInputTick = GetLastInputTick(); // Update the last real input tick to the current tick
			lastMoveTick = now; // Reset the last move tick to the current tick
			UpdateStatus(L"Status: AFK"); // Update the status label to indicate AFK status
			MoveMouseInSquare(); // Move the mouse in a square pattern to simulate activity
        }

		if (isAFK) { // If the user is AFK, check for real user input
			if (ConfirmRealUserInput(lastRealInputTick)) { // If real user input is confirmed
				isAFK = false; // Set the AFK flag to false
				UpdateStatus(L"Status: Active"); // Update the status label to indicate active status
				continue; // Skip the rest of the loop to avoid moving the mouse again
            }

			if (now - lastMoveTick >= afkTimeoutMs) { // If the AFK timeout has passed since the last mouse movement
				MoveMouseInSquare(); // Move the mouse in a square pattern to simulate activity
				lastMoveTick = now; // Update the last move tick to the current tick
            }
        }

		std::this_thread::sleep_for(std::chrono::milliseconds(kLoopSleepMs)); // Sleep for a short duration to avoid high CPU usage, kLoopSleepMs is defined as 200ms
    }

	UpdateStatus(L"Status: Stopped"); // Update the status label to indicate monitoring has stopped
}

void StartMonitoring(HWND hwnd) { // Function to start monitoring user activity
	wchar_t buffer[16]; // Buffer to hold the input text, wchar_t is a wide character type used for Unicode strings in Windows
	GetWindowTextW(hInput, buffer, 16); // Get the text from the input field, GetWindowTextW is a Windows API function to get the text of a window, GetWindowTextW is used for wide character strings

	int seconds = _wtoi(buffer); // Convert the input text to an integer, _wtoi is a standard C library function and is used to convert a wide character string to an integer
	if (seconds < kMinAFKSeconds) { // Check if the input is less than the minimum AFK seconds
		MessageBoxW(hwnd, // Show a message box to inform the user
            L"AFK timeout must be at least 5 seconds.\nPlease enter a valid number.",
			L"Invalid Input",
			MB_ICONWARNING | MB_OK); //MB_ICONWARNING is a flag to show a warning icon, MB_OK is a flag to show an OK button
		return; // If the input is invalid, return without starting monitoring
    }

	afkTimeoutMs = seconds * 1000; // Convert seconds to milliseconds for the AFK timeout
	isRunning = true; // Set the running flag to true to indicate monitoring is active
	EnableWindow(hInput, FALSE); // Disable the input field to prevent changes while monitoring
	EnableWindow(hStartBtn, FALSE); // Disable the start button to prevent multiple starts
	EnableWindow(hStopBtn, TRUE); // Enable the stop button to allow stopping the monitoring
	UpdateStatus(L"Status: Monitoring..."); // Update the status label to indicate monitoring is active
	std::thread(MonitorAFK).detach(); // Start the monitoring in a separate thread, detach is used to allow the thread to run independently
}

void StopMonitoring() { // Function to stop monitoring user activity
	if (isRunning) { // Check if monitoring is currently running
		isRunning = false; // Set the running flag to false to stop the monitoring loop
		EnableWindow(hInput, TRUE); // Enable the input field to allow changes
		EnableWindow(hStartBtn, TRUE); // Enable the start button to allow starting monitoring again
		EnableWindow(hStopBtn, FALSE); // Disable the stop button since monitoring is stopped
		UpdateStatus(L"Status: Stopped"); // Update the status label to indicate monitoring has stopped
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { // Window procedure to handle messages for the main window
	switch (uMsg) { // Handle different messages sent to the window
	case WM_CREATE: // Handle the creation of the window
        CreateWindowW(L"STATIC", L"AFK Timeout (sec):", WS_VISIBLE | WS_CHILD,
			20, 20, 120, 20, hwnd, NULL, NULL, NULL); // Create a static label for the AFK timeout input

        hInput = CreateWindowW(L"EDIT", L"10", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
			150, 20, 100, 20, hwnd, (HMENU)ID_INPUT, NULL, NULL); // Create an input field for the AFK timeout, WS_BORDER adds a border to the input field

        hStartBtn = CreateWindowW(L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD | WS_TABSTOP,
			270, 20, 80, 25, hwnd, (HMENU)ID_START, NULL, NULL); // Create a start button to begin monitoring, WS_TABSTOP allows the button to be focused with the Tab key

        hStopBtn = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | WS_DISABLED | WS_TABSTOP,
			270, 55, 80, 25, hwnd, (HMENU)ID_STOP, NULL, NULL); // Create a stop button to stop monitoring, WS_DISABLED disables the button initially

        hStatus = CreateWindowW(L"STATIC", L"Status: Idle", WS_VISIBLE | WS_CHILD,
			20, 60, 230, 20, hwnd, (HMENU)ID_STATUS, NULL, NULL); // Create a static label to show the status of the monitoring, WS_VISIBLE makes the label visible
		break; // End of WM_CREATE

	case WM_COMMAND: // Handle commands from the window, such as button clicks
		switch (LOWORD(wParam)) { // Get the command ID from the low-order word of wParam
		case ID_START: // Handle the start button click
			StartMonitoring(hwnd); // Call the function to start monitoring user activity
			break; // End of ID_START
		case ID_STOP: // Handle the stop button click
			StopMonitoring(); // Call the function to stop monitoring user activity
			break; // End of ID_STOP
        }
		break; // End of WM_COMMAND

	case WM_KEYDOWN: // Handle key down events
		if (wParam == VK_RETURN) { // If the Enter key is pressed
			StartMonitoring(hwnd); // Call the function to start monitoring user activity
        }
		else if (wParam == VK_ESCAPE) { // If the Escape key is pressed
			StopMonitoring(); // Call the function to stop monitoring user activity
        }
		break; // End of WM_KEYDOWN

	case WM_DESTROY: // Handle the window destruction message
		isRunning = false; // Set the running flag to false to stop monitoring if it was running
		PostQuitMessage(0); // Post a quit message to the message queue to exit the application
		break; // End of WM_DESTROY
    }
	return DefWindowProcW(hwnd, uMsg, wParam, lParam); // Call the default window procedure to handle any unprocessed messages
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) { // Entry point for the Windows application
	const wchar_t CLASS_NAME[] = L"AFKMouseMoverWindow"; // Define a class name for the window, wchar_t is a wide character type used for Unicode strings in Windows

	WNDCLASSW wc = {}; // Initialize the WNDCLASS structure, WNDCLASSW is a structure that defines the window class, window class is a blueprint for creating windows in the Windows API
	wc.lpfnWndProc = WindowProc; // Set the window procedure function to handle messages for this window class
	wc.hInstance = hInstance; // Set the instance handle for the window class, hInstance is a handle to the current instance of the application
	wc.lpszClassName = CLASS_NAME; // Set the class name for the window class
	wc.hCursor = LoadCursor(NULL, IDC_ARROW); // Load the default arrow cursor for the window, LoadCursor is a Windows API function to load a cursor resource

	RegisterClassW(&wc); // Register the window class with the operating system, RegisterClassW is a Windows API function to register a window class

	// Create the main window for the application
	HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"AFK Mouse Mover",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, 400, 160,
		NULL, NULL, hInstance, NULL); // CreateWindowExW is a Windows API function to create a window with extended styles, WS_OVERLAPPED creates an overlapped window, WS_CAPTION adds a title bar, WS_SYSMENU adds a system menu

	if (!hwnd) return 0; // Check if the window was created successfully, if not return 0 to indicate failure

	ShowWindow(hwnd, nCmdShow); // Show the window with the specified command show parameter, ShowWindow is a Windows API function to show or hide a window
	SetFocus(hwnd); // Set focus to the main window, SetFocus is a Windows API function to set the keyboard focus to a specified window

	MSG msg = {}; // Initialize the MSG structure to hold messages, MSG is a structure that contains message information for a window
	while (GetMessageW(&msg, NULL, 0, 0)) { // Main message loop to retrieve messages from the message queue, GetMessageW is a Windows API function to retrieve messages for a window
		TranslateMessage(&msg); // Translate virtual-key messages into character messages, TranslateMessage is a Windows API function to translate messages
		DispatchMessageW(&msg); // Dispatch the message to the window procedure for processing, DispatchMessageW is a Windows API function to dispatch messages to the window procedure
    }

	return 0; // Return the exit code of the application, which is 0 for success
}
