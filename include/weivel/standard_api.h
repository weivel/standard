#ifndef WEIVEL_STANDARD_API_H
#define WEIVEL_STANDARD_API_H

#if defined(_WIN32)
#if defined(WEIVEL_STANDARD_BUILDING_SHARED)
#define WEIVEL_STANDARD_API __declspec(dllexport)
#elif defined(WEIVEL_STANDARD_USING_SHARED)
#define WEIVEL_STANDARD_API __declspec(dllimport)
#else
#define WEIVEL_STANDARD_API
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define WEIVEL_STANDARD_API __attribute__((visibility("default")))
#else
#define WEIVEL_STANDARD_API
#endif

#endif
