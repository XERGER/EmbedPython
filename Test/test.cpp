#include "pch.h"
#include <QCoreApplication>
#include <QString>
#include <ostream>
#include <gtest/gtest.h>

QT_BEGIN_NAMESPACE
inline void PrintTo(const QString& qString, ::std::ostream* os)
{
	*os << qUtf8Printable(qString);
}
QT_END_NAMESPACE



void printString(QString const& output)
{
	std::cerr << output.toStdString();
}

int main(int argc, char** argv) {

	::testing::InitGoogleTest(&argc, argv);

	QCoreApplication app(argc, argv);

	return RUN_ALL_TESTS();
}