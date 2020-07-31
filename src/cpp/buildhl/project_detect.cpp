#include "project_detect.hpp"

#include <subprocess.hpp>
#include <filesystem>
#include <teaport_utils/fileutils.hpp>
#include <teaport_utils/stringutils.hpp>
#include <teaport_utils/shell.hpp>

#include "lexer.hpp"

namespace fs = std::filesystem;

namespace buildhl {

    std::string to_string(BuildType build_type) {
        switch(build_type) {
        case BuildType::debug: return "debug";
        case BuildType::release: return "release";
        }
        return "release";
    }

    InvocationInfo parse_args(std::vector<std::string> args) {
        InvocationInfo invocation;
        if (!subprocess::cenv["BUILDHL_MAX_JOBS"].to_string().empty()) {
            invocation.max_jobs = std::stoi(subprocess::cenv["BUILDHL_MAX_JOBS"]);
        }
        invocation.project_dir = subprocess::getcwd();

        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--build") {
                invocation.build_dir = args[i+1];
                ++i;
            } else if (args[i] == "--project") {
                invocation.project_dir = args[i+1];
                ++i;
            } else if (args[i] == "--target") {
                invocation.target = args[i+1];
                ++i;
            } else if (args[i] == "debug") {
                invocation.build_type = BuildType::debug;
            } else if (args[i] == "release") {
                invocation.build_type = BuildType::release;
            } else if (args[i].substr(0, 2) == "-j") {
                invocation.max_jobs = std::stoi(args[i].substr(2));
            } else if (args[i][0] == '-') {
                invocation.configure_options.push_back(args[i]);
            } else {
                invocation.target = args[i];
            }
        }
        return invocation;
    }


    PopenInputStream_uptr popen_command(const CommandLine& command, const std::string& cwd="", subprocess::EnvMap env={}) {
        BlockSignalRaii bsr;
        // this makes compatibility on windows better. So you can have the same
        // script work on windows & other OS's
        #ifdef _WIN32
        CommandLine rcommand = tea::process_shebang_recursively(command);
        rcommand = tea::process_env(rcommand);
        #else
        CommandLine rcommand = command;
        #endif
        auto builder = subprocess::RunBuilder(rcommand)
            .cwd(cwd)
            .cout(subprocess::PipeOption::pipe).cerr(subprocess::PipeOption::cout);
        if (!env.empty()) {
            builder.env(env);
        }

        return std::make_unique<PopenInputStream>(builder.popen());
    }

    struct CFileInputStream : InputStream {
        CFileInputStream(FILE* fp) {mFile = fp;}
        ~CFileInputStream() {
            if (mFile != nullptr)
                fclose(mFile);
        }
        ssize_t read(void* buffer, size_t size) override {
            return fread(buffer, 1, size, mFile);
        }
    private:
        FILE* mFile;
    };

    std::string read_all(InputStream& input) {
        std::vector<char> buffer(2048);
        std::string result;
        while (true) {
            ssize_t transfered = input.read(&buffer[0], buffer.size());
            if (transfered <= 0)
                break;
            result.insert(result.end(), buffer.begin(), buffer.begin() + buffer.size());
        }
        return result;
    }
    Project::Project(std::string dir, std::string build_dir) {
        mProjectDir = dir;
        mBuildDir = build_dir;
    }

    std::pair<std::string, std::string> parse_name_value(const std::string& line) {
        if (line.empty() || line[0] == '#' || line[0] == '/')
            return {};
        auto parts = tea::split(line, '=');
        if (parts.size() != 2)
            return {};
        std::string name = parts[0];
        std::string value = parts[1];
        if (name.find('-') != std::string::npos) {
            name = name.substr(0, name.find('-'));
        }
        if (name.find(':') != std::string::npos) {
            name = name.substr(0, name.find(':'));
        }
        return {name, value};
    }
    std::map<std::string, std::string> load_cmake_cache(const std::string& filepath) {
        std::map<std::string, std::string> result;
        if (!tea::path_exists(filepath))
            return {};
        std::string data = tea::file_get_contents(filepath);
        for(auto line : tea::split(data, '\n')) {
            auto pair = parse_name_value(line);
            if (pair.first.empty())
                continue;
            result[pair.first] = pair.second;
        }
        return result;
    }

    bool CMakeProject::should_configure() {
        std::string cmake_cache_file = tea::join_path(get_build_dir(), "CMakeCache.txt");
        if (!tea::path_exists(cmake_cache_file))
            return true;
        auto vars = load_cmake_cache(cmake_cache_file);

        if (vars["CMAKE_GENERATOR"].empty())
            return true;
        if (vars["CMAKE_HOME_DIRECTORY"].empty())
            return true;
        if (vars["CMAKE_PROJECT_NAME"].empty())
            return true;
        std::string cmake_file = tea::join_path(vars["CMAKE_HOME_DIRECTORY"], "CMakeLists.txt");
        if (!tea::path_exists(cmake_file)) {
            return false;
        }
        if (fs::last_write_time(cmake_file) > fs::last_write_time(cmake_cache_file))
            return true;
        return false;
    }

    std::string cmake_build_type(BuildType build_type) {
        switch (build_type) {
        case BuildType::debug:      return "Debug";
        case BuildType::release:    return "Release";
        default:                    return "Release";
        }
    }
    InputStream_uptr CMakeProject::configure(CommandLine commandExtra) {
        if (!fs::exists(get_build_dir())) {
            tea::mkdir_p(get_build_dir());
        }
        if (!fs::exists(get_build_dir())) {
            std::cout << "Could not create build directory: " << get_build_dir();
            std::cout << "\n";
            throw std::runtime_error("could not create build dir");
        }
        bool has_ninja = !subprocess::find_program("ninja").empty();
        CommandLine command = {"cmake", get_project_dir()};

        bool generator_specified = false;
        bool build_type_specified = false;
        for(auto& arg : commandExtra) {
            if (arg[0] == '-' && arg[1] == 'G')
                generator_specified = true;
            if (tea::starts_with(arg, "-DCMAKE_BUILD_TYPE="))
                build_type_specified = true;
        }
        command.insert(command.end(), commandExtra.begin(), commandExtra.end());

        std::string cmake_cache_file = tea::join_path(get_build_dir(), "CMakeCache.txt");
        auto vars = load_cmake_cache(cmake_cache_file);
        generator_specified |= !vars["CMAKE_GENERATOR"].empty();

        if (!generator_specified && has_ninja)
            command.push_back("-GNinja");

        std::string build_type = cmake_build_type(get_invocation().build_type);
        if (!build_type_specified)
            command.push_back("-DCMAKE_BUILD_TYPE=" + build_type);

        return popen_command(command, get_build_dir());
    }

    InputStream_uptr CMakeProject::make(std::string target) {
        subprocess::EnvGuard env_guard;
        CommandLine command = {"cmake", "--build", get_build_dir()};
        if (!target.empty()) {
            command.push_back("--target");
            command.push_back(target);
        }
        if (get_invocation().max_jobs > 0) {
            command.push_back("--");
            command.push_back("-j" + std::to_string(get_invocation().max_jobs));
        }

        return popen_command(command);
    }



    std::string join(const std::vector<std::string>& vec, char delimiter) {
        bool put_del = false;
        std::string result;
        for (auto& part : vec) {
            if (put_del) {
                result += delimiter;
            }
            put_del = true;
            result += part;
        }
        return result;
    }

    CMakeProject_uptr detect_cmake_project(InvocationInfo invocation) {
        std::string dir = subprocess::abspath(invocation.project_dir);
        std::string project_dir;
        std::string build_dir;
        using tea::split;
        if (fs::exists(dir + "/CMakeCache.txt")) {
            FILE*fp = fopen((dir + "/CMakeCache.txt").c_str(), "rb");
            CFileInputStream input(fp);
            std::string data = read_all(input);

            auto lines = split(data, '\n');
            for(auto& line : lines) {
                if (line.empty() || line[0] == '#' || line[0] == '/')
                    continue;
                auto parts = split(line, '=');
                if (parts.size() != 2)
                    continue;
                std::string name = parts[0];
                std::string value = parts[1];
                if (name.find('-') != std::string::npos) {
                    name = name.substr(0, name.find('-'));
                }
                if (name.find(':') != std::string::npos) {
                    name = name.substr(0, name.find(':'));
                }
                if (name == "CMAKE_HOME_DIRECTORY") {
                    project_dir = value;
                }
            }
            build_dir = dir;
            return std::make_unique<CMakeProject>(project_dir, build_dir);
        } else if (fs::exists("CMakeLists.txt")) {
            project_dir = dir;
            build_dir = invocation.build_dir;
            if (build_dir.empty()) {
                build_dir = join({project_dir, "build",
                    invocation.build_type == BuildType::release? "release" : "debug"}, '/');
            }

            return std::make_unique<CMakeProject>(project_dir, build_dir);
        }

        return nullptr;
    }

    CommandLine parse_command(nlohmann::json& js) {
        if (js.is_string()) {
            return {js.get<std::string>()};
        }
        CommandLine cmd;
        for (auto& value : js) {
            cmd.push_back(value.get<std::string>());
        }
        return cmd;
    }

    BuildProject::BuildProject(const std::string& project_dir) : Project(project_dir) {
        m_build_file = project_dir + "/buildhl.json";
        auto json = tea::load_json_file(m_build_file);

#define LOAD(what) if (json.contains(#what)) { \
        m_commands.what = parse_command(json[#what]);\
    }
        LOAD(env_script)
        LOAD(make)
        LOAD(configure)
        LOAD(should_configure)
#undef LOAD

    }
    CommandLine process_shebang_recursively(CommandLine args) {
        while (tea::is_file(args[0])) {
            CommandLine parts = tea::parse_shebang_file(args[0]);
            if (parts.empty())
                break;
            if (!parts.empty()) {
                args.insert(args.begin(), parts.begin(), parts.end());
            }
        }
        return args;
    }

    void load_env(CommandLine cmd) {
        if (cmd.empty())
            return;

        auto env = subprocess::current_env_copy();
        for (auto& part : cmd) {
            part = tea::replace_string_variables(part, env);
        }
        #ifdef _WIN32
        cmd = process_shebang_recursively(cmd);
        cmd = tea::process_env(cmd);
        #endif

        auto process = subprocess::RunBuilder(cmd)
            .cout(subprocess::PipeOption::pipe)
            .run();
        auto lines = tea::split_no_empty(process.cout, '\n');

        using namespace lex;
        std::vector<StaticString> ignore_env {"CWD", "PWD", "HOME",
            "HOMEDRIVE", "HOMEPATH", "USER", "UserProfile"};
        for (auto& line : lines) {
            StaticString str(line.c_str());
            if (str.find("=") == StaticString::npos)
                continue;
            while (str[str.size()-1] == '\r')
                str.trim_end(1);
            auto equal_pos = str.find("=");
            StaticString value = str.substr(equal_pos+1);
            size_t name_start = equal_pos -1;
            while (name_start > 0 && isspace(str[name_start]))
                --name_start;
            if (isspace(str[name_start]))
                continue;
            size_t name_end = name_start + 1;
            while (name_start > 0 && !isspace(str[name_start]))
                --name_start;
            if (isspace(str[name_start]))
                ++name_start;
            StaticString name = str.substr(name_start, name_end - name_start);
            if (name.empty())
                continue;
            std::string upper = name.to_upper();
            bool ignore = false;
            for (auto& it : ignore_env) {
                if (it == upper) {
                    ignore = true;
                    break;
                }
            }
            if (ignore)
                continue;
            subprocess::cenv[name.to_string()] = value.to_string();
        }
    }

    void BuildProject::load_env_if_needed() {
        if (m_env_loaded)
            return;
        load_env(m_commands.env_script);
        m_base_project = detect_cmake_project(get_invocation());
        m_env_loaded = true;
    }

    bool BuildProject::should_configure() {
        load_env_if_needed();
        if (m_base_project) {
            if (m_base_project->should_configure())
                return true;
        }
        if (!m_commands.should_configure.empty()) {
            auto env = get_build_env();
            CommandLine cmd = m_commands.configure;
            for (auto& part : cmd) {
                part = tea::replace_string_variables(part, env);
            }
            auto process = subprocess::RunBuilder(cmd).env(env).run();
            return !process;
        }

        return m_base_project == nullptr && !m_commands.configure.empty();
    }

    subprocess::EnvMap BuildProject::get_build_env() {
        load_env_if_needed();
        using std::to_string;
        auto env = subprocess::current_env_copy();
        return env;
    }

    InputStream_uptr BuildProject::configure(CommandLine args) {
        CommandLine cmd = m_commands.configure;
        if (cmd.empty()) {
            if (m_base_project != nullptr) {
                return m_base_project->configure(args);
            }
            return nullptr;
        }
        auto env = get_build_env();

        for (auto& part : cmd) {
            part = tea::replace_string_variables(part, env);
        }
        cmd.insert(cmd.end(), args.begin(), args.end());
        return popen_command(cmd, "", env);
    }

    InputStream_uptr BuildProject::make(std::string target) {
        CommandLine cmd = m_commands.configure;
        if (cmd.empty()) {
            if (m_base_project != nullptr) {
                return m_base_project->make(target);
            }
            return nullptr;
        }
        auto env = subprocess::current_env_copy();
        if (!target.empty())
            env["TARGET"] = target;

        for (auto& part : cmd) {
            part = tea::replace_string_variables(part, env);
        }
        return popen_command(cmd, "", env);
    }

    BuildProject_uptr detect_build_project(InvocationInfo invocation) {
        std::string dir = subprocess::abspath(invocation.project_dir);
        std::string project_dir = invocation.project_dir;
        using tea::split;
        if (fs::exists(dir + "/buildhl.json")) {
            return std::make_unique<BuildProject>(project_dir);
        }

        return nullptr;

    }

    Project_uptr detect_project(InvocationInfo invocation) {
        Project_uptr project = detect_build_project(invocation);
        if (project != nullptr)
            return project;
        project = detect_cmake_project(invocation);
        if (project != nullptr)
            return project;
        return nullptr;
    }
    void block_signals() {
#ifndef _WIN32
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);

        sigprocmask(SIG_BLOCK, &set, nullptr);
#endif
    }

    void unblock_signals() {
#ifndef _WIN32
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);

        sigprocmask(SIG_UNBLOCK, &set, nullptr);
#endif
    }
}