#pragma once

// this is a good item to be it's own library. Cross platform process
// creation library.

#include <string>
#include <initializer_list>
#include <vector>
#include <cstdint>

#include "Version.hpp"

namespace tea {
    struct CompletedProcess {
        int exit_code = 1;
        std::vector<uint8_t> stdout_data;
        explicit operator bool() const {
            return exit_code == 0;
        }
    };
    typedef std::vector<std::string> CommandLine;


    std::string find_program(const std::string& name);
    std::string escape_shell_arg(std::string arg);
    int system(std::vector<std::string> args);
    CompletedProcess system_capture(std::vector<std::string> argsIn, bool capture_stderr=false);

    // these 2 throw CommandError if exit status is not 0
    int system_checked(std::vector<std::string> args);
    CompletedProcess system_capture_checked(std::vector<std::string> argsIn, bool capture_stderr=false);

    /* because of windows we won't allow this form
    int system(const std::string& str);
    */
    bool unzip(const std::string& zipfile, const std::string& output_dir);
    /** runs the command and tries to parse the version.

        @param command  Either command name or path to command.
        @returns    The version of the command parsed. or 0.0.0 if could not
                    parse.
        @throw CommandError         If there is an error executing the command.
        @throw FileNotFoundError    If the command could not be found.
    */
    StrictVersion command_version(std::string command);

    CommandLine parse_shebang(const std::string& line);
    CommandLine parse_shebang_file(const std::string& filepath);

    CommandLine process_shebang_recursively(CommandLine args);
    CommandLine process_env(CommandLine args);
}