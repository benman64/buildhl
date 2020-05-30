#include "iostream.hpp"

namespace ios {
bout_ostream::bout_ostream() {
    mFile = stdout;
}

std::streamsize bout_ostream::write(const void*data, std::streamsize size) {
    return fwrite(data, 1, size, mFile);
}
void bout_ostream::flush() { fflush(mFile); }

bin_istream::bin_istream() {
    mFile = stdin;
}

std::streamsize bin_istream::read(void *data, std::streamsize size) {
    return fread(data, 1, size, mFile);
}


berr_ostream::berr_ostream() {
    mFile = stderr;
}

std::streamsize berr_ostream::write(const void*data, std::streamsize size) {
    return fwrite(data, 1, size, mFile);
}

bout_ostream bout;
bin_istream bin;
berr_ostream berr;

}
