#include <Windows.h>
#include <iostream>
#include "MsiSdk/MACMSharedMemory.h"
#include "MsiSdk/MAHMSharedMemory.h"

HANDLE hMapCM, hMapHM;
LPVOID pAddrCM, pAddrHM;

const UINT WM_MACM_CMD_NOTIFICATION = ::RegisterWindowMessage(L"MACMCmdNotification");

void DisconnectCM()
{
    if (pAddrCM)
    {
        UnmapViewOfFile(pAddrCM);
        pAddrCM = NULL;
    }

    if (hMapCM)
    {
        CloseHandle(hMapCM);
        hMapCM = NULL;
    }
}

bool ConnectCM()
{
	if(!hMapCM)
	{
        hMapCM = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "MACMSharedMemory");

        if (!hMapCM)
        {
            std::cout << "MACMSharedMemory baglanilamadi! Hata Kodu: " << GetLastError() << std::endl;
            return false;
        }

        pAddrCM = MapViewOfFile(hMapCM, FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (!pAddrCM)
        {
            std::cout << "MapViewOfFile! Hata Kodu: " << GetLastError() << std::endl;
            return false;
        }

        LPMACM_SHARED_MEMORY_HEADER lpHeader = (LPMACM_SHARED_MEMORY_HEADER)pAddrCM;

        if(lpHeader->dwSignature != 'MACM')
        {
            DisconnectCM();
            return false;
        }
	}

    return true;
}



bool CheckConnectionCM()
{
    if (!pAddrCM)
        return false;

    LPMACM_SHARED_MEMORY_HEADER lpHeader = (LPMACM_SHARED_MEMORY_HEADER)pAddrCM;

    if (lpHeader->dwSignature == 0xDEAD)
        return false;

    return true;
}


void DisconnectHM()
{
    if (pAddrHM)
    {
        UnmapViewOfFile(pAddrHM);
        pAddrHM = NULL;
    }

    if (hMapHM)
    {
        CloseHandle(hMapHM);
        hMapHM = NULL;
    }
}

bool ConnectHM()
{
    if (!hMapHM)
    {
        hMapHM = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "MAHMSharedMemory");

        if (!hMapHM)
        {
            std::cout << "MAHMSharedMemory baglanilamadi! Hata Kodu: " << GetLastError() << std::endl;
            return false;
        }

        pAddrHM = MapViewOfFile(hMapHM, FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (!pAddrHM)
        {
            std::cout << "MapViewOfFile! Hata Kodu: " << GetLastError() << std::endl;
            return false;
        }

        LPMAHM_SHARED_MEMORY_HEADER lpHeader = (LPMAHM_SHARED_MEMORY_HEADER)pAddrHM;

        if (lpHeader->dwSignature != 'MAHM')
        {
            DisconnectHM();
            return false;
        }
    }

    return true;
}


bool CheckConnectionHM()
{
    if (!pAddrHM)
        return false;

    LPMAHM_SHARED_MEMORY_HEADER lpHeader = (LPMAHM_SHARED_MEMORY_HEADER)pAddrHM;

    if (lpHeader->dwSignature == 0xDEAD)
        return false;

    return true;
}

LPMAHM_SHARED_MEMORY_ENTRY FoundEntry(LPVOID pAddr, DWORD dwIndex)
{
    LPMAHM_SHARED_MEMORY_HEADER lpHeaderHM = (LPMAHM_SHARED_MEMORY_HEADER)pAddr;

    for (DWORD dwSource = 0; dwSource < lpHeaderHM->dwNumEntries; dwSource++)
    {
        LPMAHM_SHARED_MEMORY_ENTRY	lpEntry = (LPMAHM_SHARED_MEMORY_ENTRY)((LPBYTE)lpHeaderHM + lpHeaderHM->dwHeaderSize + dwSource * lpHeaderHM->dwEntrySize);

        if (lpEntry->dwGpu != dwIndex)
            continue;

        if (lpEntry->dwSrcId != MONITORING_SOURCE_ID_MEMORY_CLOCK)
            continue;

        return lpEntry;
    }

    return NULL;
}

