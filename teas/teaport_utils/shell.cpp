#include "shell.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <spawn.h>
#ifdef __APPLE__
#include <sys/wait.h>
extern "C" char **environ;
#else
#include <wait.h>
#endif
#include <errno.h>
#endif

#include <cctype>
#include <stdlib.h>
#include <map>
#include <mutex>

#include "SafePrintf.hpp"
#include "fileutils.hpp"
#include "stringutils.hpp"
#include "Log.hpp"
#include "Version.hpp"

#ifdef _WIN32
#define PATH_DELIMITER ';'
#include "windows_createprocess.hpp"
#else
#define PATH_DELIMITER ':'
#endif

using std::isspace;
namespace tea {
    std::string try_exe(std::string path) {
        throw_signal_ifneeded();
#ifdef _WIN32
        std::string path_ext = getenv("PATHEXT");
        if(path_ext.empty())
            path_ext = "exe";
        if(is_file(path))
            return path;
        for(std::string ext : split(path_ext, PATH_DELIMITER)) {
            if(ext.empty())
                continue;
            std::string test_path = path + ext;
            if(is_file(test_path))
                return test_path;
        }
#else
        if(is_file(path))
            return path;
#endif
        return "";
    }
    static std::string find_program_in_path(const std::string& name) {
        throw_signal_ifneeded();
        // because of the cache variable is static we do this to be thread safe
        static std::mutex mutex;
        std::unique_lock lock(mutex);

        static std::map<std::string, std::string> cache;
        if(name.empty())
            return "";
        if(name.size() >= 2) {
            if((name[0] == '.' && name[1] == '/') || name[0] == '/') {
                if(is_file(name)) {
                    return absdir(name);
                }
            }

            if(is_absolute_path(name) || (name[0] == '.' && name[1] == '/')) {
                if(std::string test = try_exe(name); !test.empty()) {
                    if(is_file(test)) {
                        return absdir(test);
                    }
                }
            }

        }

        std::map<std::string, std::string>::iterator it = cache.find(name);
        if(it != cache.end())
            return it->second;

        std::string path_env = getenv("PATH");
        for(std::string test : split(path_env, PATH_DELIMITER)) {
            if(test.empty())
                continue;
            test = join_path(test, name);
            test = try_exe(test);
            if(!test.empty() && is_file(test)) {
                cache[name] = test;
                return test;
            }
        }
        return "";
    }

    std::string find_program(const std::string& name) {
        if (name != "python3") {
            return find_program_in_path(name);
        }
        std::string result = find_program_in_path(name);
        if (!result.empty())
            return result;
        StrictVersion minVersion(3, 0);

        std::string path_env = getenv("PATH");
        for(std::string test : split(path_env, PATH_DELIMITER)) {
            if(test.empty())
                continue;
            test = join_path(test, "python");
            test = try_exe(test);
            if(!test.empty() && is_file(test)) {
                StrictVersion version;
                try {
                    version = command_version(test);
                    if (version >= minVersion)
                        return test;
                } catch (CommandError& e) {
                }
            }
        }
        return "";
    }
    std::string escape_shell_arg(std::string arg) {
        bool needs_quote = false;
        for(std::size_t i = 0; i < arg.size(); ++i) {
            // white list
            if(isalpha(arg[i]))
                continue;
            if(isdigit(arg[i]))
                continue;
            if(arg[i] == '.')
                continue;
            if(arg[i] == '_' || arg[i] == '-' || arg[i] == '+' || arg[i] == '/')
                continue;

            needs_quote = true;
            break;
        }
        if(!needs_quote)
            return arg;
        std::string result = "\"";
        for(unsigned int i = 0; i < arg.size(); ++i) {
            if(arg[i] == '\'')
                result += '\\';
            result += arg[i];
        }
        result += "\"";

        return result;
    }

#ifdef _WIN32

    int run_command(std::string command, std::string args) {
        throw_signal_ifneeded();
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory( &si, sizeof(si) );
        si.cb = sizeof(si);
        ZeroMemory( &pi, sizeof(pi) );

        // Start the child process. 
        if( !CreateProcess(command.c_str(),   // No module name (use command line)
            (char*)args.c_str(),        // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            0,              // No creation flags
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory 
            &si,            // Pointer to STARTUPINFO structure
            &pi )           // Pointer to PROCESS_INFORMATION structure
        )
        {
            return {};
        }

        // Wait until child process exits.
        WaitForSingleObject( pi.hProcess, INFINITE );
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        // Close process and thread handles.
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
        throw_signal_ifneeded();
        return exit_code;
    }

