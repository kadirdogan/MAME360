#include <xtl.h>
#include <xmedia2.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include "xbox_Mame.h"
#include "xbox_FileIO.h"
#include "main.h"
#include "RomList.h"

extern "C" {
#include "osd_cpu.h"
#include "driver.h"
#include "mame.h"
}

using namespace std;

extern const struct GameDriver *drivers[];


extern void DrawDriverProgressData( const char *fileName, DWORD index, DWORD total );
 
char szRoms[MAX_PATH]; 
char szRomPath[MAX_PATH];
 
extern IDirect3DDevice9 *pDevice;
extern CMameLauncherApp app;
extern char pshader[255];

char InGamePreview[MAX_PATH];

int nLastRom = 0;
int nLastFilter = 0;
int HideChildren = 0;
int ThreeOrFourPlayerOnly = 0;

wchar_t DeviceText[60];
extern bool exitGame;

std::vector<std::string> m_ListData;
 
std::map<std::string, int> mapType;
std::map<std::string, int> mapRoms;

std::string ReplaceCharInString(const std::string & source, char charToReplace, const std::string replaceString);

void UpdateConsole(char *text);

static int CurrentFilter = 0;
char rom_filename[256];


wchar_t ucString[512];

HXUIOBJ hRomListScene;
HXUIOBJ hOptionsScene = NULL;


// ROM short names are persistent IDs; labels and list indices are never IDs.
static vector<int> s_availableDrivers, s_visibleDrivers;
static set<string> s_favorites;
static bool s_favoritesOnly = false, s_favoritesLoaded = false;
static bool s_favoritesWritable = true;

static void FavoriteStatus(const wchar_t *text)
{
    HXUIOBJ label = NULL;
    if (hRomListScene) {
        XuiElementGetChildById(hRomListScene, L"XuiNumberOfRoms", &label);
        if (label) XuiTextElementSetText(label, text);
    }
}

static void LoadFavorites()
{
    if (s_favoritesLoaded) return;
    s_favoritesLoaded = true;
    HANDLE file = CreateFileA("GAME:\\favorites.txt", GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        s_favoritesWritable = error == ERROR_FILE_NOT_FOUND;
        if (!s_favoritesWritable) logerror("[FAVORITES] Read open failed: %lu\n", error);
        return;
    }
    LARGE_INTEGER size;
    DWORD count = 0;
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 1048576) {
        s_favoritesWritable = false;
    } else {
        vector<char> bytes((size_t)size.QuadPart + 1, 0);
        if (!ReadFile(file, &bytes[0], (DWORD)size.QuadPart, &count, NULL)
            || count != (DWORD)size.QuadPart) s_favoritesWritable = false;
        else {
            istringstream lines(string(&bytes[0], count));
            string name;
            while (getline(lines, name)) {
                if (!name.empty() && name[name.size()-1] == '\r') name.erase(name.size()-1);
                if (name.empty()) continue;
                if (name.size() > 63 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != string::npos) {
                    s_favoritesWritable = false;
                    break;
                }
                s_favorites.insert(name);
            }
        }
    }
    CloseHandle(file);
    if (!s_favoritesWritable) logerror("[FAVORITES] Invalid or unreadable favorites.txt; changes disabled\n");
}

static bool SaveFavorites(const set<string>& names)
{
    if (!s_favoritesWritable) return false;
    string data;
    for (set<string>::const_iterator i = names.begin(); i != names.end(); ++i) data += *i + "\r\n";
    HANDLE file = CreateFileA("GAME:\\favorites.tmp", GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        logerror("[FAVORITES] Write open failed: %lu\n", GetLastError());
        return false;
    }
    DWORD written = 0;
    bool ok = WriteFile(file, data.c_str(), (DWORD)data.size(), &written, NULL) != 0;
    ok = ok && written == data.size();
    if (ok) ok = FlushFileBuffers(file) != 0;
    DWORD error = ok ? 0 : GetLastError();
    if (!CloseHandle(file)) { ok = false; error = GetLastError(); }
    if (ok && !MoveFileExA("GAME:\\favorites.tmp", "GAME:\\favorites.txt", MOVEFILE_REPLACE_EXISTING)) {
        ok = false;
        error = GetLastError();
    }
    if (!ok) logerror("[FAVORITES] Write/replace failed: %lu\n", error);
    return ok;
}

static bool DriverLess(int a, int b)
{
    int order = strcmp(drivers[a]->description, drivers[b]->description);
    return order ? order < 0 : strcmp(drivers[a]->name, drivers[b]->name) < 0;
}