int main()
{
    std::cout << "Msi Afterburner'a baglaniliyor..." << std::endl;

    while (!ConnectCM())
        Sleep(100);

    while (!ConnectHM())
        Sleep(100);

    LPMACM_SHARED_MEMORY_HEADER lpHeaderCM = (LPMACM_SHARED_MEMORY_HEADER)pAddrCM;
    LPMAHM_SHARED_MEMORY_HEADER lpHeaderHM = (LPMAHM_SHARED_MEMORY_HEADER)pAddrHM;

    std::cout << "Versiyon: " << "0x" << std::hex << lpHeaderHM->dwVersion << std::endl;

    if (lpHeaderHM->dwVersion < 0x20000)
    {
        std::cout << "Versiyon 0x20000 ve uzeri olmalidir." << std::endl;
        int dwExit;
        std::cin >> dwExit;
        DisconnectCM();
        DisconnectHM();
        return dwExit;
    }

    std::cout << "Ekran Kartlari Listeleniyor" << std::endl;

    for(DWORD i = 0; i < lpHeaderHM->dwNumGpuEntries; i++)
    {
        LPMAHM_SHARED_MEMORY_GPU_ENTRY	lpGpuEntry = (LPMAHM_SHARED_MEMORY_GPU_ENTRY)((LPBYTE)lpHeaderHM + lpHeaderHM->dwHeaderSize + lpHeaderHM->dwNumEntries * lpHeaderHM->dwEntrySize + i * lpHeaderHM->dwGpuEntrySize);
        std::cout << i + 1 << "- " << lpGpuEntry->szDevice << std::endl;
    }

    TEKRAR:
    std::cout << "Lutfen Ekran Karti Seciniz: ";
    std::cin >> lpHeaderCM->dwMasterGpu;

    if(lpHeaderCM->dwMasterGpu < 1  || lpHeaderCM->dwMasterGpu > lpHeaderCM->dwNumGpuEntries)
    {
        std::cout << "Hatali Secim" << std::endl;
        goto TEKRAR;
    }

    lpHeaderCM->dwMasterGpu--;
    LPMAHM_SHARED_MEMORY_ENTRY lpGpuMemoryEntry = FoundEntry(pAddrHM, lpHeaderCM->dwMasterGpu);

    if(!lpGpuMemoryEntry)
    {
        std::cout << "Bellek Yapisi Bulunamadi!" << std::endl;
        int dwExit;
        std::cin >> dwExit;
        DisconnectCM();
        DisconnectHM();
        return dwExit;
    }

    std::cout << "Ayarlar kaydediliyor" << std::endl;

	LPMACM_SHARED_MEMORY_GPU_ENTRY	lpGpuEntry = (LPMACM_SHARED_MEMORY_GPU_ENTRY)((LPBYTE)lpHeaderCM + lpHeaderCM->dwHeaderSize + lpHeaderCM->dwMasterGpu * lpHeaderCM->dwGpuEntrySize);
    LPMACM_SHARED_MEMORY_GPU_ENTRY	lpGpuEntrySaved = new MACM_SHARED_MEMORY_GPU_ENTRY;
    memcpy(lpGpuEntrySaved, lpGpuEntry, sizeof(MACM_SHARED_MEMORY_GPU_ENTRY));

    std::cout << "Ayarlar kaydedildi" << std::endl;

    float dwLimitMemoryClock;
    std::cout << "Memory Clock Limitini Giriniz(Mhz): ";
    std::cin >> dwLimitMemoryClock;
    std::cout << "Memory Clock " << dwLimitMemoryClock << " Mhz degerinin altina dustugunda ayarlar yeniden yapilandirilacak" << std::endl;

    while(true)
    {
        if(!CheckConnectionCM())
        {
            DisconnectCM();

            while (!ConnectCM())
                Sleep(100);

            lpGpuEntry = (LPMACM_SHARED_MEMORY_GPU_ENTRY)((LPBYTE)lpHeaderCM + lpHeaderCM->dwHeaderSize + lpHeaderCM->dwMasterGpu * lpHeaderCM->dwGpuEntrySize);
        }

        if (!CheckConnectionHM())
        {
            DisconnectHM();

            while (!ConnectHM())
                Sleep(100);

            lpGpuMemoryEntry = FoundEntry(pAddrHM, lpHeaderCM->dwMasterGpu);
        }

        if (lpGpuMemoryEntry->data < dwLimitMemoryClock)
        {
            std::cout << "Ayar degisikligi algilandi" << std::endl;

            memcpy(lpGpuEntry, lpGpuEntrySaved, sizeof(MACM_SHARED_MEMORY_GPU_ENTRY));
            lpHeaderCM->dwCommand = MACM_SHARED_MEMORY_COMMAND_FLUSH;

        	HWND hWnd = FindWindow(NULL, L"MSI Afterburner ");

            if (hWnd)
                ::PostMessage(hWnd, WM_MACM_CMD_NOTIFICATION, 0, 0);

            while (true)
            {
                if (!lpHeaderCM->dwCommand || !CheckConnectionCM())
                    break;

                Sleep(100);
            }

            std::cout << "Ayarlar yeniden yapilandirildi" << std::endl;
        }

        Sleep(1000);
    }
}