    int system(std::vector<std::string> args) {
        throw_signal_ifneeded();
        args = process_shebang_recursively(args);
        args = process_env(args);

        std::string program = find_program(args[0]);
        std::string command;
        if(program.empty()) {
            log_message("E1011", "command not found " + args[0]);
            return 1;
        }
        if(command.find(' ') != std::string::npos) {
            // needs quotes
            command.insert(command.begin(), '"');
            command += '"';
        }
        for(unsigned int i = 1; i < args.size(); ++i) {
            command += ' ';
            command += escape_shell_arg(args[i]);
        }

        log_message("V0002", "executing " + program + " " + command);
        int result = run_command(program, command);
        if(result != 0) {
            log_message("E1013", "failed to execute: " + program + " " +  command);
        }
        throw_signal_ifneeded();
        return result;
    }
    CompletedProcess system_capture(std::vector<std::string> args, bool capture_stderr) {
        throw_signal_ifneeded();
        std::string program = find_program(args[0]);
        std::string command;
        if(program.empty()) {
            log_message("E1011", "command not found " + args[0]);
            return {};
        }
        if(command.find(' ') != std::string::npos) {
            // needs quotes
            command.insert(command.begin(), '"');
            command += '"';
        }
        for(unsigned int i = 1; i < args.size(); ++i) {
            command += ' ';
            command += escape_shell_arg(args[i]);
        }

        log_message("V0002", "executing " + program + " " + command);
        return CreateChildProcess(program.c_str(), command.c_str(), capture_stderr);
    }
#else
    int system(std::vector<std::string> argsIn) {
        std::string program = find_program(argsIn[0]);
        if(program.empty()) {
            log_message("E1014", "command not found " + argsIn[0]);
            return 1;
        }
        argsIn[0] = program;
        int exit_code;
        posix_spawn_file_actions_t action;

        posix_spawn_file_actions_init(&action);

        pid_t pid;
        std::vector<char*> args;
        args.reserve(argsIn.size());
        for(std::string& str : argsIn) {
            args.push_back(strdup(str.c_str()));
        }
        args.push_back(0);
        if(posix_spawn(&pid, args[0], &action, NULL, &args[0], environ) != 0)
            log_message("E1015", "posix_spawn failed with error: " + std::string(strerror(errno)));
        for(char* ptr : args) {
            if(ptr != nullptr)
                free(ptr);
        }
        argsIn = {};
        args = {};

        waitpid(pid,&exit_code,0);
        if (WIFSIGNALED(exit_code)) {
            throw_signal(WTERMSIG(exit_code));
        }
#ifdef WCOREDUMP
        if(WCOREDUMP(exit_code)) {
            throw_signal(100);
        }
#endif
        CompletedProcess process;
        process.exit_code = WEXITSTATUS(exit_code);
        return process.exit_code;
    }

    CompletedProcess system_capture(std::vector<std::string> argsIn, bool captureStdErr) {
        std::string program = find_program(argsIn[0]);
        if(program.empty()) {
            log_message("E1020", "command not found " + argsIn[0]);
            return CompletedProcess{1};
        }
        argsIn[0] = program;
        int exit_code;
        int cout_pipe[2];
        posix_spawn_file_actions_t action;

        if(pipe(cout_pipe))
            log_message("E1021", "pipe returned an error.");

        posix_spawn_file_actions_init(&action);
        posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
        posix_spawn_file_actions_adddup2(&action, cout_pipe[1], 1);
        posix_spawn_file_actions_addclose(&action, cout_pipe[1]);

        if (captureStdErr)
            posix_spawn_file_actions_adddup2(&action, 1, 2);

        pid_t pid;
        std::vector<char*> args;
        args.reserve(argsIn.size());
        for(std::string& str : argsIn) {
            args.push_back(strdup(str.c_str()));
        }
        args.push_back(0);
        if(posix_spawn(&pid, args[0], &action, NULL, &args[0], environ) != 0)
            log_message("E1022", "posix_spawn failed with error: " + std::string(strerror(errno)));
        for(char* ptr : args) {
            if(ptr != nullptr)
                free(ptr);
        }
        args.clear();
        close(cout_pipe[1]); // close child-side of pipes

        constexpr int buf_size = 2048;
        uint8_t buf[buf_size];
        std::vector<uint8_t> result;
        while(true) {
            size_t transfered = read(cout_pipe[0], buf, buf_size);
            if(transfered > 0) {
                result.insert(result.end(), &buf[0], &buf[transfered]);
            } else {
                break;
            }
        }
        close(cout_pipe[0]);
        waitpid(pid,&exit_code,0);
        CompletedProcess process;
        process.exit_code = exit_code;
        process.stdout_data = std::move(result);
        throw_signal_ifneeded();
        return process;
    }
#endif



