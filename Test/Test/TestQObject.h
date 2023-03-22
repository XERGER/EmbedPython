#pragma once
#include <QObject>

class TestQObject : public QObject
{
	Q_OBJECT

public:
	QObject::QObject;

	int testValue = 42;

public slots:
	QString callThisFunctionFromPython(QString const& randomString) const
	{
		return "output is " + randomString;
	}

};