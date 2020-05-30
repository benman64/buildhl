#include <map>

namespace tea {
    template<typename K, typename V>
    bool contains(const std::map<K, V>& m, const K& k) {
        return m.find(k) != m.end();
    }

    template<typename T, typename Container, typename Key>
    void set_if_contains(T& var, const Container& container, const Key& key) {
        typename Container::iterator it = container.find(key);
        if (it != container.end())
            var = *it;
    }

    template<typename List, typename Compare>
    void sort_list(List& list, Compare compare) {
        std::sort(list.begin(), list.end(), compare);
    }

    template<typename List>
    void sort_list(List& list) {
        std::sort(list.begin(), list.end());
    }
}