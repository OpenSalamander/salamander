// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

SYSTEMTIME LastCheckTime;       // kdy byla naposledy provedena kontrola
SYSTEMTIME NextOpenOrCheckTime; // kdy nejdrive se ma automaticky otevrit okno pluginu a prip. provest kontrola
int ErrorsSinceLastCheck = 0;   // kolikrat jsme se uz neuspesne pokouseli automaticky provest kontrolu

//****************************************************************************
//
// Deklarace / Definice
//

// CModuleInfo

const char* SCRIPT_SIGNATURE_BEGIN = "SALAMANDER_VERINFO_BEGIN"; // signature at begin of file
const char* SCRIPT_SIGNATURE_EOF = "SALAMANDER_VERINFO_EOF";     // signature at end of file

BYTE LoadedScript[LOADED_SCRIPT_MAX];
DWORD LoadedScriptSize = 0;

class CModules;

class CSalModuleInfo // moduly vracene z CSalamanderGeneral::EnumInstalledModules
{
public:
    CSalModuleInfo();
    ~CSalModuleInfo();

    BOOL SetModule(const char* module);   // pres DupStr si vytvori kopii
    BOOL SetVersion(const char* version); // pres DupStr si vytvori kopii

    DWORD GetResVer() { return ResolvedVersion; }
    BOOL GetResBeta() { return ResolvedBeta; }
    DWORD GetSpecBld() { return ResolvedSpecialBuild; }

protected:
    char* Module;  // salamand.exe
    char* Version; // 2.0 beta 1

private:
    // nastaveno v metode SetVersion()
    DWORD ResolvedVersion; // celociselna verze slozena z "2.00" nebo "2.00 beta 2" nebo "2.00 beta 2c"
    BOOL ResolvedBeta;
    DWORD ResolvedSpecialBuild; // 0 = nejde o IB/PB/DB/CB verzi; jinak je zde jeji cislo (vzdy > 0)

    friend class CModules;
};

enum CModuleTypeEnum
{
    mteNewRelease,
    mteNewBeta,
    mteNewPB,
    mteFiltered,
    mteInstalled,
};

class CModuleInfo : public CSalModuleInfo
{
public:
    CModuleInfo();
    ~CModuleInfo();

    void AddToLog(int index);

protected:
    BOOL Beta; // FALSE = release version; TRUE = beta version
    // alokovane retezce
    char* Name;                // Open Salamander
    char* Url;                 // www.altap.cz
    TDirectArray<char*> Infos; // urcity pocet radku popisujicich modul

    // pouze vnitrni flag pro CompareWithInstalledModulesAndLogIt
    CModuleTypeEnum Type;

    BOOL ShowDetails; // jsou zobrazeny detaily k tomuto modulu?

    friend class CModules;
};

enum CModulesSate
{
    msEmpty,         // cerstve inicializovano
    msCorruptedData, // data jsou porusena nebo maji chybny format
    msCorrectData    // data jsou OK
};

// drzak CModuleInfo, ktery dokaze nacist script
class CModules
{
public:
    CModules();
    ~CModules();

    void Cleanup(); // provede destrukci a inicializaci drzenych dat

    BOOL BuildDataFromScript();             // sestavi data z LoadedScript
    void FillLogWindow(BOOL rereadModules); // vysmazi do logu soucasny stav
    void EnumSalModules();                  // omezeni - musi byt volana z hlavniho threadu Salamandera

    void ClearModuleWasFound() { ModuleWasFound = FALSE; }
    BOOL GetModuleWasFound() { return ModuleWasFound; }

    void ChangeShowDetails(int index);

    BOOL HasCorrectData() { return State == msCorrectData; }

protected:
    BOOL LoadDataFromScript(TDirectArray<char*>* lines);
    void CompareWithInstalledModulesAndLogIt(BOOL rereadModules); // provna pole Modules s tim, co nam vraci Salamander

protected:
    TIndirectArray<CModuleInfo> Modules;
    TIndirectArray<CSalModuleInfo> SalModules;
    CModulesSate State;
    BOOL ModuleWasFound; // alespon jeden novy modul byl nalezen
};

//****************************************************************************
//
// Globals
//

CDataDefaults DataDefaults[inetCount] =
    {
        {achmMonth, FALSE, FALSE, TRUE, FALSE /* v PB verzich je sem dosazeno TRUE, viz LoadConfig */, TRUE},
        {achmWeek, TRUE, TRUE, TRUE, FALSE /* v PB verzich je sem dosazeno TRUE, viz LoadConfig */, TRUE},
        {achmNever, FALSE, FALSE, TRUE, FALSE /* v PB verzich je sem dosazeno TRUE, viz LoadConfig */, TRUE},
};

CInternetConnection InternetConnection = inetLAN;
CInternetProtocol InternetProtocol = inetpHTTP;
CDataDefaults Data = DataDefaults[InternetConnection];
TDirectArray<char*> Filters(10, 10); // citelna forma nazvu polozek (vcetne verze nebo bez verze (v tom pripade jde o vsechny verze)), ktere se nemaji checkovat
CModules Modules;

