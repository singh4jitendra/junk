#ifndef SETSIGNAL_H_
#define SETSIGNAL_H_

int setsignal(int, void (*)(int), struct sigaction *, int, unsigned int, ...);

#endif
