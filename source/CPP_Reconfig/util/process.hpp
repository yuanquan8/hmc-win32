
#include "./include/process.h"
#include "./include/attribute.hpp"

namespace hmc_process
{
    /**
     * @brief 获取当前进程的父进程id
     *
     * @param matchProcessID
     * @return DWORD
     */
    DWORD getParentProcessID(DWORD matchProcessID)
    {
        DWORD CurrentProcessId = 0;
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        // 获取进程快照
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
        {
            return NULL;
        }

        // 枚举第一个进程
        if (Process32First(hSnap, &pe32))
        {
            do
            {
                // 打开进程句柄
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
                if (hProcess)
                {
                    if (pe32.th32ProcessID == matchProcessID)
                    {
                        CurrentProcessId = pe32.th32ParentProcessID;
                        CloseHandle(hProcess);
                        break;
                    }
                    CloseHandle(hProcess);
                }
            } while (Process32Next(hSnap, &pe32));
        }

        CloseHandle(hSnap);
        return CurrentProcessId;
    }

    /**
     * @brief 获取当前进程的所有子进程
     *
     * @param ProcessId
     * @param SubProcessIDList
     */
    void getSubProcessList(DWORD dwProcessID, vector<DWORD> &SubProcessIDList)
    {

        hmc_EnableShutDownPriv();
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        // 获取进程快照
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
        {
            return;
        }

        // 枚举第一个进程
        if (Process32First(hSnap, &pe32))
        {
            do
            {
                // 打开进程句柄
                HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
                if (hProcess)
                {
                    // 子进程的进程 ID
                    bool is_sub = pe32.th32ParentProcessID == dwProcessID;

                    // 二次子进程
                    if (!is_sub)
                    {
                        if (find(SubProcessIDList.begin(), SubProcessIDList.end(), pe32.th32ParentProcessID) != SubProcessIDList.end())
                        {
                            is_sub = true;
                        }
                    }

                    if (is_sub)
                    {
                        if (!(find(SubProcessIDList.begin(), SubProcessIDList.end(), pe32.th32ProcessID) != SubProcessIDList.end()))
                        {
                            SubProcessIDList.push_back(pe32.th32ProcessID);
                        }
                    }
                    CloseHandle(hProcess);
                }
            } while (Process32Next(hSnap, &pe32));
        }

        CloseHandle(hSnap);
    };

    /**
     * @brief 获取进程可执行文件路径
     *
     * @param ProcessID
     * @return string
     */
    string getFilePath(DWORD dwProcessID)
    {
        LPSTR lpFilename;
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessID);