const char* CONFIG_AUTOCHECKMODE = "AutoCheckMode";
const char* CONFIG_AUTOCONNECT = "AutoConnect";
const char* CONFIG_AUTOCLOSE = "AutoClose";
const char* CONFIG_CHECKBETA = "CheckBetaVersions";
const char* CONFIG_CHECKPB = "CheckPBVersions";
const char* CONFIG_ISCFGOFPB = "CfgOfPB";
const char* CONFIG_CHECKRELEASE = "CheckReleaseVersions";
const char* CONFIG_CONNECTION = "IneternetConnection";
const char* CONFIG_PROTOCOL = "InternetProtocol";
const char* CONFIG_FILTER_KEY = "Filters";
const char* CONFIG_TIMESTAMP_KEY = "TimeStamp.hidden";
const char* CONFIG_LASTCHECK = "LastOpen";
const char* CONFIG_NEXTOPEN = "NextOpen";
const char* CONFIG_NUMOFERRORS = "NumOfErrors";

// ConfigVersion: 0 - zadna konfigurace se z Registry nenacetla (jde o instalaci pluginu),
//                1 - konfigurace pred zavedenim promenne 'Version' do registry
//                2 - Salamander 2.52: forcujeme nove default hodnoty (vsem nastavime defaulty pro inetLAN)
//                3 - Salamander 3.0: forcujeme nove default hodnoty (ukazeme vsechny nove verze, pridany link "ignore this version", snad uz nebudou frontalne vypinat CheckVer)
//                4 - Salamander 4.0: forcujeme nove default hodnoty

int ConfigVersion = 0;             // verze nactene konfigurace z registry (popis verzi viz vyse)
#define CURRENT_CONFIG_VERSION 4   // aktualni verze konfigurace (uklada se do registry pri unloadu pluginu)
#define LOAD_ONLY_CONFIG_VERSION 4 // starsi verze konfigurace se neloadi, bereme defaulty (FORCE DEFAULTS)
const char* CONFIG_VERSION = "Version";

//****************************************************************************
//
// Filters
//

void DestroyFilters()
{
    int i;
    for (i = 0; i < Filters.Count; i++)
        free(Filters[i]);
    Filters.DestroyMembers();
}

BOOL AddUniqueFilter(const char* itemName)
{
    int i;
    for (i = 0; i < Filters.Count; i++)
    {
        const char* item = Filters[i];
        if (SalGeneral->StrICmp(item, itemName) == 0)
            return TRUE; // polozka uz je v seznamu
    }
    char* newItem = SalGeneral->DupStr(itemName);
    if (newItem == NULL)
        return FALSE;
    Filters.Add(newItem);
    if (!Filters.IsGood())
    {
        Filters.ResetState();
        free(newItem);
        return FALSE;
    }
    else
        return TRUE;
}

BOOL FiltersContains(const char* itemName, const char* itemVersion)
{
    int itemNameLen = (int)strlen(itemName);
    int i;
    for (i = 0; i < Filters.Count; i++)
    {
        const char* item = Filters[i];
        const char* itemVer = strchr(item, '|');
        if (itemVer != NULL)
        {
            if (SalGeneral->StrICmpEx(item, (int)(itemVer - item), itemName, itemNameLen) == 0 &&
                SalGeneral->StrICmp(itemVer + 1, itemVersion) == 0)
            {
                return TRUE;
            }
        }
        else
        {
            if (SalGeneral->StrICmp(item, itemName) == 0)
                return TRUE;
        }
    }
    return FALSE;
}

void FiltersFillListBox(HWND hListBox)
{
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    char item[1024];
    int i;
    for (i = 0; i < Filters.Count; i++)
    {
        lstrcpyn(item, Filters[i], 1024);
        char* itemVer = strchr(item, '|');
        if (itemVer != NULL)
            *itemVer = ' ';
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)item);
        SendMessage(hListBox, LB_SETITEMDATA, i, itemVer == NULL ? -1 : itemVer - item);
    }
    if (i > 0)
        SendMessage(hListBox, LB_SETCURSEL, 0, 0);
}

void FiltersLoadFromListBox(HWND hListBox)
{
    DestroyFilters();
    int count = (int)SendMessage(hListBox, LB_GETCOUNT, 0, 0);
    char buff[1024];
    int i;
    for (i = 0; i < count; i++)
    {
        if (SendMessage(hListBox, LB_GETTEXT, i, (LPARAM)buff) != LB_ERR)
        {
            char* itemVer = buff + SendMessage(hListBox, LB_GETITEMDATA, i, 0);
            if (itemVer >= buff && itemVer < buff + strlen(buff))
                *itemVer = '|';
            AddUniqueFilter(buff);
        }
    }
}

//****************************************************************************
//
// Configuration Load/Save
//

unsigned __int64
GetDaysCount(const SYSTEMTIME* time)
{
    FILETIME ft;
    if (SystemTimeToFileTime(time, &ft))
    {
        unsigned __int64 t = ft.dwLowDateTime + (((unsigned __int64)ft.dwHighDateTime) << 32);
        return t / ((unsigned __int64)10000000 * 60 * 60 * 24);
    }
    else
        return 0;
}

DWORD
GetWaitDays()
{
    DWORD days = 0;
    switch (Data.AutoCheckMode)
    {
    case achmDay:
        days = 1;
        break;
    case achmWeek:
        days = 7;
        break;
    case achmMonth:
        days = 30;
        break;
    case achm3Month:
        days = 3 * 30;
        break;
    case achm6Month:
        days = 6 * 30;
        break;
    }
    return days;
}

