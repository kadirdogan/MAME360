"""Execute the actual favorites methods with fault-injected file/XUI adapters."""
from pathlib import Path
import subprocess, sys, shutil
base=Path(__file__).resolve().parent
repo = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else base.parent
source=(repo/'mame_0.72/src/xbox/RomList.cpp').read_text()
actual=source[source.index('// ROM short names'):source.index('LPCWSTR MultiCharToUniChar')]
stub=r'''
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
typedef unsigned long DWORD;
typedef int HANDLE;
typedef int HXUIOBJ;
typedef int HRESULT;
typedef int BOOL;
struct LARGE_INTEGER { long long QuadPart; };
#define TRUE 1
#define FALSE 0
#define S_OK 0
#define INVALID_HANDLE_VALUE -1
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 1
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0
#define ERROR_FILE_NOT_FOUND 2
#define MOVEFILE_REPLACE_EXISTING 1
#define VK_PAD_X 0x5802
#define VK_PAD_Y 0x5803
#define XUI_INPUT_FLAG_REPEAT 1
struct XUIMessageInput { DWORD dwKeyCode, dwFlags; };
struct GameDriver { const char *name, *description; };
GameDriver game0={"shinobi", "Same title"}, game1={"clone", "Same title"}, game2={"other", "Z title"};
const GameDriver* drivers[]={&game0,&game1,&game2,0};
static map<string,string> files;
static string opened, statusText;
static int failure=0, selection=-1, itemCount=0, writes=0;
static DWORD lastError=0;
static HXUIOBJ hRomListScene=1;
static void logerror(const char*,...) {}
static DWORD GetLastError() { return lastError; }
static HANDLE CreateFileA(const char* path,int access,int,void*,int,int,void*) {
    opened=path;
    if (failure==1 || (failure==2 && access==GENERIC_WRITE)) { lastError=5; return -1; }
    if(access==GENERIC_READ && !files.count(path)) { lastError=2; return -1; }
    if(access==GENERIC_WRITE) { files[path]=""; ++writes; }
    return 1;
}
static BOOL GetFileSizeEx(HANDLE,LARGE_INTEGER* size) { size->QuadPart=files[opened].size(); return 1; }
static BOOL ReadFile(HANDLE,void* dest,DWORD size,DWORD* count,void*) {
    *count=size; memcpy(dest,files[opened].data(),size); return failure!=3;
}
static BOOL WriteFile(HANDLE,const void* src,DWORD size,DWORD* count,void*) {
    *count=failure==4 ? size/2 : size; files[opened].assign((const char*)src,*count); return 1;
}
static BOOL FlushFileBuffers(HANDLE) { return failure!=5; }
static BOOL CloseHandle(HANDLE) { return 1; }
static BOOL MoveFileExA(const char* from,const char* to,int) {
    if(failure==6) return 0;
    files[to]=files[from]; files.erase(from); return 1;
}
static void XuiElementGetChildById(HXUIOBJ,const wchar_t*,HXUIOBJ* out) { *out=1; }
static void XuiTextElementSetText(HXUIOBJ,const wchar_t* text) { statusText=string(text,text+wcslen(text)); }
static int XuiListGetCurSel(HXUIOBJ,void*) { return selection; }
static void XuiListSetCurSel(HXUIOBJ,int i) { assert(i>=0 && i<itemCount); selection=i; }
static void XuiListSetCurSelVisible(HXUIOBJ,int i) { assert(i==selection); }
class CRomList {
public:
    int RomCount, m_hObj;
    vector<string> m_vecAvailRomList;
    CRomList():RomCount(0),m_hObj(1) {}
    void DeleteItems(int,int count) { assert(count==itemCount); itemCount=0; selection=-1; }
    void InsertItems(int,int count) { itemCount=count; }
    void RebuildVisible(int,int);
    HRESULT OnKeyDown(XUIMessageInput*,BOOL&);
};
'''
tests=r'''
static void reload() { s_favorites.clear(); s_favoritesLoaded=false; s_favoritesWritable=true; LoadFavorites(); }
int main() {
    LoadFavorites(); assert(s_favorites.empty() && s_favoritesWritable);
    CRomList list; s_availableDrivers.push_back(0); s_availableDrivers.push_back(1); s_availableDrivers.push_back(2);
    sort(s_availableDrivers.begin(),s_availableDrivers.end(),DriverLess);
    list.RebuildVisible(0,0); assert(s_visibleDrivers[selection]==0);
    XUIMessageInput key={VK_PAD_Y,0}; BOOL handled=0;
    list.OnKeyDown(&key,handled); assert(handled && s_favorites.count("shinobi"));
    assert(files["GAME:\\favorites.txt"]=="shinobi\r\n");
    assert(s_visibleDrivers[selection]==0 && list.m_vecAvailRomList[selection].find("[*] ")==0);
    key.dwFlags=1; int oldWrites=writes; list.OnKeyDown(&key,handled); assert(writes==oldWrites);
    key.dwFlags=0; key.dwKeyCode=VK_PAD_X; list.OnKeyDown(&key,handled);
    assert(list.RomCount==1 && s_visibleDrivers[0]==0);
    key.dwKeyCode=VK_PAD_Y; list.OnKeyDown(&key,handled); assert(list.RomCount==0);
    list.OnKeyDown(&key,handled); assert(list.RomCount==0);
    key.dwKeyCode=VK_PAD_X; list.OnKeyDown(&key,handled); assert(list.RomCount==3);
    list.RebuildVisible(0,0); key.dwKeyCode=VK_PAD_Y; list.OnKeyDown(&key,handled);
    reload(); assert(s_favorites.count("shinobi"));
    s_favorites.insert("uninstalled"); assert(SaveFavorites(s_favorites));
    for(int fault=2;fault<=6;fault++) {
        if(fault==3) continue;
        failure=fault; string original=files["GAME:\\favorites.txt"];
        list.OnKeyDown(&key,handled);
        assert(s_favorites.count("shinobi") && files["GAME:\\favorites.txt"]==original);
        assert(statusText.find("failed")!=string::npos);
    }
    failure=0; files["GAME:\\favorites.txt"]="uninstalled\r\nshinobi\r\nshinobi\r\n";
    reload(); assert(s_favorites.size()==2 && s_favorites.count("uninstalled"));
    list.OnKeyDown(&key,handled); assert(files["GAME:\\favorites.txt"]=="uninstalled\r\n");
    files["GAME:\\favorites.txt"]="../bad\n"; reload(); assert(!s_favoritesWritable);
    assert(!SaveFavorites(s_favorites));
    failure=1; reload(); assert(!s_favoritesWritable);
    failure=3; reload(); assert(!s_favoritesWritable);
    puts("PASS: favorites persistence/reload, duplicate labels, missing ROM preservation, repeat suppression");
    puts("PASS: empty filter recovery, selection identity, open/short-write/flush/rename/read failure protection");
}
'''
out=base/'generated';out.mkdir(exist_ok=True)
(out/'favorites_test.cpp').write_text(stub+actual+tests)
compiler=shutil.which('g++')
assert compiler, 'g++ required for host tests'
subprocess.run([compiler,'-std=c++03','-Wall','-Wextra',str(out/'favorites_test.cpp'),'-o',str(out/'favorites_test.exe')],check=True)
subprocess.run([str(out/'favorites_test.exe')],check=True)
