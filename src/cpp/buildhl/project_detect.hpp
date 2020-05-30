#pragma once

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <subprocess.hpp>

namespace buildhl {
    using std::unique_ptr;
    typedef std::vector<std::string> CommandLine;

    enum class BuildType {
        debug, release
    };

    std::string to_string(BuildType build_type);

    struct InvocationInfo {
        std::string                 project_dir;
        std::string                 build_dir;
        std::string                 target;
        BuildType                   build_type = BuildType::release;
        /** base path where all paths to be printed relative to */
        std::string                 path_base;
        std::vector<std::string>    configure_options;
        int                         max_jobs = 0;
    };

    InvocationInfo parse_args(std::vector<std::string> args);

    struct VBase {
        virtual ~VBase(){}
    };
    class InputStream : public VBase {
    public:
        virtual ssize_t read(void* buffer, size_t size)=0;
    };

    class OutputStream : public VBase {
    public:
        virtual ssize_t write(const void* buffer, size_t size)=0;
    };
    class CFileOutputStream : public OutputStream {
    public:
        CFileOutputStream(){}
        CFileOutputStream(FILE* fp) {mFile = fp;}
        ~CFileOutputStream() {
            close();
        }
        void close() {
            if (mFile != nullptr) {
                fclose(mFile);
                mFile = nullptr;
            }
        }
        ssize_t write(const void* buffer, size_t size) override {
            return fwrite(buffer, 1, size, mFile);
        }
    private:
        FILE* mFile = nullptr;
    };
    typedef unique_ptr<InputStream> InputStream_uptr;
    typedef unique_ptr<OutputStream> OutputStream_uptr;
    typedef unique_ptr<CFileOutputStream> CFileOutputStream_uptr;

    struct PipeInputStream : InputStream {
        PipeInputStream(subprocess::PipeHandle handle) : mHandle(handle){}
        ~PipeInputStream() {
            subprocess::pipe_close(mHandle);
        }
        ssize_t read(void* buffer, size_t size) override {
            return subprocess::pipe_read(mHandle, buffer, size);
        }
    private:
        subprocess::PipeHandle mHandle  = subprocess::kBadPipeValue;
    };

    struct PopenInputStream : PipeInputStream {
        PopenInputStream(subprocess::Popen&& popen) :
            PipeInputStream(popen.cout), mPopen(std::move(popen)) {
        }

        subprocess::Popen& popen() { return mPopen; }
    private:
        subprocess::Popen mPopen;
    };
    typedef std::unique_ptr<PopenInputStream> PopenInputStream_uptr;


    inline std::string getline(InputStream& stream) {
        std::string result;
        while (true) {
            char ch;
            ssize_t transfered = stream.read(&ch, 1);
            if (transfered != 1)
                break;
            result += ch;
            if (ch == '\n')
                break;
        }
        return result;
    }
    class Project : public VBase {
    public:
        Project(std::string project_dir, std::string build_dir="");
        std::string get_project_dir() const {return mProjectDir;}
        std::string get_build_dir() const {return mBuildDir; }

        InvocationInfo get_invocation() const { return m_invocation_info; }
        void set_invocation(const InvocationInfo& info) { m_invocation_info = info;}

        virtual bool should_configure() {return false;}
        virtual InputStream_uptr configure(CommandLine command){return nullptr;}
        virtual InputStream_uptr make(std::string target="")=0;
    private:
        std::string mProjectDir;
        std::string mBuildDir;
        InvocationInfo m_invocation_info;
    };
    typedef std::unique_ptr<Project> Project_uptr;

    class CMakeProject : public Project {
    public:
        CMakeProject(std::string project_dir, std::string build_dir) : Project(project_dir, build_dir) {}

        bool should_configure() override;
        InputStream_uptr configure(CommandLine command) override;
        InputStream_uptr make(std::string target="") override;

    private:
        std::map<std::string, std::string> mDefines;
    };
    typedef std::unique_ptr<CMakeProject> CMakeProject_uptr;


    CMakeProject_uptr detect_cmake_project(InvocationInfo invocation);


    class BuildProject : public Project {
    public:
        BuildProject(const std::string& build_file);

        void load_env_if_needed();
        bool should_configure() override;
        InputStream_uptr configure(CommandLine command) override;
        InputStream_uptr make(std::string target="") override;

        std::map<std::string, std::string> get_build_env();
    private:
        std::string m_build_file;

        struct {
            CommandLine env_script;
            CommandLine make;
            CommandLine should_configure;
            CommandLine configure;
        } m_commands;
        bool m_env_loaded = false;
        Project_uptr m_base_project;
    };

    typedef std::unique_ptr<BuildProject> BuildProject_uptr;

    Project_uptr detect_project(InvocationInfo invocation);

    void block_signals();
    void unblock_signals();

    struct BlockSignalRaii {
        BlockSignalRaii( ){
            unblock_signals();
        }
        ~BlockSignalRaii() {
            block_signals();
        }
    };
}