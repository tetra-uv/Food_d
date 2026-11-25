// gui_main.cpp (professional program communitybridge_pro): starts the program, loads the data and opens the one main window.
//   communitybridge_gui.exe [datafolder] [--selftest | --selftest-flow | --selftest-dpichange | --screenshots <folder>] [--dpi <n>] [--multi] [--dpi-log]
//   communitybridge_gui.exe <copy of the data> --open <role>/<page> [--as <id>] [--size <w>x<h>] [--dpi <n>] [--shot <file.bmp>]   (quick open, test mode)
// Exit codes: 0 fine, 1 start-up problem (normal mode), 2 start-up problem in a test mode, 3 a check failed.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>

#include "AppState.h"
#include "Exceptions.h"
#include "GuiMessage.h"
#include "GuiPage.h"
#include "GuiSelfTest.h"
#include "GuiShell.h"
#include "Pages.h"
#include "PageTable.h"
#include "resource.h"
#include "TextTools.h"
#include "ProDraw.h"
#include "Theme.h"

#include <stdlib.h>

#include <fstream>
#include <string>
#include <vector>

using namespace std;

// The one global pointer to the program state. It is set after the start-up worked.
AppState* theapp = NULL;

// The name of the mutex that lets only one copy of the program run.
static const char* const mutexname = "CommunityBridgeProSingleInstance";

// Returns the folder of the exe with a backslash at the end.
static string exefolder()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    string text = path;
    size_t slash = text.find_last_of("\\/");

    if (slash == string::npos)
    {
        return "";
    }

    return text.substr(0, slash + 1);
}

