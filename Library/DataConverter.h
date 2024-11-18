#pragma once
#include <Python.h>  // Include Python headers without 'slots' macro interference

#include <QVariant>
#include <QJsonValue>



class DataConverter {
public:
	static PyObject* QVariantToPyObject(const QVariant& variant);
	static QJsonValue PyObjectToJson(PyObject* obj);
};
