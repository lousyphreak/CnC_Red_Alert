#ifndef NOINIT_H
#define NOINIT_H

#ifndef NOINITCLASS
#define NOINITCLASS
struct NoInitClass {
    void operator()(void) const {}
};
#endif

#endif
