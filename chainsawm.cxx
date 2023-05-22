#include "config.h"
#include <X11/Xlib.h>

#define GET_ATOM(name) XInternAtom(wm.dpy, name, False)

#define GRAB_KEY(keysym, mods) XGrabKey(wm.dpy, XKeysymToKeycode(wm.dpy, keysym), mods, wm.root, False, GrabModeAsync, GrabModeAsync)

Atom atom_window_tp;
Atom atom_window_tp_normal;
Atom atom_window_tp_dialog;
Atom atom_window_tp_menu;
Atom atom_window_tp_splash;
Atom atom_window_tp_toolbar;
Atom atom_window_tp_utility;
Atom atom_window_tp_dock;

Atom atom_wm_strut;

Atom atom_wm_protocols;

Atom atom_wm_delete_window;

template <typename ty_coord = float>
struct vec2 {
public:
	ty_coord x, y;

	vec2() : x(0), y(0) {}
	vec2(ty_coord _x, ty_coord _y) : x(_x), y(_y) {}
	vec2(ty_coord _s) : x(_s), y(_s) {}

	void operator+(const vec2& rhs) {
		this->x += rhs.x;
		this->y += rhs.y;
	}

	void operator-(const vec2& rhs) {
		this->x -= rhs.x;
		this->y -= rhs.y;
	}

	void operator*(const vec2& rhs) {
		this->x *= rhs.x;
		this->y *= rhs.y;
	}

	void operator*(ty_coord rhs) {
		this->x *= rhs;
		this->y *= rhs;
	}

	void operator/(const vec2& rhs) {
		this->x /= rhs.x;
		this->y /= rhs.y;
	}

	void operator/(ty_coord rhs) {
		this->x /= rhs;
		this->y /= rhs;
	}
};

struct client {
public:
	Window xwin;

	bool b_manage;
	bool b_strut;
	bool b_force_kill;

	unsigned int workspace;
	
	struct {
	public:
		long l = 0, r = 0, t = 0, b = 0;
	} strut;

	bool operator == (const client& rhs) {
		return xwin == rhs.xwin;
	}
};

struct {
public:
	Display* dpy = nullptr;
	Screen* screen = nullptr;
	Window root = None;

	int dpy_x = 0, dpy_y = 0;

	std::list<client*> clients;
	client* master[NUM_WORKSPACES] = {nullptr};
	unsigned int master_width = DEFAULT_MASTER_SIZE;

	Cursor cursor;

	unsigned int current_workspace = 1;
	unsigned int last_workspace = 1;

	void set_wkspc(unsigned int wkspc) {
		this->last_workspace = this->current_workspace;
		this->current_workspace = wkspc;
	}

	struct {
	public:
		long l = 0, r = 0, t = 0, b = 0;
	} strut;
} wm;

void die(const std::string&);
Window get_window_under_pointer();
bool win_supports_protocol(Window xwin, Atom protocol);
Atom get_atom_prop(Window win, Atom atom, long offset, long length);
long get_cardinal_prop(Window xwin, Atom atom, long offset);
void kill_client(client* c);
unsigned int managed_count();
bool has_prop(Window xwin, Atom prop);
int wm_detect(Display* dpy, XErrorEvent* ev);
int x_error(Display* dpy, XErrorEvent* ev);
void on_create(const XCreateWindowEvent& xev);
void on_destroy(const XDestroyWindowEvent& xev);
void on_configure(const XConfigureRequestEvent& xev);
void setup_keybinds(Window xwin);
void on_map(const XMapRequestEvent& xev);
client* match_window(const Window& xwin);
void manage();
void set_focus(client* c);
void on_key_press(const XKeyPressedEvent& xev);
void on_button_press(const XButtonPressedEvent& xev);
void user_init();
void main_init();
void init_atoms();

Window get_window_under_pointer() {
	Window root, child;
	int rxr,  ryr;
	int wxr,  wyr;
	unsigned int mask;

	XQueryPointer(wm.dpy, wm.root, &root, &child, &rxr, &ryr, &wxr, &wyr, &mask);

	return root;
}