void CRomList::RebuildVisible(int selectedDriver, int fallback)
{
    DeleteItems(0, RomCount);
    RomCount = 0;
    s_visibleDrivers.clear();
    m_vecAvailRomList.clear();
    int selected = -1, favoriteCount = 0;
    for (size_t i = 0; i < s_availableDrivers.size(); ++i) {
        int driver = s_availableDrivers[i];
        bool favorite = s_favorites.count(drivers[driver]->name) != 0;
        if (favorite) ++favoriteCount;
        if (s_favoritesOnly && !favorite) continue;
        if (driver == selectedDriver) selected = (int)s_visibleDrivers.size();
        s_visibleDrivers.push_back(driver);
        m_vecAvailRomList.push_back(string(favorite ? "[*] " : "") + drivers[driver]->description);
    }
    RomCount = (int)s_visibleDrivers.size();
    if (RomCount) {
        InsertItems(0, RomCount);
        if (selected < 0) selected = max(0, min(fallback, RomCount - 1));
        XuiListSetCurSel(m_hObj, selected);
        XuiListSetCurSelVisible(m_hObj, selected);
    }
    wchar_t status[128];
    swprintf_s(status, L"%ls:%d Fav:%d | LB+Y:+/- X:Filter",
        s_favoritesOnly ? L"Favorites" : L"All", RomCount, favoriteCount);
    FavoriteStatus(status);
    if (!s_favoritesWritable) FavoriteStatus(L"favorites.txt error; changes disabled");
}

HRESULT CRomList::OnKeyDown(XUIMessageInput* input, BOOL& bHandled)
{
    if (input->dwKeyCode != VK_PAD_X && input->dwKeyCode != VK_PAD_Y) {
        bHandled = FALSE;
        return S_OK;
    }
    bHandled = TRUE;
    if (input->dwFlags & XUI_INPUT_FLAG_REPEAT) return S_OK;
    /* Leave plain Y to the skin's existing Options shortcut. */
    if (input->dwKeyCode == VK_PAD_Y) {
        XINPUT_STATE pad;
        if (input->UserIndex >= 4 ||
            XInputGetState(input->UserIndex, &pad) != ERROR_SUCCESS ||
            !(pad.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)) {
            bHandled = FALSE;
            return S_OK;
        }
    }
    int index = XuiListGetCurSel(m_hObj, NULL);
    int driver = index >= 0 && index < (int)s_visibleDrivers.size() ? s_visibleDrivers[index] : -1;
    if (input->dwKeyCode == VK_PAD_X) {
        s_favoritesOnly = !s_favoritesOnly;
    } else {
        if (driver < 0) return S_OK;
        set<string> updated = s_favorites;
        string name = drivers[driver]->name;
        if (updated.count(name)) updated.erase(name); else updated.insert(name);
        if (!SaveFavorites(updated)) {
            FavoriteStatus(L"Favorite save failed: see debug.log");
            return S_OK;
        }
        s_favorites.swap(updated);
        logerror("[FAVORITES] %s: %s\n", s_favorites.count(name) ? "Added" : "Removed", name.c_str());
    }
    RebuildVisible(driver, index);
    return S_OK;
}

LPCWSTR MultiCharToUniChar(char* mbString)
{
	ucString[0] = 0;
	mbstowcs(ucString, mbString, 511);
	ucString[511] = 0;
	return (LPCWSTR)ucString;
}

const char *GetFilename (char *path, const char *InFileName, char *fext)
{
    static char filename [MAX_PATH + 1];
    char dir [_MAX_DIR + 1];
    char drive [_MAX_DRIVE + 1];
    char fname [42];
    char ext [_MAX_EXT + 1];
   _splitpath (InFileName, drive, dir, fname, ext);

   std::string fatxfname(fname);
	if (fatxfname.length() > 37)
	{
			fatxfname = fatxfname.substr(0,36);
	}

   _snprintf(filename, sizeof(filename),  "%s\\%s.%s",path, 
              fatxfname.c_str(), fext);
    return (filename);
}

void InRescanRomsFirstFunc(XUIMessage *pMsg, InRescanRomsStruct* pData, char *szPath)
{
    pData->szPath = szPath;
    XuiMessage(pMsg,XM_MESSAGE_ON_RESCAN_ROMS);
    _XuiMessageExtra(pMsg,(XUIMessageData*) pData, sizeof(*pData));

	
}


