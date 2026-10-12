/*
 * @Description: macro files to disallow copy or move segmantics for class
 */

#ifndef NOCOPYABLEMACRO_H
#define NOCOPYABLEMACRO_H

// 禁止拷贝与移动（在类体内使用：DISALLOW_COPY_AND_MOVE(TypeName)）
#ifndef DISALLOW_COPY_AND_MOVE
#define DISALLOW_COPY_AND_MOVE(TypeName)           \
    TypeName(const TypeName&) = delete;            \
    TypeName& operator=(const TypeName&) = delete; \
    TypeName(TypeName&&) = delete;                 \
    TypeName& operator=(TypeName&&) = delete;
#endif

#ifndef DISALLOW_COPY
#define DISALLOW_COPY(className)          \
    className(const className&) = delete; \
    className& operator=(const className&) = delete
#endif

#ifndef DISALLOW_MOVE
#define DISALLOW_MOVE(className)     \
    className(className&&) = delete; \
    className& operator=(className&&) = delete
#endif

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;    \
    TypeName& operator=(const TypeName&) = delete
#endif

#ifndef DISALLOW_MOVE_AND_ASSIGN
#define DISALLOW_MOVE_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&&) = delete;   \
    TypeName& operator=(const TypeName&&) = delete
#endif

#ifndef DISALLOW_ASSIGN
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete
#endif

#ifndef DISALLOW_MOVE_ASSIGN
#define DISALLOW_MOVE_ASSIGN(TypeName) TypeName& operator=(const TypeName&&) = delete
#endif

#endif // NOCOPYABLEMACRO_H