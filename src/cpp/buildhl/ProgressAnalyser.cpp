#include "ProgressAnalyser.hpp"

#include <subprocess.hpp>
#include <regex>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

using subprocess::monotonic_seconds;
namespace buildhl {
    double ProgressGraph::eta() const {
        if (size() <= 2)
            return 0;
        double speed = this->speed();
        if (speed <= 0.000001)
            return 0;
        auto current = (*this)[size()-1];
        double done_estimate = speed*(monotonic_seconds() - current.x) + current.y;
        double remaining = m_total - done_estimate;
        double eta = remaining / speed;
        return eta;
    }

    double ProgressGraph::complete(double amount) {
        if (amount == get_complete())
            return get_complete();
        Point point {monotonic_seconds(), amount};

        if (amount < get_complete()) {
            // something weird happenned, lets start over
            clear();
        }
        push_back(point);
        keep(256);
        return get_complete();
    }

    Progress parse_progress(const std::string& line) {
        std::regex reg("(\\d+)\\s*/\\s*(\\d+)");
        std::smatch match;
        if(std::regex_search(line, match, reg)) {
            Progress progress;
            progress.complete   = std::stod(match[1]);
            progress.total      = std::stod(match[2]);
            // if this is the case probably it's not a progress indicator
            if (progress.complete > progress.total)
                return {};
            return progress;
        }
        return {};
    }
    std::string repeat(const std::string& str, int count) {
        std::string result;
        for (int i = 0; i < count; ++i) {
            result += str;
        }
        return result;
    }
    std::string render_progress(double progress, int width) {
        progress = std::min(1.0, progress);
        progress = std::max(0.0, progress);

        std::string line = "[";
        int total_spots = width -2;
        int spots = total_spots*progress;
        const char* sub = "|-=";
        int sub_spot = (total_spots*progress - spots)*strlen(sub);

        line += repeat("#", spots);
        if (sub_spot) {
            line += sub[sub_spot];
        }
        line += repeat(" ", width - line.size() -1) + "]";
        return line;
    }

    std::string left_pad(std::string var, int length, const std::string& what) {
        while ((int)var.size() < length) {
            var = what + var;
        }
        return var;
    }
}