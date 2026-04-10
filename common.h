#ifndef ISP_COMMON_H
#define ISP_COMMON_H


template <typename _T>
static inline _T Max(_T a, _T b) {
    return (a > b ? a : b);
}

template <typename _T>
static inline _T Min(_T a, _T b) {
    return (a > b ? b : a);
}



#endif /* ISP_COMMON_H */