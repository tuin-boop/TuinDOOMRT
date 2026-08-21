#include <windows.h>
#include <dia2.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

template <typename T>
static void release(T*& value)
{
    if (value) {
        value->Release();
        value = nullptr;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) {
        std::fwprintf(stderr, L"usage: symbolize_rva.exe <msdia.dll> <file.pdb> <rva>\n");
        return 2;
    }

    const DWORD rva = static_cast<DWORD>(std::wcstoul(argv[3], nullptr, 0));
    HMODULE dia = LoadLibraryW(argv[1]);
    if (!dia) {
        std::fwprintf(stderr, L"LoadLibrary failed: %lu\n", GetLastError());
        return 3;
    }

    using DllGetClassObjectFn = HRESULT(WINAPI*)(REFCLSID, REFIID, void**);
    auto getClassObject = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(dia, "DllGetClassObject"));
    if (!getClassObject) {
        std::fwprintf(stderr, L"DllGetClassObject was not found\n");
        FreeLibrary(dia);
        return 4;
    }

    IClassFactory* factory = nullptr;
    IDiaDataSource* source = nullptr;
    IDiaSession* session = nullptr;
    IDiaSymbol* symbol = nullptr;
    IDiaEnumLineNumbers* lines = nullptr;
    HRESULT hr = getClassObject(__uuidof(DiaSource), IID_IClassFactory,
                                reinterpret_cast<void**>(&factory));
    if (SUCCEEDED(hr)) {
        hr = factory->CreateInstance(nullptr, __uuidof(IDiaDataSource),
                                     reinterpret_cast<void**>(&source));
    }
    if (SUCCEEDED(hr)) hr = source->loadDataFromPdb(argv[2]);
    if (SUCCEEDED(hr)) hr = source->openSession(&session);
    if (SUCCEEDED(hr)) hr = session->findSymbolByRVA(rva, SymTagFunction, &symbol);

    if (SUCCEEDED(hr) && symbol) {
        BSTR name = nullptr;
        DWORD symbolRva = 0;
        ULONGLONG length = 0;
        symbol->get_name(&name);
        symbol->get_relativeVirtualAddress(&symbolRva);
        symbol->get_length(&length);
        std::wprintf(L"RVA 0x%08X: %ls + 0x%X (function RVA 0x%08X, length 0x%llX)\n",
                     rva, name ? name : L"<unnamed>", rva - symbolRva,
                     symbolRva, length);
        if (name) SysFreeString(name);
    } else {
        std::wprintf(L"No function found for RVA 0x%08X (HRESULT 0x%08X)\n", rva,
                     static_cast<unsigned>(hr));
    }

    hr = session ? session->findLinesByRVA(rva, 1, &lines) : E_FAIL;
    if (SUCCEEDED(hr) && lines) {
        IDiaLineNumber* line = nullptr;
        ULONG fetched = 0;
        while (lines->Next(1, &line, &fetched) == S_OK && fetched == 1) {
            DWORD lineNo = 0;
            DWORD lineRva = 0;
            IDiaSourceFile* file = nullptr;
            BSTR fileName = nullptr;
            line->get_lineNumber(&lineNo);
            line->get_relativeVirtualAddress(&lineRva);
            if (SUCCEEDED(line->get_sourceFile(&file)) && file) {
                file->get_fileName(&fileName);
            }
            std::wprintf(L"  %ls:%lu (line RVA 0x%08X)\n",
                         fileName ? fileName : L"<unknown>", lineNo, lineRva);
            if (fileName) SysFreeString(fileName);
            release(file);
            release(line);
        }
    } else {
        std::wprintf(L"No source line found (HRESULT 0x%08X)\n",
                     static_cast<unsigned>(hr));
    }

    release(lines);
    release(symbol);
    release(session);
    release(source);
    release(factory);
    FreeLibrary(dia);
    return 0;
}