// Handler for the XM_NOTIFY message
HRESULT CRomListScene::OnNotifyPress( HXUIOBJ hObjPressed, 
       BOOL& bHandled )
    {
 
		char rom[255];
		wchar_t LabelText[255];
		char shaderName[42];

		int nIndex = 0;
		int drvIdx = 0;

        if ( hObjPressed == m_RomList )
        {
			nIndex = m_RomList.GetCurSel();
				
            if (nIndex < 0 || nIndex >= (int)s_visibleDrivers.size()) {
                bHandled = TRUE;
                return S_OK;
            }
            drvIdx = s_visibleDrivers[nIndex];
            nLastRom = nIndex;

			main_mame(drvIdx);
			
			XuiRenderRestoreState(app.GetDC());
			bHandled = TRUE;
			return S_OK;
  
        }
		else if (hObjPressed == m_Options)
		{

			XuiSceneCreate( L"file://game:/media/mame.xzp#Skin\\", L"Options.xur", NULL, &hOptionsScene );
			this->NavigateForward(hOptionsScene);

			bHandled = TRUE;
			return S_OK;
		}
	 
	
        bHandled = TRUE;
        return S_OK;
    }


    //----------------------------------------------------------------------------------
    // Performs initialization tasks - retreives controls.
    //----------------------------------------------------------------------------------
HRESULT CRomListScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {

		wchar_t LabelText[255];
		char shaderName[42];
 
        // Retrieve controls for later use.
        GetChildById( L"XuiAddToFavorites", &m_AddToFavorites );
        GetChildById( L"XuiImage1", &m_SkinImage );
        GetChildById( L"XuiMainMenu", &m_Back );		 
		GetChildById( L"XuiRomList", &m_RomList );
		GetChildById( L"XuiRomPreview", &m_PreviewImage );	
		GetChildById( L"XuiRomTitle", &m_TitleImage );			 
		GetChildById( L"XuiBackVideoRoms", &m_BackVideo );
		GetChildById( L"XuiCurrentDeviceText", &m_DeviceText);
		GetChildById( L"XuiOptions", &m_ConsoleOutput);

		GetChildById( L"XuiGameInfo", &m_GameInfo);
		GetChildById( L"XuiRomName", &m_RomName);
		GetChildById( L"XuiROMInfo", &m_RomInfo);
		GetChildById( L"XuiReleasedBy", &m_ReleasedBy);
		GetChildById( L"XuiGenre", &m_Genre);

		GetChildById( L"XuiVersion", &m_Version);


		GetChildById( L"XuiButtonOptions", &m_Options);
		GetChildById( L"XuiShaderName", &m_ShaderName);
		
 
		 

		//m_RomList.DiscardResources(XUI_DISCARD_ALL);
		m_RomList.SetFocus();
		m_RomList.SetCurSel(0);
 
		swprintf_s(DeviceText, L"Current Hardware : All Games", szRomPath);
		m_DeviceText.SetText(DeviceText);
 
		hRomListScene = this->m_hObj;
 
		swprintf_s(LabelText,L"%S", LVERSION_STRING);

		m_ListData.clear();
		m_Version.SetText(L"SS2.1+Fav");

		CurrentFilter = nLastFilter;
		XuiImageElementSetImagePath(m_PreviewImage.m_hObj, L"");

		XUIMessage xuiMsg;
		InRescanRomsStruct msgData;
		InRescanRomsFirstFunc( &xuiMsg, &msgData, "GAME:\\ROMS\\");
		XuiSendMessage( m_RomList.m_hObj, &xuiMsg );
 
		
		_splitpath(pshader,NULL,NULL,shaderName,NULL);
		swprintf_s(LabelText,L"%S", shaderName);
		m_ShaderName.SetText(LabelText);

		m_RomList.SetShow(true);
		m_RomList.SetFocus();		
		if (!s_visibleDrivers.empty()) {
            nLastRom = min(nLastRom, (int)s_visibleDrivers.size() - 1);
            m_RomList.SetCurSel(nLastRom);
            m_RomList.SetCurSelVisible(nLastRom);
        }

		bHandled = TRUE;
        return S_OK;
    }

CRomList::CRomList() : RomCount(0), RomListOK(false)
{
}
int CRomList::InitRomList()
{

	RomCount = 0;
	RomListOK = false;

	m_vecAvailRomList.clear();	 

	return 0;
} 

