"""Host checks against the actual patched C code; not an Xbox emulator test."""
from pathlib import Path
import subprocess
import shutil
import sys

base = Path(__file__).resolve().parent
repo = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else base.parent
src = repo / 'mame_0.72/src'
out = base / 'generated'
out.mkdir(exist_ok=True)

driver = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef struct { unsigned char data[32768]; unsigned size; } mame_file;
struct GameDriver { const char *name; } game_driver = {"shinobi"};
struct RunningMachine { struct GameDriver *gamedrv; int sample_rate; } machine = {&game_driver, 44100};
struct RunningMachine *Machine = &machine;
static int allocation_failure = -1, short_read, short_write;
static void *test_malloc(size_t size) {
    if (allocation_failure == 0) return NULL;
    if (allocation_failure > 0) allocation_failure--;
    return malloc(size);
}
void logerror(const char *fmt, ...) { (void)fmt; }
void usrintf_showmessage(const char *fmt, ...) { (void)fmt; }
unsigned mame_fsize(mame_file *f) { return f->size; }
unsigned mame_fread(mame_file *f, void *data, unsigned size) {
    unsigned count = size < f->size ? size : f->size;
    if (short_read && count) count--;
    memcpy(data, f->data, count);
    return count;
}
unsigned mame_fwrite(mame_file *f, const void *data, unsigned size) {
    unsigned count = short_write ? size - 1 : size;
    assert(count <= sizeof(f->data));
    memcpy(f->data, data, count);
    f->size = count;
    return count;
}
#define malloc test_malloc
'''
(out/'driver.h').write_text(driver)
(out/'core_state.c').write_bytes((src/'state.c').read_bytes())
(out/'legacy_state.c').write_bytes(subprocess.check_output(['git','-C',str(repo),'show','63a21cfce35d42333885031cf2fe221808c44016:mame_0.72/src/state.c']))

harness = r'''
#ifdef LEGACY
#include "legacy_state.c"
#else
#include "core_state.c"
#endif
#undef malloc
static int presaves, postloads;
static void before(void) { presaves++; }
static void after(void) { postloads++; }
static void save_tags(void) {
    state_save_set_current_tag(0); state_save_save_continue();
    state_save_set_current_tag(1); state_save_save_continue();
}
static void load_tags(void) {
    state_save_set_current_tag(0); state_save_load_continue();
    state_save_set_current_tag(1); state_save_load_continue();
    state_save_load_finish();
}
int main(int argc, char **argv) {
    mame_file f = {{0},0}, valid, corrupt;
    UINT8 ram[8] = {1,2,3,4,5,6,7,8};
    UINT16 words[2] = {0x1234,0xabcd};
    int score = 12345;
    double volume = 0.25;
    FILE *disk;
    state_save_set_current_tag(0);
    state_save_register_UINT8("test",0,"ram",ram,8);
    state_save_register_int("test",0,"score",&score);
    state_save_register_double("test",0,"volume",&volume,1);
    state_save_register_func_presave(before);
    state_save_register_func_postload(after);
    state_save_set_current_tag(1);
    state_save_register_UINT16("cpu",0,"words",words,2);
#ifdef LEGACY
    state_save_save_begin(&f); save_tags(); state_save_save_finish();
#else
    assert(!state_save_save_begin(&f)); save_tags(); assert(!state_save_save_finish());
#endif
    assert(presaves == 1);
    assert(!memcmp(f.data,"MAMESAVE",8) && f.data[8] == 1);
    assert(argc == 2);
    disk=fopen(argv[1],"wb"); assert(disk);
    assert(fwrite(f.data,1,f.size,disk) == f.size); fclose(disk);
#ifndef LEGACY
    valid=f;
    memset(ram,0,sizeof(ram)); score=0; volume=0; words[0]=words[1]=0;
    assert(!state_save_load_begin(&f)); load_tags();
    assert(ram[0]==1 && ram[7]==8 && score==12345 && volume==0.25);
    assert(words[0]==0x1234 && words[1]==0xabcd && postloads==1);
    puts("PASS: registered data, CPU tags, pre-save/post-load callbacks round trip");
    score=777;
    corrupt=valid; corrupt.size=0; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.size=7; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.size--; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.size++; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.data[0]='!'; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.data[8]=2; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.data[10]='!'; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.data[20]^=1; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.data[9]|=0x80; assert(state_save_load_begin(&corrupt));
    corrupt=valid; corrupt.data[9]^=1; assert(state_save_load_begin(&corrupt));
    short_read=1; assert(state_save_load_begin(&valid)); short_read=0;
    assert(score==777 && postloads==1);
    puts("PASS: invalid sizes/header/version/ROM/signature/flags/sound and short reads rejected before mutation");
    allocation_failure=0; assert(state_save_save_begin(&f));
    assert(state_save_load_begin(&valid));
    allocation_failure=1; assert(state_save_load_begin(&valid));
    allocation_failure=-1;
    assert(!state_save_save_begin(&f)); save_tags();
    allocation_failure=0; assert(state_save_save_finish());
    allocation_failure=-1;
    assert(!state_save_save_begin(&f)); save_tags();
    short_write=1; assert(state_save_save_finish()); short_write=0;
    assert(!state_save_load_begin(&valid)); load_tags();
    puts("PASS: allocation failures, short writes, and recovery after failure");
    game_driver.name="12345678901"; assert(state_save_save_begin(&f));
    game_driver.name="1234567890";
    assert(!state_save_save_begin(&f)); save_tags(); assert(!state_save_save_finish());
    assert(!state_save_load_begin(&f)); load_tags();
    puts("PASS: legacy 10-byte ROM-name boundary");
#endif
    state_save_reset();
    return 0;
}
'''
(out/'state_test.c').write_text(harness)
gcc=shutil.which('gcc')
assert gcc, 'gcc required for host tests'
args=[gcc,'-std=gnu99','-DLSB_FIRST','-Wno-pointer-to-int-cast',f'-I{repo / "zlib"}',str(out/'state_test.c'),str(repo/'zlib/crc32.c')]
subprocess.run(args+['-o',str(out/'state_test.exe')],check=True)
subprocess.run([str(out/'state_test.exe'),str(out/'new.sta')],check=True)
subprocess.run(args+['-DLEGACY','-o',str(out/'legacy_test.exe')],check=True)
subprocess.run([str(out/'legacy_test.exe'),str(out/'old.sta')],check=True)
assert (out/'new.sta').read_bytes()==(out/'old.sta').read_bytes()
print('PASS: byte-for-byte identical save file to unmodified MAME72 serializer',flush=True)

poll=(src/'xbox/xbox_JoystickMouse.c').read_text().split('int osd_savestate_action(int reset)')[1]
edge=r'''
#include <assert.h>
#include <stdio.h>
typedef unsigned short WORD;
typedef struct { struct { WORD wButtons; } Gamepad; } XINPUT_STATE;
#define ERROR_SUCCESS 0
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x200
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000
#define LOADSAVE_NONE 0
#define LOADSAVE_SAVE 1
#define LOADSAVE_LOAD 2
static WORD allbuttons[4];
static int offline[4];
#define buttons allbuttons[0]
#define disconnected offline[0]
static int XInputGetState(int pad, XINPUT_STATE *s) { assert(pad>=0 && pad<4); s->Gamepad.wButtons=allbuttons[pad]; return offline[pad]; }
'''+ '\nint osd_savestate_action(int reset)' + poll + r'''
int main(void) {
    int i;
    WORD shoulders=XINPUT_GAMEPAD_LEFT_SHOULDER|XINPUT_GAMEPAD_RIGHT_SHOULDER;
    buttons=shoulders|XINPUT_GAMEPAD_Y;
    assert(!osd_savestate_action(1)); assert(!osd_savestate_action(0));
    buttons=0; assert(!osd_savestate_action(0));
    buttons=shoulders|XINPUT_GAMEPAD_Y; assert(osd_savestate_action(0)==LOADSAVE_SAVE);
    for(i=0;i<300;i++) assert(!osd_savestate_action(0));
    buttons=shoulders; assert(!osd_savestate_action(0));
    buttons|=XINPUT_GAMEPAD_X; assert(osd_savestate_action(0)==LOADSAVE_LOAD);
    for(i=0;i<300;i++) assert(!osd_savestate_action(0));
    buttons=0; assert(!osd_savestate_action(0));
    buttons=shoulders|XINPUT_GAMEPAD_X|XINPUT_GAMEPAD_Y; assert(!osd_savestate_action(0));
    buttons=shoulders|XINPUT_GAMEPAD_Y; assert(!osd_savestate_action(0));
    disconnected=1; assert(!osd_savestate_action(0));
    disconnected=0; assert(!osd_savestate_action(0));
    buttons=0; assert(!osd_savestate_action(0));
    buttons=shoulders|XINPUT_GAMEPAD_X; assert(osd_savestate_action(0)==LOADSAVE_LOAD);
    assert(!osd_savestate_action(1)); assert(!osd_savestate_action(0));
    buttons=XINPUT_GAMEPAD_Y; assert(!osd_savestate_action(0));
    buttons=shoulders|XINPUT_GAMEPAD_Y; assert(osd_savestate_action(0)==LOADSAVE_SAVE);
    buttons=0; assert(!osd_savestate_action(0));
    for(i=1;i<4;i++) {
        allbuttons[i]=shoulders|XINPUT_GAMEPAD_Y;
        assert(osd_savestate_action(0)==LOADSAVE_SAVE);
        assert(!osd_savestate_action(0));
        allbuttons[i]=0; assert(!osd_savestate_action(0));
    }
    allbuttons[1]=shoulders|XINPUT_GAMEPAD_Y;
    allbuttons[2]=shoulders|XINPUT_GAMEPAD_X;
    assert(osd_savestate_action(0)==LOADSAVE_SAVE);
    assert(!osd_savestate_action(0));
    puts("PASS: four controllers, simultaneous edges, chord edges, 300-frame holds, rearm, both buttons, disconnect, new game/reset");
    return 0;
}
'''
(out/'edge_test.c').write_text(edge)
subprocess.run([gcc,'-std=c89','-Wall','-Wextra',str(out/'edge_test.c'),'-o',str(out/'edge_test.exe')],check=True)
subprocess.run([str(out/'edge_test.exe')],check=True)

# Execute the real Xbox state-write branch and recursive directory helper
# with fault-injected Win32 calls; this does not replace an XDK build.
fileio=(src/'xbox/xbox_FileIO.cpp').read_text()
writefn=fileio[fileio.index('UINT32 osd_fwrite('):fileio.index('//\tosd_fclose')]
mkdirfn=fileio[fileio.index('void Helper_CreateDirectoryPath( const char *path, BOOL has_filename )'):fileio.index('//\tComposeFilePath')]
io=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned int DWORD;
typedef unsigned int UINT32;
typedef int BOOL;
#define FILE_BUFFER_SIZE 256
#define TRUE 1
#define FALSE 0
#define PRINTMSG(x)
typedef struct {
    int m_handle, m_isState;
    unsigned m_bufferbytes, m_writeBufferBytes;
    unsigned m_filepos, m_offset, m_end;
    unsigned char m_writeBuffer[256];
} osd_file;
static int fail_write, fail_flush, short_write, flushes, directories;
static char created[3][100];
static void logerror(const char *fmt, ...) { (void)fmt; }
static unsigned GetLastError(void) { return 5; }
static void osd_fflush(osd_file *file) { (void)file; }
static int WriteFile(int h, const void *p, unsigned n, DWORD *written, void *overlap) {
    (void)h; (void)p; (void)overlap;
    *written=fail_write?0:(short_write?n-1:n); return !fail_write;
}
static int FlushFileBuffers(int h) { (void)h; flushes++; return !fail_flush; }
static DWORD GetFileAttributes(const char *p) {
    int i;
    if (!strcmp(p,"GAME:")) return 0x10;
    for(i=0;i<directories;i++) if(!strcmp(p,created[i])) return 0x10;
    return (DWORD)-1;
}
static int CreateDirectory(const char *p, void *unused) {
    (void)unused; assert(directories<3); strcpy(created[directories++],p); return 1;
}
'''+writefn+mkdirfn+r'''
int main(void) {
    osd_file f={0}; char path[]="GAME:\\sta\\shinobi\\0.sta";
    char bytes[4]={0};
    Helper_CreateDirectoryPath(path,TRUE);
    assert(directories==2 && !strcmp(created[0],"GAME:\\sta") && !strcmp(created[1],"GAME:\\sta\\shinobi"));
    assert(!strcmp(path,"GAME:\\sta\\shinobi\\0.sta"));
    Helper_CreateDirectoryPath(path,TRUE); assert(directories==2);
    f.m_isState=1;
    assert(osd_fwrite(&f,bytes,4)==4 && flushes==1 && f.m_writeBufferBytes==0);
    assert(f.m_offset==4 && f.m_end==4);
    fail_write=1; assert(osd_fwrite(&f,bytes,4)==0); fail_write=0;
    fail_flush=1; assert(osd_fwrite(&f,bytes,4)==0); fail_flush=0;
    short_write=1; assert(osd_fwrite(&f,bytes,4)==3); short_write=0;
    puts("PASS: recursive state directories, unbuffered state writes, flush/write failure and short-write reporting");
    return 0;
}
'''
(out/'io_test.c').write_text(io)
subprocess.run([gcc,'-std=c99','-Wall','-Wextra',str(out/'io_test.c'),'-o',str(out/'io_test.exe')],check=True)
subprocess.run([str(out/'io_test.exe')],check=True)