BOOL IsTimeExpired(const SYSTEMTIME* time)
{
    if (GetWaitDays() == 0) // teoreticky hrozi jen pro achmNever
        return FALSE;
    SYSTEMTIME currentTime;
    GetLocalTime(&currentTime);
    unsigned __int64 currentDays = GetDaysCount(&currentTime);
    unsigned __int64 days = GetDaysCount(time);
    return days <= currentDays;
}

void GetFutureTime(SYSTEMTIME* tgtTime, const SYSTEMTIME* time, DWORD days)
{
    FILETIME ft;
    if (SystemTimeToFileTime(time, &ft))
    {
        unsigned __int64 t = ft.dwLowDateTime + (((unsigned __int64)ft.dwHighDateTime) << 32);
        t += days * ((unsigned __int64)10000000 * 60 * 60 * 24);
        ft.dwLowDateTime = (DWORD)(t & 0xffffffff);
        ft.dwHighDateTime = (DWORD)(t >> 32);
        if (FileTimeToSystemTime(&ft, tgtTime))
            return;
    }
    GetLocalTime(tgtTime); // pokud 'time' obsahuje nesmysl, bereme aktualni cas (provedeme otevreni prip. s kontrolou co nejdrive)
}

void GetFutureTime(SYSTEMTIME* tgtTime, DWORD days)
{
    SYSTEMTIME currentTime;
    GetLocalTime(&currentTime);
    GetFutureTime(tgtTime, &currentTime, days);
}

BOOL IsPBVersion(const char* ver)
{ // zjistujeme jestli je bezici verze Salamandera preview build (napr. "2.53 beta 1 (PB 38)" nebo "3.0 beta 1 (PB75 x86)")
    while (*ver != 0)
    {
        if (SalGeneral->StrNICmp(ver, " (PB", 4) == 0)
            return TRUE;
        ver++;
    }
    return FALSE;
}

void LoadConfig(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("LoadConfig(, ,)");
    if (regKey != NULL && !registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        ConfigVersion = 1; // konfigurace pred zavedenim promenne 'Version' do registry
    BOOL curVerIsPB = IsPBVersion(SalamanderTextVersion);
    if (curVerIsPB) // v PB verzi je defaultni testovani PB verzi
    {
        int i;
        for (i = 0; i < inetCount; i++)
            DataDefaults[i].CheckPBVersion = TRUE;
    }
    if (regKey != NULL && ConfigVersion >= LOAD_ONLY_CONFIG_VERSION) // load z registry, jinak "force defaults"...
    {
        registry->GetValue(regKey, CONFIG_AUTOCHECKMODE, REG_DWORD, &Data.AutoCheckMode, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_AUTOCONNECT, REG_DWORD, &Data.AutoConnect, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_AUTOCLOSE, REG_DWORD, &Data.AutoClose, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CHECKBETA, REG_DWORD, &Data.CheckBetaVersion, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CHECKPB, REG_DWORD, &Data.CheckPBVersion, sizeof(DWORD));
        BOOL cfgIsOfPB;
        if (curVerIsPB &&
            (!registry->GetValue(regKey, CONFIG_ISCFGOFPB, REG_DWORD, &cfgIsOfPB, sizeof(DWORD)) || !cfgIsOfPB))
        {
            Data.CheckPBVersion = TRUE; // automaticky zapiname overovani PB verzi pri prechodu na PB verzi
        }
        registry->GetValue(regKey, CONFIG_CHECKRELEASE, REG_DWORD, &Data.CheckReleaseVersion, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_CONNECTION, REG_DWORD, &InternetConnection, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_PROTOCOL, REG_DWORD, &InternetProtocol, sizeof(DWORD));
        HKEY actKey;
        if (registry->OpenKey(regKey, CONFIG_FILTER_KEY, actKey))
        {
            DestroyFilters();
            char name[10];
            char buffer[1024];
            int i;
            for (i = 0; TRUE; i++)
            {
                sprintf(name, "%d", i + 1);
                if (registry->GetValue(actKey, name, REG_SZ, buffer, 1024))
                    AddUniqueFilter(buffer);
                else
                    break;
            }
            registry->CloseKey(actKey);
        }

        BOOL timeStampLoaded = FALSE;
        if (registry->OpenKey(regKey, CONFIG_TIMESTAMP_KEY, actKey))
        {
            if (registry->GetValue(actKey, CONFIG_LASTCHECK, REG_BINARY, &LastCheckTime, sizeof(LastCheckTime)) &&
                registry->GetValue(actKey, CONFIG_NEXTOPEN, REG_BINARY, &NextOpenOrCheckTime, sizeof(NextOpenOrCheckTime)) &&
                registry->GetValue(actKey, CONFIG_NUMOFERRORS, REG_DWORD, &ErrorsSinceLastCheck, sizeof(DWORD)))
            {
                //        TRACE_I("Loaded: LastCheckTime: " << LastCheckTime.wDay << "." << LastCheckTime.wMonth << "." << LastCheckTime.wYear);
                //        TRACE_I("Loaded: NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
                //        TRACE_I("Loaded: ErrorsSinceLastCheck: " << ErrorsSinceLastCheck);
                timeStampLoaded = TRUE;
            }
            registry->CloseKey(actKey);
        }

        if (LoadedOnSalamanderStart && timeStampLoaded)
        {
            // porovname cas v registry s casem aktualnim
            if (IsTimeExpired(&NextOpenOrCheckTime))
                SalGeneral->PostMenuExtCommand(CM_AUTOCHECK_VERSION, TRUE); // a pokud cas vyprsel, vyvola automaticke otevreni hlavniho okna CheckVer
            else
                SalGeneral->PostUnloadThisPlugin(); // nechame plugin unloadnout - uz nebude potreba
        }
    }
    else
    {
        // default values
        InternetConnection = inetLAN;
        Data = DataDefaults[InternetConnection];
        DestroyFilters();

        if (LoadedOnSalInstall)
        {
            SalGeneral->PostMenuExtCommand(CM_FIRSTCHECK_VERSION, TRUE); // vyvolame automaticke otevreni hlavniho okna CheckVer
            LoadedOnSalInstall = FALSE;
        }
    }
}

void OnSaveTimeStamp(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("OnSaveTimeStamp(, ,)");
    HKEY actKey;
    if (registry->CreateKey(regKey, CONFIG_TIMESTAMP_KEY, actKey))
    {
        //    TRACE_I("Storing LastCheckTime: " << LastCheckTime.wDay << "." << LastCheckTime.wMonth << "." << LastCheckTime.wYear);
        //    TRACE_I("Storing NextOpenOrCheckTime: " << NextOpenOrCheckTime.wDay << "." << NextOpenOrCheckTime.wMonth << "." << NextOpenOrCheckTime.wYear);
        //    TRACE_I("Storing ErrorsSinceLastCheck: " << ErrorsSinceLastCheck);
        registry->SetValue(actKey, CONFIG_LASTCHECK, REG_BINARY, &LastCheckTime, sizeof(LastCheckTime));
        registry->SetValue(actKey, CONFIG_NEXTOPEN, REG_BINARY, &NextOpenOrCheckTime, sizeof(NextOpenOrCheckTime));
        registry->SetValue(actKey, CONFIG_NUMOFERRORS, REG_DWORD, &ErrorsSinceLastCheck, sizeof(DWORD));
        registry->CloseKey(actKey);
    }
}

void SaveConfig(HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("SaveConfig(, ,)");
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_AUTOCHECKMODE, REG_DWORD, &Data.AutoCheckMode, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_AUTOCONNECT, REG_DWORD, &Data.AutoConnect, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_AUTOCLOSE, REG_DWORD, &Data.AutoClose, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CHECKBETA, REG_DWORD, &Data.CheckBetaVersion, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CHECKPB, REG_DWORD, &Data.CheckPBVersion, sizeof(DWORD));
    BOOL curVerIsPB = IsPBVersion(SalamanderTextVersion);
    registry->SetValue(regKey, CONFIG_ISCFGOFPB, REG_DWORD, &curVerIsPB, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CHECKRELEASE, REG_DWORD, &Data.CheckReleaseVersion, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_CONNECTION, REG_DWORD, &InternetConnection, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_PROTOCOL, REG_DWORD, &InternetProtocol, sizeof(DWORD));
    HKEY actKey;
    if (registry->CreateKey(regKey, CONFIG_FILTER_KEY, actKey))
    {
        registry->ClearKey(actKey);
        char buf[10];
        int i;
        for (i = 0; i < Filters.Count; i++)
        {
            sprintf(buf, "%d", i + 1);
            registry->SetValue(actKey, buf, REG_SZ, Filters[i], -1);
        }
        registry->CloseKey(actKey);
    }

    // load-on-start
    SalGeneral->SetFlagLoadOnSalamanderStart(Data.AutoCheckMode != achmNever);
}

