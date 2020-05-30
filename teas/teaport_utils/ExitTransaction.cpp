#include "ExitTransaction.hpp"

#include <map>
#include <mutex>

#include "Log.hpp"

namespace {
    struct Handlers : std::map<int, std::function<void()>>, std::mutex {
        
        void run_all() {
            std::unique_lock<Handlers> lock(*this);
            // order matters so we run in reverse order
            auto it = rbegin();
            auto last = rend();
            while(it != last) {
                try {
                    it->second();
                } catch(std::exception& e) {
                    tea::log_message("E1018", e.what());
                }
            }
            clear();
        }
    };
    static Handlers gHandlers;
    int next_id() {
        static std::mutex mutex;
        static int cur_id = 0;
        std::unique_lock<std::mutex> lock(mutex);
        ++cur_id;
        return cur_id;
    }
}

namespace tea {
    void install_atexit() {
        static bool did_init = false;
        if(did_init)
            return;
        did_init = true;
        atexit([] {
            gHandlers.run_all();
        });
    }
    int add_atexit(const std::function<void()>& func) {
        install_atexit();
        std::unique_lock<Handlers> lock(gHandlers);
        int handler_id = next_id();
        gHandlers[handler_id] = func;
        return handler_id;
    }

    bool remove_atexit(int exit_id) {
        std::unique_lock<Handlers> lock(gHandlers);
        return gHandlers.erase(exit_id) > 0;
    }
}