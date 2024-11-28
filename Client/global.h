#pragma once

#include <QtCore/qglobal.h>

#ifndef PythonClient_STATIC
# if defined(PythonClient_EXPORTS)
#  define PYTHHONCLIENT_EXPORT Q_DECL_EXPORT
# else
#  define PYTHHONCLIENT_EXPORT Q_DECL_IMPORT
# endif
#else
# define PYTHHONCLIENT_EXPORT
#endif
