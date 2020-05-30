#include <cstdint>
#include <vector>
#include <string>

namespace buildhl {
    struct Point {
        double x;
        double y;

        Point operator-(const Point& other) const {
            return {x - other.x, y - other.y};
        }
    };

    class Graph {
    public:
        void push_back(Point point) {
            m_points.push_back(point);
        }

        double speed() const {
            if (m_points.size() <= 1)
                return 0;
            int last = m_points.size()-1;
            Point diff = m_points[last] - m_points[0];
            return diff.y/diff.x;
        }

        void clear() { m_points.clear(); }
        void keep(int size) {
            if ((int)m_points.size() <= size)
                return;
            int last = m_points.size() - size;
            m_points.erase(m_points.begin(), m_points.begin() + last);
        }

        size_t size() const {
            return m_points.size();
        }

        Point& operator[](int index) {
            return m_points[index];
        }

        Point operator[](int index) const {
            return m_points[index];
        }
    private:
        std::vector<Point> m_points;
    };

    class ProgressGraph: public Graph {
    public:
        ProgressGraph(double total=1) {
            m_total = total;
        }

        double progress() const {
            if (size() == 0)
                return 0;
            return (*this)[size()-1].y / m_total;
        }
        double eta() const;

        double get_complete() const {
            if (size() == 0)
                return 0;
            return (*this)[size()-1].y;
        }

        double complete(double amount);
    private:
        double m_total;
    };

    struct Progress {
        double complete = 0;
        double total    = 0;

        operator double() const {
            if (!*this)
                return 0;
            return complete/total;
        }

        explicit operator bool() const {
            return total > 0 && complete > 0;
        }
    };

    Progress parse_progress(const std::string& line);
    std::string render_progress(double progress, int width);
    std::string left_pad(std::string var, int length, const std::string& what=" ");
}