int CRomList::FreeRomList()
{
	m_vecAvailRomList.clear();
	 
	return 0;
}

int CRomList::AvRoms()
{

	RomCount = m_vecAvailRomList.size();
	 
	return 0;
}

HRESULT CRomList::OnNotify( XUINotify *hObj, BOOL& bHandled )
{	
	wchar_t previewPath[MAX_PATH];
	wchar_t titlePath[MAX_PATH];
	wchar_t InfoText[255];

	char PreviewFName[MAX_PATH];
	char TitleFName[MAX_PATH];

	char rom[255];

	HXUIOBJ hTitleImage;

	HXUIOBJ hGameInfoText;
	HXUIOBJ hRomNameText;
	HXUIOBJ hRomInfoText;
	HXUIOBJ hReleasedText;
	HXUIOBJ hGenreText;
	HXUIOBJ hParentText;

	int nIndex = 0;
	int drvIdx = 0;
	switch(hObj->dwNotify)
	{
		case XN_SELCHANGED:			 						 
			nIndex = XuiListGetCurSel( this->m_hObj, NULL ); 
            if (nIndex < 0 || nIndex >= (int)s_visibleDrivers.size()) return S_OK;
            drvIdx = s_visibleDrivers[nIndex];

			strcpy(TitleFName, GetFilename((char *)g_FileIOConfig.m_screenshotPath.c_str(),  drivers[drvIdx]->name , "png"));			 
 
			XuiElementGetChildById( hRomListScene, 
                L"XuiRomTitle", &hTitleImage );

			XuiElementGetChildById( hRomListScene, 
                L"XuiGameInfo", &hGameInfoText );

			XuiElementGetChildById( hRomListScene, 
                L"XuiRomName", &hRomNameText );

			XuiElementGetChildById( hRomListScene, 
                L"XuiROMInfo", &hRomInfoText );

			XuiElementGetChildById( hRomListScene, 
                L"XuiReleasedBy", &hReleasedText );


			XuiElementGetChildById( hRomListScene, 
                L"XuiGenre", &hGenreText );

			XuiElementGetChildById( hRomListScene, 
                L"XuiParent", &hParentText );

			swprintf_s(InfoText,L"%S", drivers[drvIdx]->manufacturer );
			XuiTextElementSetText(hReleasedText,InfoText);
 
			swprintf_s(InfoText,L"%S", drivers[drvIdx]->description );
			XuiTextElementSetText(hRomNameText,InfoText);
 
			swprintf_s(InfoText,L"%S", drivers[drvIdx]->year );
			XuiTextElementSetText(hGameInfoText,InfoText);

			swprintf_s(InfoText,L"%S", drivers[drvIdx]->name );
			XuiTextElementSetText(hParentText,InfoText);
			
 
			
			if (GetFileAttributes(TitleFName) != -1)
			{
				string titleName(TitleFName);

				titleName = ReplaceCharInString(titleName, '\\',"/");
				swprintf_s(titlePath,L"file://%S", titleName.c_str());
				XuiImageElementSetImagePath(hTitleImage, titlePath);


				swprintf_s(titlePath,L"file://%S", ReplaceCharInString(g_FileIOConfig.m_screenshotPath, '\\', "/").c_str());
				//XuiElementDiscardResources(hTitleImage, XUI_DISCARD_ALL);
				XuiElementSetBasePath(hTitleImage, titlePath);
 
			}
			else
			{		
				XuiImageElementSetImagePath(hTitleImage, L"no_title.png");
			}
	}

	return S_OK;

}
 

