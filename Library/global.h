#pragma once

#include <QtCore/qglobal.h>

#ifndef Library_STATIC
# if defined(Library_EXPORTS)
#  define LIBRARY_EXPORT Q_DECL_EXPORT
# else
#  define LIBRARY_EXPORT Q_DECL_IMPORT
# endif
#else
# define LIBRARY_EXPORT
#endif