bool win_supports_protocol(Window xwin, Atom protocol) {
	Atom* protocols = nullptr;
	int protocol_count = 0;

	XGetWMProtocols(wm.dpy, xwin, &protocols, &protocol_count);
	
	if(!protocols) {
		XFree(protocols);
		return false;
	}

	for(int i = 0; i < protocol_count; i++) {
		if(protocols[i] == protocol) {
			XFree(protocols);
			return true;
		}
	}

	XFree(protocols);
	return false;
}

Atom get_atom_prop(Window win, Atom atom, long offset, long length) {
    Atom prop = (unsigned long)NULL, da;
    unsigned char *prop_ret = NULL;
    int di;
    unsigned long dl;
    if (XGetWindowProperty(wm.dpy, win, atom, offset, length, False, XA_ATOM, &da, &di, &dl, &dl, &prop_ret)
        == Success) {
        if (prop_ret) {
            prop = ((Atom *)prop_ret)[0];
            XFree(prop_ret);
        }
    }
    return prop;
}

long get_cardinal_prop(Window xwin, Atom atom, long offset) {
	Atom atr;
	int afr;
	unsigned long nir, bar;
	unsigned char* prop_return;
	if(XGetWindowProperty(wm.dpy, xwin, atom, offset, 1, False, XA_CARDINAL, &atr, &afr, &nir, &bar, &prop_return) == Success) {
		return *((long*)(prop_return));
	} else {
		die("get_atom_cardinal failed");
		// This will never get executed. But it does satisfy compiler warnings tho
		return 0;
	}
}

void kill_client(client* c) {
		if(c->b_force_kill) {
			XKillClient(wm.dpy, c->xwin);
		} else {
			XEvent msg;
			memset(&msg, 0, sizeof(msg));
			msg.xclient.type = ClientMessage;
			msg.xclient.message_type =  atom_wm_protocols;
			msg.xclient.window = c->xwin;
			msg.xclient.format = 32;
			msg.xclient.data.l[0] = atom_wm_delete_window;
			XSendEvent(wm.dpy, c->xwin, false, 0, &msg);
		}
}

void die(const std::string& reason) {
	std::cout << reason << "\n";
	for(auto c : wm.clients) {
		kill_client(c);
	}
	exit(1);
}

bool has_prop(Window xwin, Atom prop) {
	Atom* props = nullptr;
	int num_props;
	props = XListProperties(wm.dpy, xwin, &num_props);

	if(!props) return false;

	for(int i = 0; i < num_props; i++) {
		if(props[i] == prop) return true;
	}

	return false;
}

int wm_detect(Display* dpy, XErrorEvent* ev) {
	die("Another WM detected");
	return 0;
}

int x_error(Display* dpy, XErrorEvent* ev) {
	//std::cout << "X11 error\n";
	return 0;
}

void on_create(const XCreateWindowEvent& xev) {

}



void on_destroy(const XDestroyWindowEvent& xev) {
	for(auto it = wm.clients.begin(); it != wm.clients.end(); it++) {
		auto c = *it;
		if(wm.master[wm.current_workspace] && c->xwin == xev.window) {
			if(c->xwin == wm.master[wm.current_workspace]->xwin) {
				if(wm.clients.size() == 1) {
					wm.master[wm.current_workspace] = nullptr;
				} else {
					if(std::next(it) != wm.clients.end()) {
						wm.master[wm.current_workspace] = (*it);
					} else {
						wm.master[wm.current_workspace] = (*std::prev(it));
					}
				}	
			}
		}

		if(c->b_strut) {
			wm.strut.l = wm.strut.r = wm.strut.t = wm.strut.b = 0;
			for(auto c : wm.clients) {
				wm.strut.l += c->strut.l;
				wm.strut.r += c->strut.r;
				wm.strut.t += c->strut.t;
				wm.strut.b += c->strut.b;
			}
		}

		if(c->xwin == xev.window) {
			std::cout << "Window destroy matched\n";
			wm.clients.erase(it);
			delete c;
			return;
		}
	}
}

void on_configure(const XConfigureRequestEvent& xev) {

}

unsigned int total_managed_count() {
	unsigned int cnt = 0;
	for(auto c : wm.clients) {
		if(c->b_manage) cnt += 1;
	}
	return cnt;
}

unsigned int managed_count() {
	unsigned int cnt = 0;
	for(auto c : wm.clients) {
		if(c->b_manage && c->workspace == wm.current_workspace) cnt += 1;
	}
	return cnt;
}