HRESULT CRomList::OnRescanRoms( char *szPath,  BOOL& bHandled )
{ 
	HXUIOBJ hRomCountText; 
	wchar_t RomCountText[60];

	bool IsFiltered = false;
	std::vector<std::string> vecTempRomList;
	std::vector<std::string> vecAvailRomListFileName;
	std::vector<std::string> vecAvailRomList;
	std::vector<int>		 vecAvailRomIndex;

	DeleteItems(0, RomCount);
	InitRomList();
	s_visibleDrivers.clear();
	s_availableDrivers.clear();
  
	m_vecAvailRomList.clear();

	DrawDriverProgressData("",0,0);

	if (m_ListData.empty())
	{

		for (int d = 0; d < 4; d++) 
		{

			HANDLE hFind;	
			WIN32_FIND_DATAA oFindData;			 

			switch (d)
			{
				case 0:
					strcpy((char *)szRoms,  g_FileIOConfig.m_romPath0.c_str() );
					break;
				case 1:
					strcpy((char *)szRoms,  g_FileIOConfig.m_romPath1.c_str() );
					break;
				case 2:
					strcpy((char *)szRoms,  g_FileIOConfig.m_romPath2.c_str() );
					break;
				case 3:
					strcpy((char *)szRoms,  g_FileIOConfig.m_romPath3.c_str() );
					break;
			}
			
			strcat(szRoms, "\\*.*");

			hFind = FindFirstFile(szRoms, &oFindData);

			if (hFind != INVALID_HANDLE_VALUE)
			{
				do
				{
					std::vector<std::string>::iterator iter = std::find(m_ListData.begin(), m_ListData.end(), _strlwr(oFindData.cFileName));

					if (iter == m_ListData.end()) {
						m_ListData.push_back(_strlwr(oFindData.cFileName));		
					}
				
				} while (FindNextFile(hFind, &oFindData));
                FindClose(hFind);
			}
		}
		  
	}
 
	InitRomList();

	mapType.clear();
 
	// Count and sort the game drivers (Taken from XMame)
    DWORD totalMAMEGames = 0;
    for( totalMAMEGames = 0; drivers[totalMAMEGames]; ++totalMAMEGames)
	{
		char fullname[60];
		sprintf(fullname,"%s.zip",drivers[totalMAMEGames]->name);
		vecAvailRomListFileName.push_back(fullname);
		vecAvailRomList.push_back(drivers[totalMAMEGames]->name);
		vecAvailRomIndex.push_back(totalMAMEGames);

		mapType[fullname] = totalMAMEGames;
		mapRoms[drivers[totalMAMEGames]->description] = totalMAMEGames;

	}

 
 
	for (unsigned int x = 0; x < vecAvailRomListFileName.size(); x++) {
		for (unsigned int y = 0; y < m_ListData.size() ; y++) {
			if (m_ListData[y] == vecAvailRomListFileName[x]) {
 				 
				s_availableDrivers.push_back(x);
				//m_vecAvailRomReleasedBy.push_back(drivers[x]->manufacturer);						 
				//m_vecAvailRomManufacturer.push_back(drivers[x]->year);
							
			}

		}


	}

    LoadFavorites();
    std::sort(s_availableDrivers.begin(), s_availableDrivers.end(), DriverLess);
    RebuildVisible(-1, nLastRom);

	bHandled = TRUE;	
    return( S_OK );
}

HRESULT CRomList::OnInit(XUIMessageInit *pInitData, BOOL& bHandled)
{
 
	XUIMessage xuiMsg;
	InRescanRomsStruct msgData;
	InRescanRomsFirstFunc( &xuiMsg, &msgData, "GAME:\\ROMS\\" );
	XuiSendMessage( m_hObj, &xuiMsg );

	bHandled = TRUE;
    return S_OK;
}

HRESULT CRomList::OnGetItemCountAll(
        XUIMessageGetItemCount *pGetItemCountData, 
        BOOL& bHandled)
    {
        pGetItemCountData->cItems = RomCount;
        bHandled = TRUE;
        return S_OK;
    }


// Gets called every frame
HRESULT CRomList::OnGetSourceDataText(
    XUIMessageGetSourceText *pGetSourceTextData, 
    BOOL& bHandled)
{
	
    
    if( ( 0 == pGetSourceTextData->iData ) && ( ( pGetSourceTextData->bItemData ) ) ) {

			LPCWSTR lpszwBuffer = L"";
            if (pGetSourceTextData->iItem >= 0 && pGetSourceTextData->iItem < (int)m_vecAvailRomList.size())
                lpszwBuffer = MultiCharToUniChar((char *)m_vecAvailRomList[pGetSourceTextData->iItem].c_str());

            pGetSourceTextData->szText = lpszwBuffer;

            bHandled = TRUE;
        }
        return S_OK;

}



std::string ReplaceCharInString(  
    const std::string & source, 
    char charToReplace, 
    const std::string replaceString 
    ) 
{ 
    std::string result; 
 
    // For each character in source string: 
    const char * pch = source.c_str(); 
    while ( *pch != '\0' ) 
    { 
        // Found character to be replaced? 
        if ( *pch == charToReplace ) 
        { 
            result += replaceString; 
        } 
        else 
        { 
            // Just copy original character 
            result += (*pch); 
        } 
 
        // Move to next character 
        ++pch; 
    } 
 
    return result; 
} 

