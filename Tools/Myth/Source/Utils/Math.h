#ifndef MYTH_UTILS_MATH_H
#define MYTH_UTILS_MATH_H

#define FS_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define FS_MAX(x, y) (((x) > (y)) ? (x) : (y))

// Division towards ceiling rather than floor.
// x = dividend, y = divisor
#define FS_DIV(x, y) (((x) + (y) - 1) / (y))

#endif // !MYTH_UTILS_MATH_H
