#include "ArgParse.hpp"

#include <map>
namespace tea {
    void ArgParse::add_arg(const ArgDef& def) {
        mArgDefs.push_back(def);
    }
    void ArgParse::print_help() {

    }

    bool ArgParse::parse(int argc, char** argv) {
        std::map<std::string, int> arg_def_map;
        for (unsigned int i = 0; i < mArgDefs.size(); ++i) {
            ArgDef& def = mArgDefs[i];
            for (const std::string& name : def.names) {
                arg_def_map[name] = i;
            }
        }
        std::vector<ArgVar> mainParams;

        for(int i = 0; i < argc; ++i) {
            if (argv[i][0] != '-') {
                mainParams.push_back(ArgVar("", argv[i]));
                continue;
            }
            if (arg_def_map.find(argv[i]) == arg_def_map.end())
                continue;
            int iDef = arg_def_map[argv[i]];
            ArgDef def = mArgDefs[iDef];

            if (def.argTypes.size() == 0) {
                def.apply({});
            } else {
                std::string name = argv[i];
                std::vector<ArgVar> args;
                ++i;
                for (unsigned int iParam = 0; iParam < def.argTypes.size() && i < argc; ++iParam, ++i) {
                    ArgVar var;
                    std::string value = argv[i];
                    switch (def.argTypes[iParam]) {
                    case arg_string:
                        var = ArgVar(name, argv[i]);
                        break;
                    case arg_int:
                        var = ArgVar(name, atoi(argv[i]));
                        break;
                    case arg_bool:
                        if (value == "1" || value == "true")
                            var = ArgVar(name, true);
                        else
                            var = ArgVar(name, false);
                        break;
                    case arg_none:
                        var = ArgVar(name, false);
                        break;
                    }
                    args.push_back(var);
                }
                def.apply(args);
            }
        }
        return false;
    }
}