// Checks whether a folder exists.
static bool folderexists(const string& folder)
{
    DWORD attributes = GetFileAttributesA(folder.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Makes sure a folder name ends with a slash, because the file names are glued behind it.
// A stray quote at the end (from a path that ended with a backslash) is removed first.
static string withslash(const string& folder)
{
    string text = folder;

    while (!text.empty() && text[text.size() - 1] == '"')
    {
        text = text.substr(0, text.size() - 1);
    }

    if (!text.empty() && text[text.size() - 1] != '/' && text[text.size() - 1] != '\\')
    {
        text = text + "\\";
    }

    return text;
}

// The full path of a folder in small letters with backslashes, so two names for the same folder compare equal.
static string normalised(const string& folder)
{
    char full[MAX_PATH];
    DWORD length = GetFullPathNameA(folder.c_str(), MAX_PATH, full, NULL);
    string text = length > 0 && length < MAX_PATH ? string(full) : folder;

    for (size_t pos = 0; pos < text.size(); pos++)
    {
        if (text[pos] == '/')
        {
            text[pos] = '\\';
        }
        else if (text[pos] >= 'A' && text[pos] <= 'Z')
        {
            text[pos] = (char)(text[pos] - 'A' + 'a');
        }
    }

    if (!text.empty() && text[text.size() - 1] != '\\')
    {
        text = text + "\\";
    }

    return text;
}

// The flow test changes files, so it refuses the real data: the sample data of desktop app, the data folder next to
// the exe, and anything inside console app.
static bool protectedfolder(const string& folder)
{
    string path = normalised(folder);
    string beside = normalised(exefolder() + "data");
    string sample = normalised(exefolder() + "..\\data");

    if (path == beside || path == sample)
    {
        return true;
    }

    if (path.find("\\console-app\\") != string::npos)
    {
        return true;
    }

    size_t tail = string("\\desktop-app\\data\\").size();
    return path.size() >= tail && path.substr(path.size() - tail) == "\\desktop-app\\data\\";
}

// The words of the quick open: --open role/page [--as id] [--shot file.bmp] (a test mode for looking at one page).
struct quickopen
{
    string role;  // coordinator, donor or recipient
    string page;  // the short name of the page (home, donations, match and so on)
    string as;    // the donor or recipient id, or the donation id for the match page
    string shot;  // the bmp file to save, empty keeps the window open
    int width;    // the client area, 1280 x 720 unless --size asks for another
    int height;
};

// A quick open refuses what the flow test refuses and also the data folders of the packaged program.
static bool openrefused(const string& folder)
{
    string path = normalised(folder);
    return protectedfolder(folder) || path.find("\\dist\\communitybridge\\") != string::npos ||
           path.find("\\sample_data\\") != string::npos;
}

// Lets the window handle its messages for a while, so the page is painted before the picture.
static void pumpfor(int milliseconds)
{
    DWORD end = GetTickCount() + (DWORD)milliseconds;
    MSG message;

    while (GetTickCount() < end)
    {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        Sleep(10);
    }
}

// Saves a picture of the window as a 24 bit bmp file. Returns false when it could not be made or written.
static bool savewindowbmp(HWND window, const string& path)
{
    RECT frame;
    GetWindowRect(window, &frame);
    int width = frame.right - frame.left;
    int height = frame.bottom - frame.top;
    HDC screen = GetDC(NULL);
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HGDIOBJ old = SelectObject(memory, bitmap);
    // 2 is PW_RENDERFULLCONTENT, which the old headers do not know
    bool painted = PrintWindow(window, memory, 2) != FALSE;
    SelectObject(memory, old);

    BITMAPINFOHEADER head;
    ZeroMemory(&head, sizeof(head));
    head.biSize = sizeof(head);
    head.biWidth = width;
    head.biHeight = height;
    head.biPlanes = 1;
    head.biBitCount = 24;
    head.biCompression = BI_RGB;
    int rowbytes = ((width * 3 + 3) / 4) * 4;
    vector<unsigned char> pixels((size_t)rowbytes * (size_t)height);
    GetDIBits(memory, bitmap, 0, (UINT)height, &pixels[0], (BITMAPINFO*)&head, DIB_RGB_COLORS);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(NULL, screen);

    BITMAPFILEHEADER file;
    ZeroMemory(&file, sizeof(file));
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(head);
    file.bfSize = file.bfOffBits + (DWORD)pixels.size();
    ofstream out(path.c_str(), ios::binary);
    out.write((const char*)&file, sizeof(file));
    out.write((const char*)&head, sizeof(head));
    out.write((const char*)&pixels[0], (streamsize)pixels.size());
    return painted && out.good();
}

// Finds a page by its short name: first in the sidebar of the signed in role, then among all pages (hidden ones too).
static int findpage(const string& name)
{
    for (int index = 0; index < shellnavcount(); index++)
    {
        guipage* page = shellpage(shellnavpage(index));

        if (page != NULL && page->name() == name)
        {
            return shellnavpage(index);
        }
    }

    for (int id = PAGE_WELCOME + 1; id < PAGE_COUNT; id++)
    {
        guipage* page = shellpage(id);

        if (page != NULL && page->name() == name)
        {
            return id;
        }
    }

    return PAGE_NONE;
}

// Signs in as the role, shows the page with a client area of 1280 x 720 and then saves one picture (exit code 0) or keeps the
// window open. Returns 2 when the role, the person, the donation or the page is unknown, or the picture failed.
static int runquickopen(const quickopen& open)
{
    int who = WHO_NONE;
    string name = "";
    int pos = 0;

    // the welcome page itself: step 1 (welcome/roles) or step 2 after pressing Donor or Recipient (welcome/donor, welcome/recipient)
    if (open.role == "welcome")
    {
        ShowWindow(shellgetparts().main, SW_SHOWNORMAL);
        shellsetclient(open.width, open.height);
        shellshow(PAGE_WELCOME, true);

        if (open.page == "donor" || open.page == "recipient")
        {
            SendMessageA(shellpage(PAGE_WELCOME)->window(), WM_COMMAND, MAKEWPARAM(open.page == "donor" ? IDC_W_DONOR : IDC_W_RECIP, BN_CLICKED), 0);
        }

        UpdateWindow(shellgetparts().main);
        pumpfor(400);

        if (open.shot.empty())
        {
            return shellrun();
        }

        bool shot = savewindowbmp(shellgetparts().main, open.shot);
        testlog(string("shot=") + open.shot + " saved=" + (shot ? "1" : "0"));
        return shot ? 0 : 2;
    }

    if (open.role == "coordinator")
    {
        who = WHO_COORDINATOR;
    }
    else if (open.role == "donor" && theapp->data.donorindex.get(open.as, pos))
    {
        who = WHO_DONOR;
        name = theapp->data.donors[pos].name();
    }
    else if (open.role == "recipient" && theapp->data.recipientindex.get(open.as, pos))
    {
        who = WHO_RECIPIENT;
        name = theapp->data.recipients[pos].name();
    }

    if (who == WHO_NONE)
    {
        testlog("error=unknown role or person " + open.role + " " + open.as);
        return 2;
    }

    ShowWindow(shellgetparts().main, SW_SHOWNORMAL);
    shellsignin(who, who == WHO_COORDINATOR ? "" : open.as, name);
    shellsetclient(open.width, open.height);
    string pagename = (open.page == "mydonations") ? "donations" : open.page;

    if (pagename == "match")
    {
        if (!theapp->data.donationindex.get(open.as, pos))
        {
            testlog("error=unknown donation " + open.as);
            return 2;
        }

        openmatchpage(pos);
    }
    else
    {
        int pageid = findpage(pagename);

        if (pageid == PAGE_NONE)
        {
            testlog("error=unknown page " + open.page);
            return 2;
        }

        shellshow(pageid, true);

        // on a list page --as scrolls that row into view without choosing it, so its colours can be seen
        HWND list = GetDlgItem(shellpage(pageid)->window(), IDC_LIST_MAIN);

        if (list != NULL && !open.as.empty() && who == WHO_COORDINATOR)
        {
            LVFINDINFOA find;
            ZeroMemory(&find, sizeof(find));
            find.flags = LVFI_STRING;
            find.psz = open.as.c_str();
            int row = (int)SendMessageA(list, LVM_FINDITEMA, (WPARAM)-1, (LPARAM)&find);

            if (row >= 0)
            {
                SendMessageA(list, LVM_ENSUREVISIBLE, (WPARAM)row, FALSE);
            }
        }
    }

    UpdateWindow(shellgetparts().main);
    pumpfor(400);

    if (open.shot.empty())
    {
        return shellrun();
    }

    bool saved = savewindowbmp(shellgetparts().main, open.shot);
    testlog(string("shot=") + open.shot + " saved=" + (saved ? "1" : "0"));
    return saved ? 0 : 2;
}

// Reports a problem at start-up: a message box, or a line of the test log.
static int reportfailure(bool testing, const string& problem, const string& folder)
{
    if (testing)
    {
        testlog("error=" + problem);
        return 2;
    }

    guishowerror(NULL, "CommunityBridge could not start\n\n" + plaintextkeepfiles(problem) + "\n\nData folder\n" + folder);
    return 1;
}

// Cuts the command line into words. A word can be in double quotes (then it may hold spaces), and a quote
// always starts or ends the quoted part, also after a backslash, so "C:\data\" stays a folder name.
static vector<string> splitwords(const string& line)
{
    vector<string> words;
    string current = "";
    bool inquotes = false;
    bool started = false;

    for (size_t pos = 0; pos < line.size(); pos++)
    {
        char letter = line[pos];

        if (letter == '"')
        {
            inquotes = !inquotes;
            started = true;
        }
        else if ((letter == ' ' || letter == '\t') && !inquotes)
        {
            if (started)
            {
                words.push_back(current);
                current = "";
                started = false;
            }
        }
        else
        {
            current = current + letter;
            started = true;
        }
    }

    if (started)
    {
        words.push_back(current);
    }

    return words;
}

// A second copy of the program: the window of the first one comes to the front.
static void bringfirstforward()
{
    HWND first = FindWindowA("CommunityBridgeProMain", NULL);

    if (first != NULL)
    {
        if (IsIconic(first))
        {
            ShowWindow(first, SW_RESTORE);
        }

        SetForegroundWindow(first);
    }
}

// The entry point of the program. commandline holds the words after the program name.
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR commandline, int showcommand)
{
    (void)previous;

    int kind = TEST_NONE;
    bool multi = false;
    bool dpilog = false;
    int testdpi = 0;
    string folder = "";
    string shots = "";
    bool openmode = false;
    quickopen open;
    open.width = 1280;
    open.height = 720;
    vector<string> words = splitwords(commandline);

    for (size_t pos = 0; pos < words.size(); pos++)
    {
        if (words[pos] == "--selftest")
        {
            kind = TEST_LAYOUT;
        }
        else if (words[pos] == "--selftest-flow")
        {
            kind = TEST_FLOW;
        }
        else if (words[pos] == "--selftest-dpichange")
        {
            kind = TEST_DPICHANGE;
        }
        else if (words[pos] == "--screenshots")
        {
            kind = TEST_SCREENSHOTS;

            if (pos + 1 < words.size())
            {
                pos++;
                shots = words[pos];
            }
        }
        else if (words[pos] == "--open" && pos + 1 < words.size())
        {
            // role/page, for example coordinator/home
            pos++;
            openmode = true;
            size_t slash = words[pos].find('/');
            open.role = words[pos].substr(0, slash);
            open.page = (slash == string::npos) ? "" : words[pos].substr(slash + 1);
        }
        else if (words[pos] == "--as" && pos + 1 < words.size())
        {
            pos++;
            open.as = words[pos];
        }
        else if (words[pos] == "--size" && pos + 1 < words.size())
        {
            // for example 900x600
            pos++;
            size_t cross = words[pos].find('x');
            open.width = atoi(words[pos].c_str());
            open.height = (cross == string::npos) ? 0 : atoi(words[pos].substr(cross + 1).c_str());
        }
        else if (words[pos] == "--shot" && pos + 1 < words.size())
        {
            pos++;
            open.shot = words[pos];
        }
        else if (words[pos] == "--dpi-log")
        {
            dpilog = true;
        }
        else if (words[pos] == "--multi")
        {
            multi = true;
        }
        else if (words[pos] == "--dpi")
        {
            // a test asks for another dpi (96, 120, 144, 168, 192 or 240); it is only used in a test mode
            if (pos + 1 < words.size())
            {
                pos++;
                testdpi = atoi(words[pos].c_str());
            }
        }
        else if (folder.empty())
        {
            folder = words[pos];
        }
    }

    bool testing = (kind != TEST_NONE) || openmode;

    // a quick open looks at the page at 96 dpi unless --dpi asks for another
    if (openmode && testdpi == 0)
    {
        testdpi = 96;
    }

    // one running copy only (not in a test mode and not with --multi)
    HANDLE mutex = NULL;

    if (!testing && !multi)
    {
        mutex = CreateMutexA(NULL, FALSE, mutexname);

        if (mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            bringfirstforward();
            CloseHandle(mutex);
            return 0;
        }
    }

    INITCOMMONCONTROLSEX controls;
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);
    prodrawstart();
    themeinit();

    if (testing)
    {
        testlogopen(exefolder() + "selftest.txt");

        // the dpi of a test (ignored in the normal mode)
        if (testdpi >= 96 && testdpi <= 480)
        {
            themesetdpioverride(testdpi);
        }
    }

    // the data folder: the one on the command line, else "data" next to the exe
    if (folder.empty())
    {
        folder = exefolder() + "data";
    }

    folder = withslash(folder);
    int code = 0;

    if (!folderexists(folder))
    {
        code = reportfailure(testing, "The data folder was not found.", folder);
    }
    else if (openmode && openrefused(folder))
    {
        testlog("error=The quick open needs a copy of the data and refuses this folder.");
        code = 2;
    }
    else if (kind == TEST_FLOW && protectedfolder(folder))
    {
        testlog("error=The flow test changes files, so it needs a copy of the data and refuses this folder.");
        code = 2;
    }
    else
    {
        try
        {
            theapp = new AppState(folder);

            bool made = shellcreate(instance);

            // --dpi-log: one line now, and one for every WM_DPICHANGED that arrives
            if (made && dpilog)
            {
                shelldpilog(exefolder() + "dpi_log.txt");
            }

            if (!made)
            {
                code = reportfailure(testing, "The window could not be made.", folder);
            }
            else if (openmode)
            {
                code = runquickopen(open);

                if (IsWindow(shellgetparts().main))
                {
                    DestroyWindow(shellgetparts().main);
                }
            }
            else if (testing)
            {
                if (kind == TEST_SCREENSHOTS)
                {
                    CreateDirectoryA(shots.c_str(), NULL);
                }

                code = testrun(kind, shots);
                DestroyWindow(shellgetparts().main);
            }
            else
            {
                ShowWindow(shellgetparts().main, showcommand);
                UpdateWindow(shellgetparts().main);
                code = shellrun();
            }
        }
        catch (const AppException& problem)
        {
            code = reportfailure(testing, problem.what(), folder);
        }
        catch (const exception& problem)
        {
            code = reportfailure(testing, problem.what(), folder);
        }
    }

    shelldestroy();
    prodrawstop();
    delete theapp;
    theapp = NULL;

    if (mutex != NULL)
    {
        CloseHandle(mutex);
    }

    return code;
}
