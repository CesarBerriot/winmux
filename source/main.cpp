#include <string>
#include <windows.h>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <optional>
#include <functional>
#include <map>
#include <format>
#include <memory>

static int session_window_title_counter = 0;
static std::vector<std::pair<HWND, int>> sessions;
static std::optional<int> active_session;
static HWND host = NULL;
static struct
{	POINT position, size;
	bool maximized;
} placement;

static void single_instance_check();
static HWND acquire_top(HWND);
static void acquire_host();
static void set_visibility(bool);
static void hide() { set_visibility(false); }
static void show() { set_visibility(true); }
static void update_placement();
static void apply_placement();
static HWND get_active_window();
static void create_session();
static void destroy_session();
static void next();
static void previous();
static void attach();
static void detach();
static void update_session_window_title();
static void process_inputs();

int main()
{	single_instance_check();
	acquire_host();
	update_placement();
	create_session();
	while(active_session)
	{	Sleep(30);
		if(!IsWindow(get_active_window()))
			destroy_session();
		else
		{	update_placement();
			update_session_window_title();
			process_inputs();
		}
	}
	return EXIT_SUCCESS;
}

static void single_instance_check()
{	char class_name[] = "WinMux FC10E868-89B0-41A9-81D0-2E1CE3C919DC";
	if(FindWindowA(class_name, NULL))
	{	puts("WinMux is already running.");
		exit(EXIT_SUCCESS);
	}
	else
	{	WNDCLASSA window_class =
		{	.lpfnWndProc = DefWindowProcA,
			.lpszClassName = class_name
		};
		RegisterClassA(&window_class);
		CreateWindowA(class_name, "", 0, 0, 0, 0, 0, NULL, NULL, NULL, 0);
	}
}

static HWND acquire_top(HWND window)
{	HWND parent = window;
	while(parent = GetParent(parent))
		window = parent;
	return window;
}

static void acquire_host()
{	host = acquire_top(GetConsoleWindow());
}

static void set_visibility(bool visibility)
{	HWND window = get_active_window();
	LONG_PTR flags = GetWindowLongPtrA(window, GWL_EXSTYLE);
	if(visibility)
		flags &= ~WS_EX_TOOLWINDOW;
	else
		flags |= WS_EX_TOOLWINDOW;
	SetWindowLongPtrA(window, GWL_EXSTYLE, flags);
	ShowWindow(window, visibility ? SW_SHOW : SW_HIDE);
	if(visibility)
		SetForegroundWindow(window);
}

static void update_placement()
{	RECT rect;
	GetWindowRect(get_active_window(), &rect);
	placement =
	{	.position = { rect.left, rect.top },
		.size = { rect.right - rect.left, rect.bottom - rect.top },
		.maximized = (bool)IsZoomed(get_active_window())
	};
}

static void apply_placement()
{	SetWindowPos
	(	get_active_window(),
		NULL,
		placement.position.x,
		placement.position.y,
		placement.size.x,
		placement.size.y,
		SWP_NOZORDER
	);
	ShowWindow(get_active_window(), placement.maximized ? SW_MAXIMIZE : SW_SHOW);
}

static HWND get_active_window()
{	if(active_session)
		return sessions[*active_session].first;
	else
		return host;
}

static void create_session()
{	STARTUPINFOA startup_information = { .cb = sizeof(STARTUPINFOA) };
	PROCESS_INFORMATION process_information = {};
	CreateProcessA
	(	"C:\\Windows\\System32\\conhost.exe",
		NULL,
		NULL,
		NULL,
		false,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&startup_information,
		&process_information
	);
	CloseHandle(process_information.hThread);
	CloseHandle(process_information.hProcess);
	struct window_enumeration_procedure_data
	{	int process_identifier;
		HWND window;
	} window_enumeration_procedure_data = { .process_identifier = (int)process_information.dwProcessId };
	while(!window_enumeration_procedure_data.window)
		EnumWindows
		(	[](HWND window, LPARAM lparam) -> BOOL
			{	struct window_enumeration_procedure_data * data = (struct window_enumeration_procedure_data*)lparam;
				DWORD window_process_identifier;
				GetWindowThreadProcessId(window, &window_process_identifier);
				bool found = window_process_identifier == data->process_identifier;
				if(found)
					data->window = acquire_top(window);
				return !found;
			},
			(LPARAM)&window_enumeration_procedure_data
		);
	sessions.push_back
	(	{	window_enumeration_procedure_data.window,
			++session_window_title_counter
		}
	);
	hide();
	active_session = sessions.size() - 1;
	show();
	apply_placement();
}

static void destroy_session()
{	if(IsWindow(get_active_window()))
	{	DWORD process_identifier;
		GetWindowThreadProcessId(get_active_window(), &process_identifier);
		HANDLE process = OpenProcess(PROCESS_TERMINATE, false, process_identifier);
		TerminateProcess(process, EXIT_SUCCESS);
		CloseHandle(process);
	}
	sessions.erase(sessions.begin() + *active_session);
	if(sessions.empty())
		active_session.reset();
	else
		if(*active_session >= sessions.size())
			active_session = sessions.size() - 1;
	show();
	apply_placement();
}

static void next()
{	hide();
	if(!active_session)
		active_session = 0;
	else
	{	++*active_session;
		if(*active_session >= sessions.size())
			active_session = 0;
	}
	show();
	apply_placement();
}

static void previous()
{	hide();
	if(!active_session)
		active_session = sessions.size() - 1;
	else
	{	--*active_session;
		if(*active_session < 0)
			active_session = sessions.size() - 1;
	}
	show();
	apply_placement();
}

static void attach()
{	show();
}

static void detach()
{	hide();
}

static void update_session_window_title()
{	auto[window, count] = sessions[*active_session];
	std::string title = std::format("WinMux Session {}", count);
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(title.size() + 1);
	buffer[0] = '\0';
	GetWindowTextA(window, buffer.get(), title.size() + 1);
	if(title != buffer.get())
		SendMessageA(window, WM_SETTEXT, 0, (LPARAM)title.c_str());
}

static void process_inputs()
{	static std::map<int, bool> last_key_states;
	auto is_key_down =
		[](int code)
		{	return
				GetKeyState(code) &
				(1 << (sizeof(SHORT) * 8 - 1));
		};
	auto is_key_freshly_down =
		[is_key_down](int code)
		{	bool last_state = false;
			if(last_key_states.contains(code))
				last_state = last_key_states[code];
			bool state = is_key_down(code);
			bool result = state && !last_state;
			last_key_states[code] = state;
			return result;
		};
	if(is_key_down(VK_CONTROL) && is_key_down(VK_MENU))
	{	struct
		{	int key;
			bool requires_foreground;
			std::function<void()> procedure;
		} callbacks[] =
		{	{ VK_INSERT, true, create_session },
			{ VK_BACK, true, destroy_session },
			{ VK_NEXT, true, next },
			{ VK_PRIOR, true, previous },
			{ VK_HOME, true, detach },
			{ VK_END, false, attach }
		};
		for(decltype(callbacks[0]) callback : callbacks)
			if
			(	is_key_freshly_down(callback.key) &&
				(	!callback.requires_foreground ||
					get_active_window() == GetForegroundWindow()
				)
			)
				callback.procedure();
	}
}
