#ifndef RA_CONIO_WRAPPER_H
#define RA_CONIO_WRAPPER_H

#include <cstdarg>
#include <cstdlib>
#include <cstdio>

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

inline int cprintf(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const int result = std::vprintf(format, arguments);
    va_end(arguments);
    return result;
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
namespace ra_conio {

inline termios& original_termios()
{
    static termios state{};
    return state;
}

inline bool& raw_mode_active()
{
    static bool active = false;
    return active;
}

inline void restore_terminal_mode()
{
    if (raw_mode_active()) {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios());
        raw_mode_active() = false;
    }
}

inline bool enable_raw_mode()
{
    if (raw_mode_active()) {
        return true;
    }

    termios raw{};
    if (tcgetattr(STDIN_FILENO, &original_termios()) != 0) {
        return false;
    }

    raw = original_termios();
    raw.c_lflag &= static_cast<unsigned>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }

    raw_mode_active() = true;
    std::atexit(restore_terminal_mode);
    return true;
}

} // namespace ra_conio

inline int getch()
{
    if (!ra_conio::enable_raw_mode()) {
        return std::getchar();
    }

    unsigned char character = 0;
    if (read(STDIN_FILENO, &character, 1) == 1) {
        return character;
    }

    return EOF;
}

inline int kbhit()
{
    if (!ra_conio::enable_raw_mode()) {
        return 0;
    }

    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(STDIN_FILENO, &descriptors);

    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    return select(STDIN_FILENO + 1, &descriptors, nullptr, nullptr, &timeout) > 0 ? 1 : 0;
}
#else
inline int getch()
{
    return std::getchar();
}

inline int kbhit()
{
    return 0;
}
#endif

#endif
