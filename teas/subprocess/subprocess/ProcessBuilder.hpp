#pragma once

#include <initializer_list>
#include <vector>
#include <string>

#include "pipe.hpp"
#include "PipeVar.hpp"

namespace subprocess {

    /*  For reference:
        0 - cin
        1 - cout
        2 - cerr
    */
    /*  possible names:
            PopenOptions,
            RunDef,
            RunConfig
            RunOptions
    */

    struct RunOptions {
        /** Set to true for subprocess::run() to throw exception. Ignored when
            using Popen directly.
        */
        bool        check   = false;
        /** Option for cin, data to pipe to cin.  or created handle to use.

            if a pipe handle is used it will be made inheritable automatically
            when process is created and closed on the parents end.
        */
        PipeVar     cin     = PipeOption::inherit;
        /** Option for cout, or handle to use.

            if a pipe handle is used it will be made inheritable automatically
            when process is created and closed on the parents end.
        */
        PipeVar     cout    = PipeOption::inherit;
        /** Option for cout, or handle to use.

            if a pipe handle is used it will be made inheritable automatically
            when process is created and closed on the parents end.
        */
        PipeVar     cerr    = PipeOption::inherit;

        /** current working directory for new process to use */
        std::string cwd;
        /** If empty inherits from current process */
        EnvMap      env;

        /** Timeout in seconds. Raise TimeoutExpired.

            Only available if you use subprocess_run
        */
        double timeout  = -1;
    };
    class ProcessBuilder;
    /** Active running process.

        Similar design of subprocess.Popen. In c++ I didn't like
    */
    struct Popen {
    public:
        /** Initialized as empty and invalid */
        Popen(){}
        /** Starts command with specified options */
        Popen(CommandLine command, const RunOptions& options);
        /** Starts command with specified options */
        Popen(CommandLine command, RunOptions&& options);
        Popen(const Popen&)=delete;
        Popen& operator=(const Popen&)=delete;

        Popen(Popen&&);
        Popen& operator=(Popen&&);

        /** Waits for process, Closes pipes and destroys any handles.*/
        ~Popen();

        /** Write to this stream to send data to the process. This class holds
            the ownership and will call pipe_close().
        */
        PipeHandle  cin       = kBadPipeValue;
        /** Read from this stream to get output of process. This class holds
            the ownership and will call pipe_close().
        */
        PipeHandle  cout      = kBadPipeValue;
        /** Read from this stream to get cerr output of process. This class holds
            the ownership and will call pipe_close().
        */
        PipeHandle  cerr      = kBadPipeValue;


        pid_t       pid         = 0;
        /** The exit value of the process. Valid once process is completed */
        int         returncode  = kBadReturnCode;
        CommandLine args;

        /** calls pipe_ignore_and_close on cout */
        void ignore_cout() { pipe_ignore_and_close(cout); cout = kBadPipeValue; }
        /** calls pipe_ignore_and_close on cerr */
        void ignore_cerr() { pipe_ignore_and_close(cerr); cerr = kBadPipeValue; }
        /** calls pipe_ignore_and_close on cout, cerr if open */
        void ignore_output() { ignore_cout(); ignore_cerr(); }
        /** @return true if terminated.

            @throw OSError  If os specific error has been encountered.
        */
        bool poll();
        /** Waits for process to finish.

            If stdout or stderr is not read from, the child process may be
            blocked when it tries to write to the respective streams. You
            must ensure you continue to read from stdout/stderr. Call
            ignore_output() to spawn threads to ignore the output preventing a
            deadlock. You can also troll the child by closing your end.

            @param timeout  timeout in seconds. Raises TimeoutExpired on
                            timeout. NOT IMPLEMENTED, WILL WAIT FOREVER.
            @return returncode

            @throw OSError          If there was an os level error call OS API's
            @throw TimeoutExpired   If the timeout has expired.
        */
        int wait(double timeout=-1);
        /** Send the signal to the process.

            On windows SIGTERM is an alias for terminate()
        */
        bool send_signal(int signal);
        /** Sends SIGTERM, on windows calls TerminateProcess() */
        bool terminate();
        /** sends SIGKILL on linux, alias for terminate() on windows. */
        bool kill();

