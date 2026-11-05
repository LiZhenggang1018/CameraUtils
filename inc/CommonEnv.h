#ifndef INC_COMMONENV_H_
#define INC_COMMONENV_H_

#include <string>

inline const std::string ENV_ROOT_PATH = PROJECT_ROOT_DIR; // 必须使用此宏来获取项目根目录，确保在不同环境下路径正确

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