//****************************************************************************
//
// CSalModuleInfo
//

CSalModuleInfo::CSalModuleInfo()
{
    Module = NULL;
    Version = NULL;
    ResolvedVersion = 0;
    ResolvedBeta = FALSE;
    ResolvedSpecialBuild = 0;
}

CSalModuleInfo::~CSalModuleInfo()
{
    if (Module != NULL)
    {
        free(Module);
        Module = NULL;
    }

    if (Version != NULL)
    {
        free(Version);
        Version = NULL;
    }
}

BOOL CSalModuleInfo::SetModule(const char* module)
{
    if (Module != NULL)
    {
        free(Module);
        Module = NULL;
    }
    if (module != NULL)
    {
        Module = SalGeneral->DupStr(module);
        if (Module == NULL)
        {
            TRACE_E("Low memory");
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CSalModuleInfo::SetVersion(const char* version)
{
    ResolvedVersion = 0;
    ResolvedBeta = FALSE;
    ResolvedSpecialBuild = 0;
    if (Version != NULL)
    {
        free(Version);
        Version = NULL;
    }
    if (version != NULL)
    {
        Version = SalGeneral->DupStr(version);
        if (Version == NULL)
        {
            TRACE_E("Low memory");
            return FALSE;
        }
    }
    if (Version != NULL)
    {
        // napocitame promenne ResolvedVersion, ResolvedBeta a ResolvedSpecialBuild
        char buff[100];
        lstrcpyn(buff, Version, 100);
        _strlwr(buff);
        char* p = buff;
        while (*p != 0 && *p != '(')
            p++;
        if (*p == '(' && (strncmp(p + 1, "ib", 2) == 0 || strncmp(p + 1, "db", 2) == 0 ||
                          strncmp(p + 1, "pb", 2) == 0 || strncmp(p + 1, "cb", 2) == 0))
        {
            char* num = p + 3;
            if (*num == ' ')
                num++;
            if (p > buff && *(p - 1) == ' ')
                p--;
            *p = 0;
            while (*num >= '0' && *num <= '9')
                ResolvedSpecialBuild = 10 * ResolvedSpecialBuild + (*num++ - '0');
            if (ResolvedSpecialBuild == 0)
                ResolvedSpecialBuild = 1; // nula to proste byt nemuze (tohle by nemelo byt nikdy potreba, ciste paranoidni test)
        }
        p = strstr(buff, "beta");
        BOOL beta = p != NULL;
        ResolvedBeta = beta;
        char partL[100]; // cast verze pred "beta"
        char partR[100]; // cast verze za "beta"
        if (!beta)
        {
            lstrcpyn(partL, buff, 100);
            partR[0] = 0;
        }
        else
        {
            char* lp = p - 1;
            while (lp > buff && *lp == ' ')
                lp--;
            lstrcpyn(partL, buff, (int)(lp - buff + 2));

            p += 4;
            while (*p != 0 && *p == ' ')
                p++;
            lstrcpyn(partR, p, 100);
            // pokud jde o tvar "6b", prevedeme ho na "6.1"

            int len = lstrlen(partR);
            if (len > 0)
            {
                int ch = partR[len - 1];
                if (ch >= 'a' && ch <= 'z')
                    sprintf(partR + len - 1, ".%d", ch - 'a' + 1);
            }
        }
        // slozime verzi
        // chyba jeste v AS2.51, bez pricteni 0.5 vychazely "nahodne" hodnoty, protoze
        // WinSCP pri nacteni do procesu prenastavi zaokrouhlovani CPU (plati pro vlakno)
        ResolvedVersion = ((WORD)(atof(partL) * 1000 + 0.5)) << 16;
        if (partR[0] != 0)
            ResolvedVersion |= ((WORD)(atof(partR) * 100 + 0.5));
    }
    return TRUE;
}

//****************************************************************************
//
// CModuleInfo
//

CModuleInfo::CModuleInfo()
    : Infos(10, 10)
{
    Beta = FALSE;
    Name = NULL;
    Url = NULL;
    Type = mteNewRelease; // jen predchazim neinicializovane promenne
    ShowDetails = FALSE;
}

CModuleInfo::~CModuleInfo()
{
    if (Name != NULL)
    {
        free(Name);
        Name = NULL;
    }

    if (Url != NULL)
    {
        free(Url);
        Url = NULL;
    }

    int i;
    for (i = 0; i < Infos.Count; i++)
        free(Infos[i]);
    Infos.DetachMembers();
}

void CModuleInfo::AddToLog(int index)
{
    char line[1024];
    sprintf(line, "   \tu%s %s\tl%s\ti%s|%s\tn       \tu%s\tlDETAILS\ti%d\tn | \tu%s\tlFILTER\ti%s|%s\tn",
            Name, Version, Url, Name, Version, LoadStr(ShowDetails ? IDS_HIDEDETAILS : IDS_SHOWDETAILS),
            index, LoadStr(IDS_FILTERTHISVER), Name, Version);
    AddLogLine(line, FALSE);
    if (ShowDetails)
    {
        int j;
        for (j = 0; j < Infos.Count; j++)
        {
            sprintf(line, "      %s", Infos[j]);
            AddLogLine(line, FALSE);
        }
    }
}

//****************************************************************************
//
// CModules
//

CModules::CModules()
    : Modules(50, 50, dtDelete), SalModules(50, 50, dtDelete)
{
    State = msEmpty;
    ModuleWasFound = FALSE;
}

CModules::~CModules()
{
}

void CModules::Cleanup()
{
    Modules.DestroyMembers();
    SalModules.DestroyMembers();
    State = msEmpty;
    ModuleWasFound = FALSE;
}

BOOL CModules::LoadDataFromScript(TDirectArray<char*>* lines)
{
    CALL_STACK_MESSAGE1("CModules::LoadDataFromScript()");
    int count = lines->Count;
    if (count == 0)
    {
        TRACE_E("SCRIPT has count of lines = 0");
        return FALSE;
    }

    // obe signatury musi byt pritomne
    if (lstrcmp(lines->At(0), SCRIPT_SIGNATURE_BEGIN) != 0)
    {
        TRACE_E("SCRIPT_SIGNATURE_BEGIN not found");
        return FALSE;
    }

    if (lstrcmp(lines->At(count - 1), SCRIPT_SIGNATURE_EOF) != 0)
    {
        TRACE_E("SCRIPT_SIGNATURE_EOF not found");
        return FALSE;
    }

    Modules.DestroyMembers();

    // dekodujeme jednotlive radky
    const char* module = NULL;
    const char* name = NULL;
    const char* version = NULL;
    const char* url = NULL;

    enum CReadMode
    {
        rmModule,
        rmName,
        rmVersion,
        rmUrl,
        rmInfo,
    };

    CReadMode rm = rmModule;
    BYTE moduleType;
    int counter;

#define MODULE_TYPE_RELEASE 0x01;
#define MODULE_TYPE_BETA 0x02;

    int index;
    for (index = 1; index < count - 1; index++)
    {
        const char* line = lines->At(index);
        BOOL empty = (*line == 0);
        switch (rm)
        {
        case rmModule:
        {
            if (empty)
            {
                TRACE_E("MODULE not found at line " << index + 1);
                return FALSE;
            }
            moduleType = 0;
            counter = 0;
            module = line;
            name = NULL;
            version = NULL;
            url = NULL;
            rm = rmName;
            break;
        }

        case rmName:
        {
            if (empty)
            {
                TRACE_E("NAME not found at line " << index + 1);
                return FALSE;
            }
            name = line;
            rm = rmVersion;
            break;
        }

        case rmVersion:
        {
            if (!empty)
            {
                if (counter == 0)
                {
                    moduleType |= MODULE_TYPE_RELEASE;
                }
                else
                {
                    if (counter == 1)
                    {
                        moduleType |= MODULE_TYPE_BETA;
                    }
                    else
                    {
                        TRACE_E("counter=" << counter << " on line " << index + 1);
                        return FALSE;
                    }
                }
                version = line;
                rm = rmUrl;
            }
            else
            {
                if (counter == 1)
                {
                    if (moduleType == 0)
                    {
                        // pokud nebyla ani release ani demo sekce naplnena, zarveme
                        TRACE_E("Release and Demo sections are not assigned on line " << index + 1);
                        return FALSE;
                    }
                    rm = rmModule;
                }
                counter++; // jsme v releasu - pouze prejdeme do dema
            }
            break;
        }

        case rmUrl:
        {
            if (empty)
            {
                TRACE_E("URL not found at line " << index + 1);
                return FALSE;
            }
            url = line;

            CModuleInfo* info = new CModuleInfo();
            if (info == NULL)
            {
                TRACE_E("Low memory");
                return FALSE;
            }
            info->Beta = counter == 1;
            info->SetModule(module);
            info->Name = SalGeneral->DupStr(name);
            info->SetVersion(version);
            info->Url = SalGeneral->DupStr(url);
            if (info->Module == NULL || info->Name == NULL ||
                info->Version == NULL || info->Url == NULL)
            {
                // zanedbam dealokace
                TRACE_E("Low memory");
                return FALSE;
            }

            const char* infoLine = "";
            do
            {
                if (index + 1 >= count - 1)
                    break;
                index++;
                infoLine = lines->At(index);
                if (*infoLine != 0)
                {
                    char* tmpInfoLine = SalGeneral->DupStr(infoLine);
                    if (tmpInfoLine != NULL)
                        info->Infos.Add(tmpInfoLine);
                }
            } while (*infoLine != 0);

            Modules.Add(info);
            if (!Modules.IsGood())
            {
                delete info;
                Modules.ResetState();
                return FALSE;
            }

            if (counter == 0)
                rm = rmVersion;
            else
                rm = rmModule;
            counter++;
            break;
        }
        }
    }

    if (rm != rmModule)
    {
        TRACE_E("Syntax error on line " << index /*+ 1*/);
        return FALSE;
    }

    return TRUE;
}

BOOL CModules::BuildDataFromScript()
{
    CALL_STACK_MESSAGE1("CModules::BuildDataFromScript()");

    State = msCorruptedData;

    TDirectArray<char*> lines(500, 500); // sem naleju obsah scripu

    BYTE* lineStart = LoadedScript;
    DWORD count = 0;
    while (count < LoadedScriptSize)
    {
        BYTE* lineEnd = lineStart;
        while (*lineEnd != '\r' && *lineEnd != '\n' && count < LoadedScriptSize)
        {
            lineEnd++;
            count++;
        }
        // upajcneme zprava mezery
        BYTE* tmpLineEnd = lineEnd - 1;
        while (tmpLineEnd > lineStart && *tmpLineEnd == ' ')
            tmpLineEnd--;
        if (tmpLineEnd != lineEnd - 1)
        {
            if (*tmpLineEnd != ' ')
                tmpLineEnd++;
        }
        else
        {
            tmpLineEnd++;
        }

        int lineLen = (int)(tmpLineEnd - lineStart);
        char* line = (char*)malloc(lineLen + 1);
        if (line != NULL)
            lines.Add(line);
        if (line == NULL || !lines.IsGood())
        {
            if (!lines.IsGood())
                lines.ResetState();
            else
                TRACE_E("Low memory");
            int i;
            for (i = 0; i < lines.Count; i++)
                free(lines[i]);
            return FALSE;
        }
        if (lineLen > 0)
            memmove(line, lineStart, lineLen);
        *(line + lineLen) = 0;

        if (lstrcmp(line, SCRIPT_SIGNATURE_EOF) == 0)
            break; // mame nacteno...

        if (count >= LoadedScriptSize - 2)
            break; // je cas vypadnout
        if (*lineEnd == '\r' && *(lineEnd + 1) == '\r' && *(lineEnd + 2) == '\n')
        { // '\r\r\n' - pres ftp predelane '\r\n'
            lineEnd += 3;
            count += 3;
        }
        else
        {
            if (*lineEnd == '\r' && *(lineEnd + 1) == '\n' ||
                *lineEnd == '\n' && *(lineEnd + 1) == '\r')
            {
                lineEnd += 2;
                count += 2;
            }
            else
            {
                lineEnd++;
                count++;
            }
        }
        lineStart = lineEnd;
    }

    BOOL ret = LoadDataFromScript(&lines);
    if (ret)
        State = msCorrectData; // data jsou OK

    int i;
    for (i = 0; i < lines.Count; i++)
        free(lines[i]);

    return ret;
}

BOOL LoadScripDataFromFile(const char* fileName)
{
    char buff[1024];
    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        sprintf(buff, LoadStr(IDS_FILE_OPENERROR), fileName);
        AddLogLine(buff, TRUE);
        return FALSE;
    }

    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0xFFFFFFFF || size == 0 || size > LOADED_SCRIPT_MAX)
    {
        sprintf(buff, LoadStr(IDS_FILE_OPENERROR), fileName);
        AddLogLine(buff, TRUE);
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD read;
    if (!ReadFile(hFile, LoadedScript, size, &read, NULL) || read != size)
    {
        sprintf(buff, LoadStr(IDS_FILE_READERROR), fileName);
        AddLogLine(buff, TRUE);
        CloseHandle(hFile);
        return FALSE;
    }
    CloseHandle(hFile);

    sprintf(buff, LoadStr(IDS_FILE_OPENED), fileName);
    AddLogLine(buff, FALSE);
    AddLogLine("", FALSE);

    LoadedScriptSize = read;
    return TRUE;
}

void ModulesCreateLog(BOOL* moduleWasFound, BOOL rereadModules)
{
    Modules.ClearModuleWasFound();
    if (rereadModules || !Modules.HasCorrectData())
        Modules.BuildDataFromScript();
    Modules.FillLogWindow(rereadModules);
    if (moduleWasFound != NULL)
        *moduleWasFound = Modules.GetModuleWasFound();
}

BOOL ModulesHasCorrectData()
{
    return Modules.HasCorrectData();
}

void ModulesCleanup()
{
    Modules.Cleanup();
}

void ModulesChangeShowDetails(int index)
{
    Modules.ChangeShowDetails(index);
}

void CModules::ChangeShowDetails(int index)
{
    if (index >= 0 && index < Modules.Count)
        Modules[index]->ShowDetails = !Modules[index]->ShowDetails;
}

void CModules::EnumSalModules()
{
    // omezeni - musi byt volana z hlavniho threadu Salamandera
    SalModules.DestroyMembers();
    int index = 0;
    char module[MAX_PATH];
    char version[MAX_PATH];
    while (SalGeneral->EnumInstalledModules(&index, module, version))
    {
        char* p = strrchr(module, '\\');
        if (p != NULL)
        {
            CSalModuleInfo* mdl = new CSalModuleInfo();
            mdl->SetModule(p + 1);
            mdl->SetVersion(version);
            if (mdl->Module == NULL || mdl->Version == NULL)
            {
                TRACE_E("Low memory");
                delete mdl;
                break;
            }
            else
            {
                SalModules.Add(mdl);
                if (!SalModules.IsGood())
                {
                    SalModules.ResetState();
                    delete mdl;
                    break;
                }
            }
        }
        else
            TRACE_E("p == NULL on index=" << index);
    }
}

void EnumSalModules()
{
    Modules.EnumSalModules();
}

void CModules::FillLogWindow(BOOL rereadModules)
{
    switch (State)
    {
    case msCorruptedData:
    {
        AddLogLine(LoadStr(IDS_INFO_CORRUPTED), TRUE);
        break;
    }

    case msCorrectData:
    {
        CompareWithInstalledModulesAndLogIt(rereadModules);
        break;
    }

    default:
    {
        TRACE_E("State = " << State);
        break;
    }
    }
}

void CModules::CompareWithInstalledModulesAndLogIt(BOOL rereadModules)
{
    // porovname proti sobe pole Modules a moduly, ktere nam vrati v iteraci Salamander

    if (rereadModules || SalModules.Count == 0)
    {
        // shodime event
        ResetEvent(HModulesEnumDone);
        // posteneme si command, aby k nam prijel hlavni thread Salamandera, ze ktereho
        // mzeme enumeraci provest
        SalGeneral->PostMenuExtCommand(CM_ENUMMODULES, FALSE);
        // pockame, az bude naenumerovano
        WaitForSingleObject(HModulesEnumDone, INFINITE);
    }

    int NewReleaseCount = 0;
    int NewBetaCount = 0;
    int NewPBCount = 0;
    int FilteredCount = 0;
    int InstalledCount = 0;

    // vsem modulum ze sciptu priradime Type
    int i;
    for (i = 0; i < Modules.Count; i++)
    {
        CModuleInfo* mdl = Modules[i];
        // patri do kategorieje filtovanych? bude user nechce videt informace o betach
        // a je to beta, nebo je v seznamu filrovanych pluginu
        BOOL filtered = FALSE;
        if (!Data.CheckReleaseVersion && !mdl->GetResBeta() && mdl->GetSpecBld() == 0 ||
            !Data.CheckPBVersion && mdl->GetSpecBld() != 0 ||
            !Data.CheckBetaVersion && mdl->GetResBeta() && mdl->GetSpecBld() == 0)
        {
            filtered = TRUE;
        }
        else
            filtered = FiltersContains(mdl->Name, mdl->Version);

        if (filtered)
        {
            mdl->Type = mteFiltered;
            FilteredCount++;
        }
        else
        {
            BOOL installed = FALSE;
            CSalModuleInfo* salMdl;
            int j;
            for (j = 0; j < SalModules.Count; j++)
            {
                // zjistime, jestli je modul instalovan
                salMdl = SalModules[j];
                if (SalGeneral->StrICmp(salMdl->Module, mdl->Module) == 0)
                {
                    installed = TRUE;
                    break;
                }
            }
            BOOL newModule = FALSE;
            if (installed && salMdl->GetResBeta() == mdl->GetResBeta())
            {
                // pokud je modul instalovany a zaroven je/neni beta stejne jako modul ze scriptu
                // porovnam jejich verze (pri shode verzi jeste muze rozhodnout jestli jde o IB/DB/PB/CB,
                // specialni build vzdy predchazi verzi bez specialniho buildu + mezi specialnimi buildy
                // rozhodne jejich cislo)
                if (salMdl->GetResVer() < mdl->GetResVer() ||
                    salMdl->GetResVer() == mdl->GetResVer() && salMdl->GetSpecBld() > 0 && mdl->GetSpecBld() == 0 || // shoda verzi + instalovany modul je special build a modul ze scriptu ne
                    salMdl->GetResVer() == mdl->GetResVer() && salMdl->GetSpecBld() > 0 && mdl->GetSpecBld() > 0 &&  // shoda verzi + oba moduly jsou special build + modul ze scriptu je vyssi special build
                        salMdl->GetSpecBld() < mdl->GetSpecBld())
                {
                    // mame instalovany modul s nizssi verzi nez je ve scriptu
                    newModule = TRUE;
                }
                else
                {
                    // mame instalovany novejsi nebo stejny
                    mdl->Type = mteInstalled;
                    InstalledCount++;
                }
            }
            else
            {
                // modul neni instalovany nebo je instalovany ale lisi se beta/nebeta (v tomto pripade special build porovnavani neovlivni)
                if (!installed ||
                    salMdl->GetResBeta() && (salMdl->GetResVer() & 0xffff0000) <= mdl->GetResVer() || // Salamander je beta, tedy zajimaji nas verze stejne nebo vyssi (2.52 beta 2 se upgradne na 2.52, 2.53, atd.)
                    !salMdl->GetResBeta() && salMdl->GetResVer() < (mdl->GetResVer() & 0xffff0000))   // Salamander neni beta, tedy zajimaji nas jen verze (orizle o cislo beta verze) vyssi (2.52 se upgradne na 2.53 beta 1, 2.54 beta 1, atd.)
                {
                    newModule = TRUE;
                }
                else
                {
                    // mame instalovany novejsi nebo stejny
                    mdl->Type = mteInstalled;
                    InstalledCount++;
                }
            }
            if (newModule)
            {
                if (mdl->GetSpecBld() != 0)
                {
                    mdl->Type = mteNewPB;
                    NewPBCount++;
                }
                else
                {
                    if (mdl->GetResBeta())
                    {
                        mdl->Type = mteNewBeta;
                        NewBetaCount++;
                    }
                    else
                    {
                        mdl->Type = mteNewRelease;
                        NewReleaseCount++;
                    }
                }
            }
        }
    }

    if (NewReleaseCount + NewBetaCount + NewPBCount + FilteredCount + InstalledCount != Modules.Count)
        TRACE_E("CModules::CompareWithInstalledModulesAndLogIt(): unexpected situation");

    char buff[1024];
    // posleme to do logu
    if (NewReleaseCount != 0)
    {
        AddLogLine(LoadStr(IDS_NEWREL_MODULES), FALSE);
        int j;
        for (j = 0; j < Modules.Count; j++)
        {
            CModuleInfo* mdl = Modules[j];
            if (mdl->Type == mteNewRelease)
                mdl->AddToLog(j);
        }
        AddLogLine("", FALSE);
    }
    if (NewBetaCount != 0)
    {
        AddLogLine(LoadStr(IDS_NEWBETA_MODULES), FALSE);
        int j;
        for (j = 0; j < Modules.Count; j++)
        {
            CModuleInfo* mdl = Modules[j];
            if (mdl->Type == mteNewBeta)
                mdl->AddToLog(j);
        }
        AddLogLine("", FALSE);
    }
    if (NewPBCount != 0)
    {
        AddLogLine(LoadStr(IDS_NEWPB_MODULES), FALSE);
        int j;
        for (j = 0; j < Modules.Count; j++)
        {
            CModuleInfo* mdl = Modules[j];
            if (mdl->Type == mteNewPB)
                mdl->AddToLog(j);
        }
        AddLogLine("", FALSE);
    }
    ModuleWasFound = NewPBCount + NewBetaCount + NewReleaseCount > 0;
    if (NewPBCount + NewBetaCount + NewReleaseCount == 0)
    {
        sprintf(buff, "%s", LoadStr(IDS_NONEW_MODULES));
        AddLogLine(buff, FALSE);
        AddLogLine("", FALSE);
    }
    if (NewPBCount + NewBetaCount + NewReleaseCount != 0)
    {
        AddLogLine(LoadStr(IDS_LOGWNDHELP1), FALSE);
        AddLogLine(LoadStr(IDS_LOGWNDHELP2), FALSE);
        AddLogLine("", FALSE);
    }
    if (FilteredCount != 0)
    {
        AddLogLine(LoadStr(IDS_FILTERED_MODULES), FALSE);
        int j;
        for (j = 0; j < Modules.Count; j++)
        {
            CModuleInfo* mdl = Modules[j];
            if (mdl->Type == mteFiltered)
            {
                sprintf(buff, "   %s %s", mdl->Name, mdl->Version);
                AddLogLine(buff, FALSE);
            }
        }
        AddLogLine("", FALSE);
    }
    if (InstalledCount != 0)
    {
        AddLogLine(LoadStr(IDS_INSTALLED_MODULES), FALSE);
        int j;
        for (j = 0; j < Modules.Count; j++)
        {
            CModuleInfo* mdl = Modules[j];
            if (mdl->Type == mteInstalled)
            {
                sprintf(buff, "   %s %s", mdl->Name, mdl->Version);
                AddLogLine(buff, FALSE);
            }
        }
        AddLogLine("", FALSE);
    }
}
