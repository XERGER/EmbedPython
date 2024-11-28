#pragma once
#include <Python.h>
#include <QVariant>
#include <QJsonValue>



class DataConverter {
public:
	static PyObject* QVariantToPyObject(const QVariant& variant);
	static QJsonValue PyObjectToJson(PyObject* obj);
};
