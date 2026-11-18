// openkal.terminal on the line discipline's `termio'.
#include "common.h"

extern "C" {

// =================================================================== terminal
int kal_terminal_get_mode(kal_stream s, kal_uintptr* mode) {
    if (!mode) return kal_err_invalid;
    oks::ktermio t{};
    const auto r = oks::call(oks::nr::ioctl, oks::unpack(s.h), oks::TCGETA, oks::arg(&t));
    // Not a terminal: the systems answer ENOTTY, and a host that emulates them another number.
    if (r.failed()) return r.error == oks::EBADF ? kal_err_invalid : kal_err_not_supported;
    kal_uintptr m = KAL_TERM_SEQUENCES;
    if (t.c_lflag & oks::ICANON) m |= KAL_TERM_LINE_EDIT;
    if (t.c_lflag & oks::ECHO)   m |= KAL_TERM_ECHO;
    if (!(t.c_lflag & oks::ISIG) && !(t.c_iflag & oks::IXON)) m |= KAL_TERM_PASS_CONTROL;
    if (!(t.c_iflag & (oks::ICRNL | oks::INLCR | oks::IGNCR | oks::ISTRIP))) m |= KAL_TERM_PASS_INPUT;
    *mode = m;
    return kal_ok;
}
int kal_terminal_set_mode(kal_stream s, kal_uintptr m) {
    const int fd = oks::unpack(s.h);
    oks::ktermio t{};
    const auto r = oks::call(oks::nr::ioctl, fd, oks::TCGETA, oks::arg(&t));
    // Not a terminal: the systems answer ENOTTY, and a host that emulates them another number.
    if (r.failed()) return r.error == oks::EBADF ? kal_err_invalid : kal_err_not_supported;
    auto setb = [](oks::u16& w, unsigned b, bool on) { w = static_cast<oks::u16>(on ? (w | b) : (w & ~b)); };
    setb(t.c_lflag, oks::ICANON, m & KAL_TERM_LINE_EDIT);
    setb(t.c_lflag, oks::ECHO,   m & KAL_TERM_ECHO);
    setb(t.c_lflag, oks::ISIG,   !(m & KAL_TERM_PASS_CONTROL));
    setb(t.c_iflag, oks::IXON,   !(m & KAL_TERM_PASS_CONTROL));
    setb(t.c_iflag, oks::ICRNL | oks::INLCR | oks::IGNCR | oks::ISTRIP, !(m & KAL_TERM_PASS_INPUT));
    // Without line editing a read answers each byte as it arrives. With it, the
    // two positions are the end-of-file and end-of-line characters again.
    if (!(m & KAL_TERM_LINE_EDIT)) { t.c_cc[oks::VMIN] = 1; t.c_cc[oks::VTIME] = 0; }
    else { t.c_cc[oks::VMIN] = 4; t.c_cc[oks::VTIME] = 0; }
    const auto w = oks::call(oks::nr::ioctl, fd, oks::TCSETAW, oks::arg(&t));
    return w.failed() ? oks::translate(w.error) : kal_ok;
}
int kal_terminal_size(kal_stream s, kal_uintptr* cols, kal_uintptr* rows) {
    oks::kwinsize w{};
    if (oks::call(oks::nr::ioctl, oks::unpack(s.h), oks::TIOCGWINSZ, oks::arg(&w)).failed() || w.ws_col == 0) return kal_err_not_supported;
    if (cols) *cols = w.ws_col;
    if (rows) *rows = w.ws_row;
    return kal_ok;
}
kal_uintptr kal_terminal_props(kal_stream s) {
    const int fd = oks::unpack(s.h);
    oks::ktermio t{};
    if (fd < 0 || oks::call(oks::nr::ioctl, fd, oks::TCGETA, oks::arg(&t)).failed()) return 0;
    oks::kwinsize w{};
    const bool sized = !oks::call(oks::nr::ioctl, fd, oks::TIOCGWINSZ, oks::arg(&w)).failed() && w.ws_col != 0;
    return KAL_TERM_PROP_MODE | (sized ? KAL_TERM_PROP_SIZE : 0);
}

}  // extern "C"