    int system_checked(std::vector<std::string> args) {
        int result = system(std::move(args));
        if (result) {
            throw CommandError(result);
        }
        return 0;
    }
    CompletedProcess system_capture_checked(std::vector<std::string> argsIn, bool capture_stderr) {
        CompletedProcess process = system_capture(std::move(argsIn), capture_stderr);
        if (process.exit_code) {
            throw CommandError(process.exit_code);
        }
        return process;
    }

    /*
    int system(const std::string& str) {
        return ::system(str.c_str());
    }
    */

    bool unzip(const std::string& zipfile, const std::string& output_dir) {
        if(is_zip(zipfile)) {
            return system_checked({"unzip", "-q", zipfile, "-d", output_dir}) == 0;
        }
        std::string message = "not a zip file: ";
        message += zipfile;
        throw IOError(message);
        //return false;
        //return system({"tar", "-xf", zipfile, }) == 0;
    }

    StrictVersion command_version(std::string command) {
        std::string name = basename(command);
        if (name.size() > 4) {
            if (name.substr(name.size()-4) == ".exe") {
                name = name.substr(0, name.size()-4);
            }
        }
        std::string path = find_program(command);
        if (path.empty()) {
            throw FileNotFoundError("command not found " + command);
        }
        // these 2 commands don't have a way to query their version
        if (name == "scp" || name == "sftp") {
            return {};
        }
        std::vector<std::string> options {path};
        bool capture_stderr = false;
        if (name == "zip" || name == "unzip") {
            options.push_back("-v");
        } else if (name == "ssh") { // todo pipe stderr
            options.push_back("-V");
            capture_stderr = true;
        } else if (name == "java") { // todo needs to pipe stderr
            options.push_back("-version");
            capture_stderr = true;
        } else {
            options.push_back("--version");
        }

        StrictVersion version = {};

        try {
            CompletedProcess process = system_capture_checked(options, capture_stderr);
            std::string output(reinterpret_cast<char*>(&process.stdout_data[0]),
                reinterpret_cast<char*>(&process.stdout_data[0]) + process.stdout_data.size());
            version = StrictVersion::from_output(output);

        } catch (CommandError& error) {
        }
        return version;
    }

    std::vector<std::string> parse_shebang(const std::string& line) {
        if (!starts_with(line, "#!")) {
            return {};
        }

        int command_start = 2;
        for (std::size_t i = command_start; i < line.size(); ++i) {
            if (!isspace(line[i])) {
                command_start = i;
                break;
            }
        }
        int command_end = -1;
        for (std::size_t i = command_start; i < line.size(); ++i) {
            if (isspace(line[i])) {
                command_end = i;
                break;
            }
        }

        int option_start = command_end+1;
        for (std::size_t i = option_start; i < line.size(); ++i) {
            if (!isspace(line[i])) {
                option_start = i;
                break;
            }
        }

        int option_end = option_start+1;
        for (std::size_t i = option_end; i < line.size(); ++i) {
            if (!isspace(line[i]))
                option_end = i+1;
        }

        std::vector<std::string> result;
        result.push_back(line.substr(command_start, command_end-command_start));
        result.push_back(line.substr(option_start, option_end - option_start));
        return result;
    }
    std::vector<std::string> parse_shebang_file(const std::string& filepath) {
        std::string file_data;

        CFile file(fopen(filepath.c_str(), "rb"));
        if (!file)
            return {};

        std::string line;

        constexpr int read_size = 1024;
        line = file.read_str(read_size);

        if(!starts_with(line, "#!/usr/bin/env "))
            return {};

        std::string::size_type lf_pos = line.find('\n');
        if (lf_pos != std::string::npos) {
            line.resize(lf_pos);
        }
        while (lf_pos == std::string::npos) {
            std::string more = file.read_str(1024);
            lf_pos = more.find('\n');
            if (lf_pos != std::string::npos) {
                more.resize(lf_pos);
                line += more;
                break;
            }
        }

        return parse_shebang(line);
    }

    CommandLine process_shebang_recursively(CommandLine args) {
        while (is_file(args[0])) {
            CommandLine parts = parse_shebang_file(args[0]);
            if (parts.empty())
                break;
            if (!parts.empty()) {
                args[0] = parts[0];
                if (parts.size() >= 2) {
                    args.insert(args.begin()+1, parts.begin()+1, parts.end());
                }
            }
        }
        return args;
    }

    CommandLine process_env(CommandLine args) {
        if (args[0] == "/usr/bin/env" && args.size() >= 2) {
            args[0] = find_program(args[1]);
            args.erase(args.begin()+1);
        }
        return args;
    }
}