        /** Destructs the object and initializes to basic state */
        void close();
        /** Closes the cin pipe */
        void close_cin() {
            if (cin != kBadPipeValue) {
                pipe_close(cin);
                cin = kBadPipeValue;
            }
        }
        friend ProcessBuilder;
    private:
        void init(CommandLine& command, RunOptions& options);

#ifdef _WIN32
        PROCESS_INFORMATION process_info;
#endif
    };



    /** This class does the bulk of the work for starting a process. It is the
        most customizable and hence the most complex.

        @note Undecided if this should be public API or private.
    */
    class ProcessBuilder {
    public:
        std::vector<PipeHandle> child_close_pipes;

        PipeHandle cin_pipe       = kBadPipeValue;
        PipeHandle cout_pipe      = kBadPipeValue;
        PipeHandle cerr_pipe      = kBadPipeValue;


        PipeOption cin_option     = PipeOption::inherit;
        PipeOption cout_option    = PipeOption::inherit;
        PipeOption cerr_option    = PipeOption::inherit;

        /** If empty inherits from current process */
        EnvMap      env;
        std::string cwd;
        CommandLine command;

        std::string windows_command();
        std::string windows_args();
        std::string windows_args(const CommandLine& command);

        Popen run() {
            return run_command(this->command);
        }
        Popen run_command(const CommandLine& command);
    };

    /** If you have stuff to pipe this will run the process to completion.

        This will read stdout/stderr if they exist and store in cout, cerr.

        @param popen    An already running process.
        @param check    if true will throw CalledProcessException if process
                        returns non-zero exit code.

        @returns a Filled out CompletedProcess.

        @throw  CalledProcessError if check=true and process returned
                non-zero error code
        @throw  TimeoutExpired if subprocess does not finish by timeout
                seconds.
        @throw  OSError for os level exceptions.
        @throw  std::runtime_error for various errors.
        @return a CompletedProcess once the command has finished.
    */
    CompletedProcess run(Popen& popen, bool check=false);
    /** Run a command blocking until completion.

        see subprocess::run(Popen&,bool) for more details about exceptions.

        @param command  The command to run. First element must be executable.
        @param options  Options specifying how to run the command.

        @return CompletedProcess containing details about execution.
    */
    CompletedProcess run(CommandLine command, RunOptions options={});

    /** Helper class to construct RunOptions with minimal typing. */
    struct RunBuilder {
        RunOptions options;
        CommandLine command;

        RunBuilder(){}
        /** Constructs builder with cmd as command to run */
        RunBuilder(CommandLine cmd) : command(cmd){}
        /** Constructs builder with command to run */
        RunBuilder(std::initializer_list<std::string> command) : command(command){}
        /** Only for run(), throws exception if command returns non-zero exit code */
        RunBuilder& check(bool ch) {options.check = ch; return *this;}
        /** Set the cin option. Could be PipeOption, input handle, std::string with data to pass. */
        RunBuilder& cin(const PipeVar& cin) {options.cin = cin; return *this;}
        /** Sets the cout option. Could be a PipeOption, output handle */
        RunBuilder& cout(const PipeVar& cout) {options.cout = cout; return *this;}
        /** Sets the cerr option. Could be a PipeOption, output handle */
        RunBuilder& cerr(const PipeVar& cerr) {options.cerr = cerr; return *this;}
        /** Sets the current working directory to use for subprocess */
        RunBuilder& cwd(std::string cwd) {options.cwd = cwd; return *this;}
        /** Sets the environment to use. Default is current environment if unset */
        RunBuilder& env(const EnvMap& env) {options.env = env; return *this;}
        /** Timeout to use for run() invocation only. */
        RunBuilder& timeout(double timeout) {options.timeout = timeout; return *this;}

        operator RunOptions() const {return options;}

        /** Runs the command already configured.

            see subprocess::run() for more details.
        */
        CompletedProcess run() {return subprocess::run(command, options);}
        /** Creates a Popen object running the process asynchronously

            @return Popen object with  current configuration.
        */
        Popen popen() { return Popen(command, options); }
    };

    /** @return seconds went by from some origin monotonically increasing. */
    double monotonic_seconds();
    /** Sleep for a number of seconds.

        @param seconds  The number of seconds to sleep for.

        @return how many seconds have been slept.
    */
    double sleep_seconds(double seconds);

    class StopWatch {
    public:
        StopWatch() { start(); }

        void start() { mStart = monotonic_seconds(); }
        double seconds() const { return monotonic_seconds() - mStart; }
    private:
        double mStart;
    };
}