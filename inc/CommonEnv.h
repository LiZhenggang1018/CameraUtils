#ifndef INC_COMMONENV_H_
#define INC_COMMONENV_H_

#include <string>

inline const std::string& EnvRootPath() {
    static const std::string path = PROJECT_ROOT_DIR;
    return path;
}

#if defined(__has_include)
#  if __has_include(<dlfcn.h>)
#    define CAMERAUTILS_HAS_DLFCN 1
#  else
#    define CAMERAUTILS_HAS_DLFCN 0
#  endif
#else
#  define CAMERAUTILS_HAS_DLFCN 0
#endif

#endif // INC_COMMONENV_H_
