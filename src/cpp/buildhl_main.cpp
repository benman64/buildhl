#include <iostream>
#include <cstring>
#include <teaport_utils/fileutils.hpp>
#include <subprocess.hpp>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "buildhl/highlight.hpp"

#include "buildhl/project_detect.hpp"
#include "buildhl/FileFilter.hpp"
#include "buildhl/ProgressAnalyser.hpp"

using namespace buildhl;


#ifdef _WIN32
void enableColors() {
    HANDLE hstd = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD flags;
    // no colors is not a big deal so just silently return
    if (!GetConsoleMode(hstd, &flags))
        return;

    if (flags & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        return;

    SetConsoleMode(hstd, flags | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
// nothing for all other OS's
void enableColors() {}
#endif
std::string dirname(std::string path) {
    size_t slash_pos = path.size();;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/' || path[i] == '\\')
            slash_pos = i;
    }
    return path.substr(0, slash_pos);
}

class StreamProcessor {
public:
    StreamProcessor(){
        process_line("[build start]");
        std::string absolute_str = subprocess::cenv["BUILDHL_ABSOLUTE"];
        bool absolute = !absolute_str.empty() && absolute_str != "0";
        m_file_filter.set_always_absolute(absolute);
    }
    StreamProcessor(const std::string log_file) {
        std::string dir = dirname(log_file);
        if (!tea::path_exists(dir)) {
            try {
                // bug in this guy needs to be fixed
                tea::mkdir_p(dir);
            } catch (...) {

            }
        }
        FILE* fp = nullptr;
        if (!log_file.empty())
            fp = fopen(log_file.c_str(), "w");
        if (fp == nullptr) {
            process_line("could not open for writing: " + log_file);
        } else {
            m_log_file = std::make_unique<CFileOutputStream>(fp);
        }
        process_line("[build start]");
    }
    ~StreamProcessor() {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_active = false;
        }
        close_thread();
        m_progress.clear();
        std::string message = std::to_string(m_total_errors) + " errors " + std::to_string(m_total_warnings) + " warnings";
        process_line(message);
        std::string total_build = "total build time: " + nice_time(m_stop_watch.seconds());
        process_line(total_build);
        process_line("[build end]");
        std::string line = "[build end]";

    }
    void log(const std::string& line) {
        if (m_log_file == nullptr)
            return;
        if (line.size() > 0) {
            m_log_file->write(line.c_str(), line.size());
            if (line[line.size()-1] != '\n')
                m_log_file->write("\n", 1);
        } else {
            m_log_file->write("\n", 1);
        }
    }

    void process_line(std::string line) {
        if (line.empty())
            return;
        log(line);
        lex::StaticString line_ss(line.c_str());

        auto tokens = tokenize(line_ss);
        for (auto token : tokens) {
            lex::StaticString str = line_ss.substr(token);
            auto upper = str.to_upper();
            if (upper == "ERROR") {
                ++m_total_errors;
                break;
            } else if (upper == "WARNING") {
                ++m_total_warnings;
                break;
            }
        }
        Progress progress = parse_progress(line);

        line = m_file_filter.filter(line);
        line = color_line(line);
        enableColors();
        if (m_last_is_progress) {
            std::cout << '\r';
            bcolors colors;
            std::cout << colors.CLEAR_LINE;
        }
        std::cout << line;
        if (line[line.size()-1] != '\n')
            std::cout << '\n';

        if (progress > 0) {
            m_progress.complete(progress);
        }
        m_last_is_progress  = false;
    }

    void update_progress_line() {
        if (m_last_is_progress) {
            std::cout << '\r';
            bcolors colors;
            std::cout << colors.CLEAR_LINE;
        }
        if (m_progress.size() > 0) {
            double progress = m_progress.progress();
            std::string pline = render_progress(progress, 20);
            pline = left_pad(std::to_string((int)(progress*100)), 3) + "% " + pline;
            pline += " " + nice_time(m_progress.eta()) + " eta";
            if (pline != m_progress_line || !m_last_is_progress) {
                m_progress_line = pline;
                std::cout << pline;
                std::cout << std::flush;
            }
            m_last_is_progress = true;
        } else {
            m_last_is_progress = false;
        }

    }

    void process(InputStream& input) {
        start_update_thread_ifneeded();
        int signal_code = 0;
        while (true) {
            try {
                tea::throw_signal_ifneeded();
            } catch (tea::SignalError& err) {
                signal_code = err.code();
                if (auto pinput = dynamic_cast<PopenInputStream*>(&input); pinput) {
                    std::cout << "sending signal " << signal_code << "\n";
                    pinput->popen().send_signal(signal_code);
                    // tell it to die too
                    pinput->popen().terminate();
                }
            }
            std::string line = buildhl::getline(input);
            if (line.empty())
                break;
            std::unique_lock<std::mutex> lock(m_mutex);
            process_line(line);
        }
        if (m_last_is_progress) {
            std::cout << '\r';
            bcolors colors;
            std::cout << colors.CLEAR_LINE << std::flush;
        }
        if (signal_code) {
            throw tea::SignalError(signal_code);
        }
    }

    void add_search_path(const std::string& str) {
        m_file_filter.add_search_path(str);
    }
    void set_base_dir(const std::string& str) {
        m_file_filter.set_base_dir(str);
    }
private:
    void start_update_thread_ifneeded() {
        if (m_update_thread.joinable())
            return;
        m_update_thread = std::thread([this] () {
            while (true) {
                subprocess::sleep_seconds(0.1);
                std::unique_lock<std::mutex> lock(m_mutex);
                if (!m_active)
                    break;
                update_progress_line();
            }
        });
    }
    void close_thread() {
        if (m_update_thread.joinable())
            m_update_thread.join();
    }
    OutputStream_uptr m_log_file;
    FileFilter m_file_filter;
    subprocess::StopWatch m_stop_watch;
    ProgressGraph m_progress;
    bool m_last_is_progress = false;
    std::mutex m_mutex;
    std::thread m_update_thread;
    bool m_active = true;
    std::string m_progress_line;

    int m_total_errors      = 0;
    int m_total_warnings    = 0;
};


std::vector<std::string> argv_to_vector(int argc, char** argv) {
    std::vector<std::string> result;
    for (int i = 1; i < argc; ++i) {
        result.push_back(argv[i]);
    }
    return result;
}

std::vector<std::string> argv_to_vector(int argc, lex::CString* argv) {
    std::vector<std::string> result;
    for (int i = 1; i < argc; ++i) {
        result.push_back(argv[i].str);
    }
    return result;
}

struct CinStream : buildhl::InputStream {
    ssize_t read(void* buffer, size_t size) override {
        return fread(buffer, 1, size, stdin);
    }
};

void print_help() {
    std::cout << "buildhl " PROJECT_VERSION R"( - Highlight your build output.

usage: buildhl -
    do "command | buildhl -" to process stdin. No further options will be
    processed.

usage: buildhl [<options>] [<build-type>=debug|release] [<target>]

    build-type  It is optionsal and can be either debug or release. Default is
                release.
    
    target      Optional the target to build. If ommitted, it's ommited being
                specified when running build command.


options:
    --build     The build directory to use. Defaults to
                <project>/build/<build-type>
    --project   defaults to PWD. The directory of which project
    --target    The target to build. If ommitted, it's ommited being specified
                when running build command.

Environment variables:
    BUILDHL_MAX_JOBS    When possible this number will be used to specify to
                        builders for the amount of jobs they run concurrently.

These environment variables are set for invocations of buildhl:
    BUILDHL_BUILD_TYPE
    BUILDHL_TARGET
    BUILDHL_PROJECT_DIR
    BUILDHL_MAX_JOBS
)";
}
int main(int argc, char** argv_in) {
    auto argv = reinterpret_cast<lex::CString*>(argv_in);

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == "--version") {
            std::cout << "buildhl version " PROJECT_VERSION;
            std::cout << "\n";
            return 0;
        }
        if (argv[i] == "--help") {
            print_help();
            return 1;
        }
    }

    if (argc == 2 && argv[1] == "-") {
        StreamProcessor stream_processor;
        stream_processor.add_search_path(tea::getcwd());
        CinStream cin;
        stream_processor.process(cin);
        return 0;
    }

    std::vector<std::string> args = argv_to_vector(argc, argv);
    InvocationInfo invocation = parse_args(args);

    {
        using subprocess::cenv;
        using std::to_string;
        auto env = subprocess::current_env_copy();
        cenv["BUILDHL_BUILD_TYPE"]      = to_string(invocation.build_type);
        cenv["BUILDHL_TARGET"]          = invocation.target;
        cenv["BUILDHL_PROJECT_DIR"]     = invocation.project_dir;
        cenv["BUILDHL_MAX_JOBS"]        = to_string(invocation.max_jobs);
    }

    auto project = buildhl::detect_project(invocation);
    InputStream_uptr input;
    if (project != nullptr) {
        project->set_invocation(invocation);

        bool signal_error = false;
        unblock_signals();
        try {
            StreamProcessor stream_processor(tea::join_path(project->get_build_dir(), "build.log"));
            stream_processor.set_base_dir(project->get_project_dir());
            stream_processor.add_search_path(project->get_build_dir());
            stream_processor.add_search_path(tea::getcwd());
            if (project->should_configure()) {
                input = project->configure(invocation.configure_options);
                if (input != nullptr) {
                    block_signals();
                    stream_processor.process(*input);
                }
            }
            unblock_signals();
            input = project->make(invocation.target);
            if (input != nullptr) {
                block_signals();
                stream_processor.process(*input);
            }
        } catch (tea::SignalError& err) {
            signal_error = true;
            if (auto pinput = dynamic_cast<PopenInputStream*>(input.get()); pinput) {
                pinput->popen().send_signal(err.code());
            }
        }
        if (auto pinput = dynamic_cast<PopenInputStream*>(input.get()); pinput) {
            int status = pinput->popen().wait();
            return signal_error? 1 : status;
        }
        return signal_error? 1 : 0;
    }

    return 1;
}