void setup_keybinds(Window xwin) {
	GRAB_KEY(KEY_KILL_WM, MODIFIER | ShiftMask);
	GRAB_KEY(KEY_CLOSE, MODIFIER);
	GRAB_KEY(KEY_OPEN_LAUNCHER, MODIFIER);
	GRAB_KEY(KEY_OPEN_TERMINAL, MODIFIER);
	GRAB_KEY(KEY_GROW_MASTER, MODIFIER);
	GRAB_KEY(KEY_SHRINK_MASTER, MODIFIER);

	for(unsigned int i = 0; i <= NUM_WORKSPACES; i++) {
		GRAB_KEY(XStringToKeysym(std::to_string(i).c_str()), MODIFIER | ShiftMask);
	}

	XGrabButton(wm.dpy, BUTTON_FOCUS, MODIFIER, xwin, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
}

void on_map(const XMapRequestEvent& xev) {
	client c;
	c.xwin = xev.window;
	c.b_manage = true;
	c.b_strut = false;

	Atom window_tp = get_atom_prop(c.xwin, atom_window_tp, 0, 1);
	if(window_tp == atom_window_tp_dialog || window_tp == atom_window_tp_menu || window_tp == atom_window_tp_splash || window_tp == atom_window_tp_toolbar || window_tp == atom_window_tp_utility || window_tp == atom_window_tp_dock) {
		c.b_manage = false;
	}

	XWindowAttributes attr;
	XGetWindowAttributes(wm.dpy, c.xwin, &attr);
	if(attr.override_redirect) c.b_manage = false;

	client* n = new client(c);
	wm.clients.push_back(n);

	if(managed_count() == 1 && n->b_manage) {
		wm.master[wm.current_workspace] = n;
	}

	if(has_prop(n->xwin, atom_wm_strut)) {
		n->strut.l = get_cardinal_prop(n->xwin, atom_wm_strut, 0);
		n->strut.r = get_cardinal_prop(n->xwin, atom_wm_strut, 1);
		n->strut.t = get_cardinal_prop(n->xwin, atom_wm_strut, 2);
		n->strut.b = get_cardinal_prop(n->xwin, atom_wm_strut, 3);

		wm.strut.l += n->strut.l;
		wm.strut.r += n->strut.r;
		wm.strut.t += n->strut.t;
		wm.strut.b += n->strut.b;
		
		n->b_strut = true;
	}

	n->b_force_kill = !win_supports_protocol(n->xwin, atom_wm_delete_window);

	n->workspace = wm.current_workspace;

	XMapWindow(wm.dpy, xev.window);
	std::cout << "Window mapped\n";
}

client* match_window(const Window& xwin) {
	for(auto c : wm.clients) {
		if(c->xwin == xwin) return c;
	}
	return nullptr;
}

void manage() {
	unsigned int stack_count = 0;
	for(auto c : wm.clients) {
		if(c->b_manage) {
			if(c->workspace == wm.current_workspace) {
				if(managed_count() == 1) {
					XMoveResizeWindow(wm.dpy, c->xwin, wm.strut.l, wm.strut.t, wm.dpy_x - wm.strut.r - wm.strut.l, wm.dpy_y - wm.strut.b - wm.strut.t);
				} else {
					if(wm.master[wm.current_workspace]) {
						if(wm.master[wm.current_workspace] == c) {
							XMoveResizeWindow(wm.dpy, c->xwin, wm.strut.l, wm.strut.t, wm.master_width - wm.strut.l, wm.dpy_y - wm.strut.t - wm.strut.b);
						} else {
							unsigned int py = (((wm.dpy_y - wm.strut.t - wm.strut.b) / (managed_count() - 1)) * stack_count) + wm.strut.t;
							unsigned int w = (wm.dpy_x - wm.master_width - wm.strut.r);
							unsigned int h = ((wm.dpy_y - wm.strut.t - wm.strut.b) / (managed_count() - 1));
							XMoveResizeWindow(wm.dpy, c->xwin, wm.master_width, py, w, h);
							stack_count += 1;
						}
					}
				}
			} else {
				XMoveWindow(wm.dpy, c->xwin, wm.dpy_x + 1, wm.dpy_y + 1);
			}
		}
	}

	Window xwin = get_window_under_pointer();
	XSetInputFocus(wm.dpy, xwin, RevertToPointerRoot, CurrentTime);
}

void set_focus(client* c) {
	wm.master[wm.current_workspace] = c;
}

void on_key_press(const XKeyPressedEvent& xev) {
		
		if(xev.keycode == XKeysymToKeycode(wm.dpy, KEY_KILL_WM) && xev.state & (MODIFIER | ShiftMask)) {
			die("User killed wm");
		} else if(xev.keycode == XKeysymToKeycode(wm.dpy, KEY_CLOSE)) {
			kill_client(match_window(xev.subwindow));

		} else if(xev.keycode == XKeysymToKeycode(wm.dpy, KEY_OPEN_LAUNCHER)) {
			system(CMD_LAUNCHER " &");

		} else if(xev.keycode == XKeysymToKeycode(wm.dpy, KEY_OPEN_TERMINAL)) {
			system(CMD_TERMINAL " &");

		} else if(xev.keycode == XKeysymToKeycode(wm.dpy, KEY_GROW_MASTER)) {
			wm.master_width += PX_INCREMENT_MASTER;
		} else if(xev.keycode == XKeysymToKeycode(wm.dpy, KEY_SHRINK_MASTER)) {
			wm.master_width -= PX_INCREMENT_MASTER;
		}
}

void on_button_press(const XButtonPressedEvent& xev) {
		if(xev.button == BUTTON_FOCUS) {
			set_focus(match_window(xev.subwindow));
		}
}

void user_init() {
	if(fork() == 0) {
		system("~/.config/chainsawm/startup");
		exit(0);
	}
}

void main_init() {
	wm.dpy = XOpenDisplay(0x0);
	wm.root = DefaultRootWindow(wm.dpy);
	wm.screen = DefaultScreenOfDisplay(wm.dpy);
	wm.dpy_x = WidthOfScreen(wm.screen);
	wm.dpy_y = HeightOfScreen(wm.screen);

	if(!wm.dpy) {
		die(std::string("Failed to open display 0x0") + XDisplayName(0x0));
	}

	XSetErrorHandler(wm_detect);
	XSelectInput(wm.dpy, wm.root, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | KeyPressMask);
	XSync(wm.dpy, false);
	XSetErrorHandler(x_error);

	wm.cursor = XcursorLibraryLoadCursor(wm.dpy, "arrow");
	XDefineCursor(wm.dpy, wm.root, wm.cursor);
}

void init_atoms() {
	atom_window_tp = GET_ATOM("_NET_WM_WINDOW_TYPE");
	atom_window_tp_normal = GET_ATOM("_NET_WM_WINDOW_TYPE_NORMAL");
	atom_window_tp_dialog = GET_ATOM("_NET_WM_WINDOW_TYPE_DIALOG");
	atom_window_tp_menu = GET_ATOM("_NET_WM_WINDOW_TYPE_MENU");
	atom_window_tp_splash = GET_ATOM("_NET_WM_WINDOW_TYPE_SPLASH");
	atom_window_tp_toolbar = GET_ATOM("_NET_WM_WINDOW_TYPE_TOOLBAR");
	atom_window_tp_utility = GET_ATOM("_NET_WM_WINDOW_TYPE_UTILITY");
	atom_window_tp_dock = GET_ATOM("_NET_WM_WINDOW_TYPE_DOCK");

	atom_wm_strut = GET_ATOM("_NET_WM_STRUT");

	atom_wm_protocols = GET_ATOM("WM_PROTOCOLS");

	atom_wm_delete_window = GET_ATOM("WM_DELETE_WINDOW");
}

int main() {
	main_init();

	init_atoms();

	setup_keybinds(wm.root);

	user_init();

	XEvent xev;
	while(1) {
		if(XPending(wm.dpy)) {
			XNextEvent(wm.dpy, &xev);

			switch(xev.type) {
				case CreateNotify:
					on_create(xev.xcreatewindow);
					break;
				case DestroyNotify:
					on_destroy(xev.xdestroywindow);
					break;
				case ConfigureRequest:
					on_configure(xev.xconfigurerequest);
					break;
				case MapRequest:
					on_map(xev.xmaprequest);
					break;
				case KeyPress:
					on_key_press(xev.xkey);
					break;
				case ButtonPress:
					on_button_press(xev.xbutton);
					break;
			}
		}
		manage();
	}

	XCloseDisplay(wm.dpy);
}