        if (hProcess == nullptr)
        {
            CloseHandle(hProcess);
            return string("");
        }
        ::GetModuleFileNameExA(hProcess, NULL, lpFilename, MAX_PATH);
        return string(lpFilename);
    }

    /**
     * @brief 结束指定进程
     *
     * @param ProcessID
     * @return BOOL
     */
    BOOL kill(DWORD dwProcessID)
    {
        bool is_kill_success = false;
        hmc_EnableShutDownPriv();
        HANDLE killHandle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, dwProcessID);
        if (killHandle != NULL)
        {
            is_kill_success = TerminateProcess(killHandle, 0);
        }
        return is_kill_success;
    }

    /**
     * @brief 判断进程是否存在
     *
     * @param ProcessID
     * @return BOOL
     */
    BOOL exists(DWORD dwProcessID)
    {
        hmc_EnableShutDownPriv();
        bool exists_process = false;
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessID);
        if (GetLastError())
            return false;

        exists_process = hProcess != NULL;
        CloseHandle(hProcess);
        return exists_process;
    }

    /**
     * @brief 获取进程的HWND
     *
     * @param dwProcessID
     * @return HWND
     */
    HWND getHwnd(DWORD dwProcessID)
    {
        HWND win_next_it = GetTopWindow(0);
        HWND result = NULL;
        while (win_next_it)
        {
            DWORD pid = 0;
            DWORD theardId = GetWindowThreadProcessId(win_next_it, &pid);
            if (theardId != 0)
            {
                if (pid == dwProcessID && GetParent(win_next_it) == NULL && ::IsWindowVisible(win_next_it))
                {

                    result = win_next_it;
                }
            }
            win_next_it = GetNextWindow(win_next_it, GW_HWNDNEXT);
        }
        return result;
    }

    /**
     * @brief 获取可执行文件名称
     *
     * @param dwProcessID
     * @return string
     */
    string getBaseName(DWORD dwProcessID)
    {
        string FilePath;
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessID);
        char lpFilename[MAX_PATH];
        if (hProcess == nullptr)
        {

            return FilePath;
        }
        GetModuleBaseNameA(hProcess, NULL, (LPSTR)lpFilename, MAX_PATH);
        CloseHandle(hProcess);
        FilePath.append(lpFilename);
        return FilePath;
    }

    /**
     * @brief 枚举进程的线程信息
     *
     * @param dwProcessID
     * @param resultsModulePathList
     */
    void getThreadList(DWORD dwProcessID, vector<DWORD> &resultsModulePathList)
    {
        HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
        // `THREADENTRY32` 是一个结构体，它定义在 `tlhelp32.h` 头文件中。它描述了在系统执行快照时正在执行的线程列表中的条目。以下是 `THREADENTRY32` 结构体中的各个变量的含义：⁴⁵
        // - dwSize：结构体的大小，以字节为单位。
        // - cntUsage：线程使用计数。
        // - th32ThreadID：线程标识符，与 `CreateProcess` 函数返回的线程标识符兼容。
        // - th32OwnerProcessID：创建线程的进程标识符。
        // - tpBasePri：分配给线程的内核基优先级。
        // - tpDeltaPri：线程优先级相对于基本优先级的增量。
        // - dwFlags：保留，不再使用。
        THREADENTRY32 te32;

        // 对所有正在运行的线程进行快照
        hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap == INVALID_HANDLE_VALUE)
            return;

        te32.dwSize = sizeof(THREADENTRY32);

        // 检索第一个线程的信息
        if (!Thread32First(hThreadSnap, &te32))
        {
            CloseHandle(hThreadSnap);
            return;
        }

        do
        {
            if (te32.th32OwnerProcessID == dwProcessID || dwProcessID == 0)
            {
                resultsModulePathList.push_back(te32.th32ThreadID);
            }
        } while (Thread32Next(hThreadSnap, &te32));

        CloseHandle(hThreadSnap);
    }

    /**
     * @brief 枚举进程的线程信息
     *
     * @param dwProcessID
     * @param resultsModulePathList
     */
    void getThreadList(DWORD dwProcessID, vector<THREADENTRY32> &resultsModulePathList)
    {
        HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
        // `THREADENTRY32` 是一个结构体，它定义在 `tlhelp32.h` 头文件中。它描述了在系统执行快照时正在执行的线程列表中的条目。以下是 `THREADENTRY32` 结构体中的各个变量的含义：⁴⁵
        // - dwSize：结构体的大小，以字节为单位。
        // - cntUsage：线程使用计数。
        // - th32ThreadID：线程标识符，与 `CreateProcess` 函数返回的线程标识符兼容。
        // - th32OwnerProcessID：创建线程的进程标识符。
        // - tpBasePri：分配给线程的内核基优先级。
        // - tpDeltaPri：线程优先级相对于基本优先级的增量。
        // - dwFlags：保留，不再使用。
        THREADENTRY32 te32;

        // 对所有正在运行的线程进行快照
        hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (hThreadSnap == INVALID_HANDLE_VALUE)
            return;

        te32.dwSize = sizeof(THREADENTRY32);

        // 检索第一个线程的信息
        if (!Thread32First(hThreadSnap, &te32))
        {
            CloseHandle(hThreadSnap);
            return;
        }

        do
        {
            if (te32.th32OwnerProcessID == dwProcessID || dwProcessID == 0)
            {
                THREADENTRY32 copy_te32;
                copy_te32.cntUsage = te32.cntUsage;
                copy_te32.cntUsage = te32.dwFlags;
                copy_te32.cntUsage = te32.dwSize;
                copy_te32.cntUsage = te32.th32OwnerProcessID;
                copy_te32.cntUsage = te32.th32ThreadID;
                copy_te32.cntUsage = te32.tpBasePri;
                copy_te32.cntUsage = te32.tpDeltaPri;

                resultsModulePathList.push_back(copy_te32);
            }
        } while (Thread32Next(hThreadSnap, &te32));

        CloseHandle(hThreadSnap);
    }

    /**
     * @brief 获取进程引用的模块列表
     *
     * @param dwProcessID
     * @param resultsData
     */
    void getModulePathList(DWORD dwProcessID, vector<string> &resultsData)
    {

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessID);
        if (hProcess == NULL)
            return;

        vector<HANDLE> vecFileHandles;

        // 枚举进程打开的文件句柄
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32 pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(hSnapshot, &pe32))
            {
                do
                {
                    if (pe32.th32ProcessID == dwProcessID)
                    {
                        HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, dwProcessID);
                        if (hModuleSnap != INVALID_HANDLE_VALUE)
                        {
                            MODULEENTRY32 me32;
                            me32.dwSize = sizeof(MODULEENTRY32);
                            if (Module32First(hModuleSnap, &me32))
                            {
                                do
                                {
                                    HANDLE hFile = CreateFile(me32.szExePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                                    if (hFile != INVALID_HANDLE_VALUE)
                                    {
                                        vecFileHandles.push_back(hFile);
                                    }
                                } while (Module32Next(hModuleSnap, &me32));
                            }
                            CloseHandle(hModuleSnap);
                        }
                    }
                } while (Process32Next(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }

        // 输出文件路径
        for (auto it = vecFileHandles.begin(); it != vecFileHandles.end(); ++it)
        {
            LPSTR szFilePath = new CHAR[MAX_PATH];
            DWORD dwSize = GetFinalPathNameByHandleA(*it, szFilePath, MAX_PATH, FILE_NAME_NORMALIZED);
            if (dwSize > 0 && dwSize < MAX_PATH)
            {
                string strFilePath = szFilePath;
                string findStr = "\\\\?\\";
                if (strFilePath.find(findStr) == 0)
                {
                    strFilePath.replace(0, findStr.length(), "");
                }
                resultsData.push_back(strFilePath);
            }
            delete[] szFilePath;
            CloseHandle(*it);
        }

        CloseHandle(hProcess);
        return;
    }

    /**
     * @brief 获取鼠标所在的句柄的进程id
     *
     * @return DWORD
     */
    DWORD getPointWindowProcessId()
    {
        DWORD processId = 0;
        POINT curPoint;
        if (!GetCursorPos(&curPoint))
            return processId;
        HWND mainWindow = WindowFromPoint(curPoint);
        GetWindowThreadProcessId(mainWindow, &processId);
        return processId;
    }

    /**
     * @brief 获取鼠标所在的窗口的进程文件名
     *
     * @return string
     */
    string getPointWindowProcessBaseName()
    {
        return getBaseName(getPointWindowProcessId());
    }

    /**
     * @brief 获取当前聚焦的窗口的进程id
     *
     * @return DWORD
     */
    DWORD getFocusWindowProcessID()
    {
        DWORD processId;
        GetWindowThreadProcessId(GetForegroundWindow(), &processId);
        return processId;
    }

    /**
     * @brief 获取聚焦的窗口的进程文件名
     *
     * @return string
     */
    string getFocusWindowProcessBaseName()
    {
        return getBaseName(getFocusWindowProcessID());
    }

    struct ProcessEnumDetailsCont
    {
        DWORD pid;
        string baseName;
        string path;
    };

    struct ProcessEnumCont
    {
        DWORD pid;
        string baseName;
    };
    /**
     * @brief 枚举进程列表
     *
     * @param resultsData
     */

    void getProcessList(vector<ProcessEnumCont> &resultsData)
    {
        hmc_EnableShutDownPriv();
        DWORD processList[1024], cbNeeded;
        if (!EnumProcesses(processList, sizeof(processList), &cbNeeded))
        {
        }
        int numProcesses = cbNeeded / sizeof(DWORD);
        for (int i = 0; i < numProcesses; ++i)
        {
            DWORD processID = processList[i];
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
            if (hProcess)
            {
                char processName[MAX_PATH];
                GetModuleBaseNameA(hProcess, NULL, processName, MAX_PATH);
                ProcessEnumCont processEnumCont;
                processEnumCont.pid = processID;
                processEnumCont.baseName = processName;
                resultsData.push_back(processEnumCont);
                CloseHandle(hProcess);
            }
        }
    }

    void getProcessList(vector<ProcessEnumDetailsCont> &resultsData)
    {
        hmc_EnableShutDownPriv();
        DWORD processList[1024], cbNeeded;
        if (!EnumProcesses(processList, sizeof(processList), &cbNeeded))
        {
        }
        int numProcesses = cbNeeded / sizeof(DWORD);
        for (int i = 0; i < numProcesses; ++i)
        {
            DWORD processID = processList[i];
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
            if (hProcess)
            {
                char processName[MAX_PATH];
                char Filename[1024];
                GetModuleBaseNameA(hProcess, NULL, processName, MAX_PATH);
                GetModuleFileNameExA(hProcess, NULL, Filename, 1024);
                ProcessEnumDetailsCont processEnumCont;
                processEnumCont.pid = processID;
                processEnumCont.baseName = processName;
                processEnumCont.path = Filename;
                resultsData.push_back(processEnumCont);
                CloseHandle(hProcess);
            }
        }
    }

    /**
     * @brief 获取窗口句柄对应的pid
     *
     * @param hwnd
     * @return DWORD
     */
    DWORD getHwndProcessID(HWND hwnd)
    {
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);
        return processId;
    }
    struct hmc_ProcessHandleContext
    {
        long ContextID;
        bool next;
        DWORD handle;
        string name; // string
        string type; // "ALPC Port" | "Event" | "Timer" | "Mutant" | "Key" | "Section" | "File" | "Thread" | string;
    };
    vector<hmc_ProcessHandleContext> _enumProcessHandleList;
    /**
     * @brief 枚举所有进程的句柄信息
     *
     * @return long
     */
    long enumProcessHandle(DWORD dwProcessID = 0)
    {
        long queryId = hmc_napi_util::getContextID();

        vector<THREADENTRY32> ProcessThreadsList;
        getThreadList(dwProcessID, ProcessThreadsList);

        vector<hmc_Volume> volumeList = util_getVolumeList();

        int id = queryId;
        for (size_t i = 0; i < ProcessThreadsList.size(); i++)
        {
            DWORD ThreadsID = ProcessThreadsList[i].th32ThreadID;
            hmc_ProcessHandleContext handleCout;
            handleCout.ContextID = id;
            handleCout.handle = 0;
            handleCout.name = to_string(ThreadsID);
            handleCout.type = "Thread";
            handleCout.next = true;
            _enumProcessHandleList.push_back(handleCout);
        }

        vector<DWORD> SubProcessIDList;
        getSubProcessList(dwProcessID, SubProcessIDList);

        for (size_t i = 0; i < SubProcessIDList.size(); i++)
        {
            DWORD ThreadsID = SubProcessIDList[i];
            hmc_ProcessHandleContext handleCout;
            handleCout.ContextID = id;
            handleCout.handle = 0;
            handleCout.name = to_string(ThreadsID);
            handleCout.type = "Process";
            handleCout.next = true;
            _enumProcessHandleList.push_back(handleCout);
        }

        HMODULE hNtMod = LoadLibraryW(L"ntdll.dll");
        if (!hNtMod)
        {
            return;
        }
        NTQUERYSYSTEMINFORMATION NtQuerySystemInformation = (NTQUERYSYSTEMINFORMATION)GetProcAddress(hNtMod, "NtQuerySystemInformation");
        NTDUPLICATEOBJECT NtDuplicateObject = (NTDUPLICATEOBJECT)GetProcAddress(hNtMod, "NtDuplicateObject");
        NTQUERYOBJECT NtQueryObject = (NTQUERYOBJECT)GetProcAddress(hNtMod, "NtQueryObject");

        if (!NtQuerySystemInformation || !NtDuplicateObject || !NtQueryObject)
        {
            return;
        }

        PSYSTEM_HANDLE_INFORMATION handleInfo = NULL;
        HANDLE processHandle;
        ULONG i;
        ULONG neededSize = 0x1000;
        NTSTATUS Status = 0;
        ULONG ReturnLength = 0;
        handleInfo = (PSYSTEM_HANDLE_INFORMATION)malloc(neededSize);

        if (!handleInfo)
        {
            return;
        }

        // 一直查询 直到成功
        while (STATUS_INFO_LENGTH_MISMATCH == (Status = NtQuerySystemInformation(
                                                   SystemHandleInformation,
                                                   handleInfo,
                                                   neededSize,
                                                   &ReturnLength)))
        {
            if (handleInfo)
            {
                free(handleInfo);
                handleInfo = NULL;
            }
            neededSize = ReturnLength;
            handleInfo = (PSYSTEM_HANDLE_INFORMATION)malloc(neededSize);
            if (!handleInfo)
            {

                return;
            }
        }
        processHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessID);
        for (i = 0; i < handleInfo->HandleCount; i++)
        {
            hmc_ProcessHandleContext handleCout;
            handleCout.ContextID = id;
            handleCout.handle = 0;
            handleCout.name = "";
            handleCout.type = "";
            handleCout.next = true;
            SYSTEM_HANDLE handle = handleInfo->Handles[i];
            if (handle.ProcessId != dwProcessID)
            {
                continue;
            }
            handleCout.handle = handle.Handle;
            if (processHandle)
            {
                HANDLE dupHandle = NULL;
                POBJECT_TYPE_INFORMATION objectTypeInfo = NULL;
                PVOID objectNameInfo = NULL;
                UNICODE_STRING objectName = {0};
                ULONG returnLength = 0;

                do
                {
                    // 句柄复制失败 就不去获取类型名
                    Status = NtDuplicateObject(
                        processHandle,
                        (void *)handle.Handle,
                        GetCurrentProcess(),
                        &dupHandle,
                        0,
                        0,
                        0);
                    if (!NT_SUCCESS(Status))
                    {
                        break;
                    }

                    // 获取对象类型名
                    ULONG ObjectInformationLength = 0;
                    while (STATUS_INFO_LENGTH_MISMATCH == (Status = NtQueryObject(
                                                               dupHandle,
                                                               ObjectTypeInformation,
                                                               objectTypeInfo,
                                                               ObjectInformationLength,
                                                               &returnLength)))
                    {
                        if (objectTypeInfo)
                        {
                            free(objectTypeInfo);
                            objectTypeInfo = NULL;
                        }

                        ObjectInformationLength = returnLength;
                        objectTypeInfo = (POBJECT_TYPE_INFORMATION)malloc(ObjectInformationLength);
                        if (!objectTypeInfo)
                        {
                            break;
                        }
                    }

                    // 获取对象类型名成功
                    if (NT_SUCCESS(Status))
                    {
                        handleCout.type = hmc_text_util::U82A(objectTypeInfo->Name);
                    }
                    if (handle.GrantedAccess == 0x0012019f)
                    {

                        break;
                    }

                    // 获取对象名
                    ObjectInformationLength = 0;
                    returnLength = 0;

                    if (STATUS_INFO_LENGTH_MISMATCH == NtQueryObject(
                                                           dupHandle,
                                                           ObjectNameInformation,
                                                           NULL,
                                                           0,
                                                           &returnLength))
                    {

                        objectNameInfo = (POBJECT_TYPE_INFORMATION)malloc(returnLength);
                        if (!objectNameInfo)
                        {
                            break;
                        }

                        ZeroMemory(objectNameInfo, returnLength);
                        Status = NtQueryObject(
                            dupHandle,
                            ObjectNameInformation,
                            objectNameInfo,
                            returnLength,
                            NULL);
                    }

                    // 获取对象名成功
                    if (NT_SUCCESS(Status) && ((PUNICODE_STRING)objectNameInfo)->Length > 0)
                    {

                        UNICODE_STRING objectName = *(PUNICODE_STRING)objectNameInfo;

                        handleCout.name = hmc_text_util::U82A(objectName);
                        if (handleCout.type == "File")
                        {
                            for (size_t i = 0; i < volumeList.size(); i++)
                            {
                                hmc_Volume volume = volumeList[i];
                                if (handleCout.name.find(volume.device) == 0)
                                {
                                    handleCout.name.replace(0, volume.device.length(), volume.path);
                                }
                            }
                        }
                    }

                } while (FALSE);

                if (dupHandle)
                {
                    CloseHandle(dupHandle);
                    dupHandle = NULL;
                }
                if (objectTypeInfo)
                {
                    free(objectTypeInfo);
                    objectTypeInfo = NULL;
                }
                if (objectNameInfo)
                {
                    free(objectNameInfo);
                    objectNameInfo = NULL;
                }
            }
            if (!handleCout.name.empty() || !handleCout.type.empty())
            {
                // if(handleCout.type ==L"Thread"&&!ProcessThreadsList.empty()){
                //     enumHandleCout PushNewHandleCout ;
                //     PushNewHandleCout.id=id;
                //     PushNewHandleCout.type = L"Thread";
                //     PushNewHandleCout.name = L"";
                //     PushNewHandleCout.handle =0;
                //     DWORD ThreadsPID = ProcessThreadsList.front();
                //     PushNewHandleCout.name = to_wstring(ThreadsPID);
                //     ProcessThreadsList.erase(ProcessThreadsList.begin());
                //     resultsEnumHandleList.push_back(PushNewHandleCout);
                // }
                _enumProcessHandleList.push_back(handleCout);
            }
            Sleep(5);
        }

        free(handleInfo);

        return queryId;
    }
    
    // 时间格式转换
    __int64 _hmc_FileTimeToInt64(const FILETIME &time)
    {
        ULARGE_INTEGER tt;
        tt.LowPart = time.dwLowDateTime;
        tt.HighPart = time.dwHighDateTime;
        return (tt.QuadPart);
    }

    /**
     * @brief 获取进程的内存
     *
     * @param ProcessID
     * @return DWORD
     */
    DWORD getProcessMemoryInfo(DWORD ProcessID)
    {
        PROCESS_MEMORY_COUNTERS pmc;
        DWORD memoryInK = 0;
        HANDLE hProcess = NULL;

        hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
            FALSE, ProcessID);

        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
        {
            // memoryInK = pmc.WorkingSetSize/1024;		//单位为k
            memoryInK = pmc.WorkingSetSize;
        }

        CloseHandle(hProcess);
        return memoryInK;
    }

    /**
     * @brief 获取CPU核心数
     *
     * @return int
     */
    int _hmc_getCPUCount()
    {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        return static_cast<int>(system_info.dwNumberOfProcessors);
    }

    /**
     * @brief 获取指定进程CPU使用率
     *
     * @param process_id
     * @return double
     */
    double getProcessCpuUsage(DWORD process_id)
    {
        static int processor_count_ = -1;     // cpu核心数
        static __int64 last_system_time_ = 0; // 上一次的系统时间
        static __int64 last_time_ = 0;        // 上一次的时间

        FILETIME now;
        FILETIME creation_time;
        FILETIME exit_time;
        FILETIME kernel_time;
        FILETIME user_time;

        __int64 system_time;
        __int64 time;

        double cpu_usage = -1;

        if (processor_count_ == -1)
        {
            processor_count_ = _hmc_getCPUCount();
        }

        GetSystemTimeAsFileTime(&now);

        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
            FALSE, process_id);

        if (!hProcess)
        {
            return -1;
        }

        if (!GetProcessTimes(hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
        {
            return -1;
        }

        system_time = (_hmc_FileTimeToInt64(kernel_time) + _hmc_FileTimeToInt64(user_time)) / processor_count_; // CPU使用时间
        time = _hmc_FileTimeToInt64(now);                                                                       // 现在的时间

        last_system_time_ = system_time;
        last_time_ = time;

        CloseHandle(hProcess);

        Sleep(1000); // 睡眠1s

        hProcess = OpenProcess(
            PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
            FALSE, process_id);

        if (!hProcess)
        {
            return -1;
        }

        if (!GetProcessTimes(hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
        {
            return -1;
        }

        GetSystemTimeAsFileTime(&now);
        system_time = (_hmc_FileTimeToInt64(kernel_time) + _hmc_FileTimeToInt64(user_time)) / processor_count_; // CPU使用时间
        time = _hmc_FileTimeToInt64(now);                                                                       // 现在的时间

        CloseHandle(hProcess);

        cpu_usage = ((static_cast<double>(system_time - last_system_time_)) / (static_cast<double>(time - last_time_))) * 100;
        return cpu_usage;
    }
    struct hmc_PROCESSENTRY32A
    {
        DWORD cntThreads;            // 进程中的线程数。
        DWORD cntUsage;              // 表示进程的引用计数。
        DWORD dwFlags;               // 保留字段，暂时没有使用。
        DWORD dwSize;                // 结构的大小，用于指定调用方提供的结构大小，以便 API 函数可以正确填充结构。
        LONG pcPriClassBase;         // 进程的优先级。
        string szExeFile;            // 存储进程的可执行文件名，使用字符数组表示，长度为 MAX_PATH。
        ULONG_PTR th32DefaultHeapID; // 默认堆的标识符，一般用于堆管理。
        DWORD th32ModuleID;          // 拥有进程主模块的标识符，一般用于模块管理。
        DWORD th32ParentProcessID;   // 父进程的标识符。
        DWORD th32ProcessID;         // 进程的标识符(Process ID)
    };

    void _addExeFileToPROCESSENTRY32A(hmc_PROCESSENTRY32A &copyPe32, CHAR szExeFile[MAX_PATH])
    {
        copyPe32.szExeFile.append(szExeFile);
    }
    void _addExeFileToPROCESSENTRY32A(hmc_PROCESSENTRY32A &copyPe32, WCHAR szExeFile[MAX_PATH])
    {
        copyPe32.szExeFile.append(hmc_text_util::W2A(szExeFile));
    }
    /**
     * @brief 枚举进程快照
     *
     * @return vector<PROCESSENTRY32>
     */
    void
    enumProcessSnapshot(vector<hmc_PROCESSENTRY32A> &ProcessSnapshotList)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        // 获取进程快照
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
        {
            return;
        }

        // 枚举第一个进程
        if (Process32First(hSnap, &pe32))
        {
            do
            {
                hmc_PROCESSENTRY32A copyPe32;
                copyPe32.cntThreads = pe32.cntThreads;
                copyPe32.cntUsage = pe32.cntUsage;
                copyPe32.dwFlags = pe32.dwFlags;
                copyPe32.dwSize = pe32.dwSize;
                copyPe32.pcPriClassBase = pe32.pcPriClassBase;
                _addExeFileToPROCESSENTRY32A(copyPe32, pe32.szExeFile);
                copyPe32.th32DefaultHeapID = pe32.th32DefaultHeapID;
                copyPe32.th32ModuleID = pe32.th32ModuleID;
                copyPe32.th32ParentProcessID = pe32.th32ParentProcessID;
                copyPe32.th32ProcessID = pe32.th32ProcessID;
                ProcessSnapshotList.push_back(copyPe32);
            } while (Process32Next(hSnap, &pe32));
        }

        CloseHandle(hSnap);
    }
}