#pragma once

#include <vector>
#include <string>
#include <functional>
namespace tea {
    enum ArgType {
        arg_none,
        arg_int,
        arg_string,
        arg_bool,
    };

    class ArgVar {
    public:
        ArgVar(){}
        ArgVar(const std::string& name, std::string value){
            this->name  = name;
            type        = arg_string;
            v_str       = value;
        }
        ArgVar(const std::string& name, int value){
            this->name  = name;
            type        = arg_int;
            v_int       = value;
        }
        ArgVar(const std::string& name, bool value){
            this->name  = name;
            type        = arg_bool;
            v_bool      = value;
        }
        std::string     name;
        ArgType         type    = arg_none;

        std::string     v_str;
        int             v_int   = 0;
        bool            v_bool  = false;
    };
    struct ArgDef {
        std::vector<std::string>    names;
        std::string                 help;
        std::vector<ArgType>        argTypes;
        std::function<void(std::vector<ArgVar>)> apply;
    };
    class ArgParse {
    public:
        ArgParse(const std::string& name, const std::string& description);

        void print_help();
        void add_arg(const ArgDef&);
        bool parse(int argc, char** argv);
    private:
        std::vector<ArgDef> mArgDefs;
    };
}