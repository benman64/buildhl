#ifdef _WIN32
#include "windows_createprocess.hpp"

#include <windows.h> 
#include <tchar.h>
#include <stdio.h> 
#include <strsafe.h>
#include "shell.hpp"
#include "exceptions.hpp"

static STARTUPINFO g_startupInfo;
static bool g_startupInfoInit = false;


static void init_startup_info() {
    if(g_startupInfoInit)
        return;
    GetStartupInfo(&g_startupInfo);
}



namespace tea {
 
    CompletedProcess CreateChildProcess(const char* command, const char* args, bool capture_stderr) {
        throw_signal_ifneeded();
        init_startup_info();

        HANDLE g_hChildStd_OUT_Rd = NULL;
        HANDLE g_hChildStd_OUT_Wr = NULL;

        SECURITY_ATTRIBUTES saAttr; 
    
        // Set the bInheritHandle flag so pipe handles are inherited. 
        
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
        saAttr.bInheritHandle = TRUE; 
        saAttr.lpSecurityDescriptor = NULL; 

        // Create a pipe for the child process's STDOUT. 
        if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) ) 
                return {};

        // Ensure the read handle to the pipe for STDOUT is not inherited.

        if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
                return {};

        PROCESS_INFORMATION piProcInfo; 
        STARTUPINFO siStartInfo;
        BOOL bSuccess = FALSE; 
        
        // Set up members of the PROCESS_INFORMATION structure. 
        
        ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
        
        // Set up members of the STARTUPINFO structure. 
        // This structure specifies the STDIN and STDOUT handles for redirection.
        
        ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
        siStartInfo.cb = sizeof(STARTUPINFO); 
        siStartInfo.hStdError = capture_stderr? g_hChildStd_OUT_Wr : g_startupInfo.hStdError;
        siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
        siStartInfo.hStdInput = g_startupInfo.hStdInput;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
        
        // Create the child process. 
            
        bSuccess = CreateProcess(command, 
            (char*)args,     // command line 
            NULL,          // process security attributes 
            NULL,          // primary thread security attributes 
            TRUE,          // handles are inherited 
            0,             // creation flags 
            NULL,          // use parent's environment 
            NULL,          // use parent's current directory 
            &siStartInfo,  // STARTUPINFO pointer 
            &piProcInfo);  // receives PROCESS_INFORMATION 

        CloseHandle(g_hChildStd_OUT_Wr);
        if ( ! bSuccess )
            return {};
    
        constexpr int buf_size = 2048;
        uint8_t buf[buf_size];
        std::vector<uint8_t> result;
        while(true) {
            DWORD transfered = 0;
            bool success = !!ReadFile( g_hChildStd_OUT_Rd, buf, buf_size, &transfered, NULL);
            if(!success)
                break;
            if(transfered > 0) {
                result.insert(result.end(), &buf[0], &buf[transfered]);
            } else {
                break;
            }
        }
        // Wait until child process exits.
        WaitForSingleObject(piProcInfo.hProcess, INFINITE );
        DWORD exit_code;
        GetExitCodeProcess(piProcInfo.hProcess, &exit_code);

    
        // Close handles to the child process and its primary thread.
        // Some applications might keep these handles to monitor the status
        // of the child process, for example. 

        CloseHandle(piProcInfo.hProcess);
        CloseHandle(piProcInfo.hThread);

        CloseHandle(g_hChildStd_OUT_Wr);
        CloseHandle(g_hChildStd_OUT_Rd);
        tea::CompletedProcess process;
        process.exit_code = exit_code;
        process.stdout_data = std::move(result);
        throw_signal_ifneeded();
        return process;
    }
    
 
}
 
void ErrorExit(PTSTR lpszFunction) 

// Format a readable error message, display a message box, 
// and exit from the application.
{ 
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf)+lstrlen((LPCTSTR)lpszFunction)+40)*sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(1);
}

#endif