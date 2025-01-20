// #include "DataConverter.h"
// #include <QJsonArray>
// #include <QJsonObject>
// #include <QMetaType>
// #include <QMetaProperty>
// 
// 
// PyObject* DataConverter::QVariantToPyObject(const QVariant& variant) {
// 	switch (variant.metaType().id()) { // Use metaType().id() instead of type()
// 	case QMetaType::LongLong:
// 	case QMetaType::Int:
// 		return PyLong_FromLong(variant.toInt());
// 	case QMetaType::Double:
// 		return PyFloat_FromDouble(variant.toDouble());
// 	case QMetaType::QString:
// 		return PyUnicode_FromString(variant.toString().toUtf8().constData());
// 	case QMetaType::Bool:
// 		return variant.toBool() ? Py_True : Py_False;
// 	case QMetaType::QVariantList: {
// 		PyObject* listObj = PyList_New(0);
// 		for (const QVariant& item : variant.toList()) {
// 			PyObject* pyItem = QVariantToPyObject(item);
// 			if (!pyItem) {
// 				Py_DECREF(listObj);
// 				return nullptr;
// 			}
// 			PyList_Append(listObj, pyItem);
// 			Py_DECREF(pyItem);
// 		}
// 		return listObj;
// 	}
// 
// 	case QMetaType::QStringList: {
// 		QStringList list = variant.toStringList();
// 		PyObject* listObj = PyList_New(list.size());
// 		if (!listObj) return nullptr;
// 
// 		for (int i = 0; i < list.size(); ++i) {
// 			PyObject* pyItem = PyUnicode_FromString(list[i].toUtf8().constData());
// 			if (!pyItem) {
// 				Py_DECREF(listObj);
// 				return nullptr;
// 			}
// 			PyList_SET_ITEM(listObj, i, pyItem);  // Steals reference
// 		}
// 		return listObj;
// 	}
// 
// 	case QMetaType::QVariantMap: {
// 		PyObject* dictObj = PyDict_New();
// 		QVariantMap map = variant.toMap();
// 		for (auto it = map.begin(); it != map.end(); ++it) {
// 			PyObject* key = PyUnicode_FromString(it.key().toUtf8().constData());
// 			PyObject* value = QVariantToPyObject(it.value());
// 			if (!key || !value) {
// 				Py_XDECREF(key);
// 				Py_XDECREF(value);
// 				Py_DECREF(dictObj);
// 				return nullptr;
// 			}
// 			PyDict_SetItem(dictObj, key, value);
// 			Py_DECREF(key);
// 			Py_DECREF(value);
// 		}
// 		return dictObj;
// 	}
// 	case QMetaType::QObjectStar: {
// 		QObject* obj = variant.value<QObject*>();
// 		if (!obj) {
// 			Py_INCREF(Py_None);
// 			return Py_None;
// 		}
// 
// 		// Convert QObject properties to Python dictionary
// 		PyObject* dictObj = PyDict_New();
// 		if (!dictObj) return nullptr;
// 
// 		const QMetaObject* metaObj = obj->metaObject();
// 		for (int i = 0; i < metaObj->propertyCount(); ++i) {
// 			QMetaProperty prop = metaObj->property(i);
// 			QString propName = prop.name();
// 			QVariant propValue = obj->property(prop.name());
// 
// 			PyObject* key = PyUnicode_FromString(propName.toUtf8().constData());
// 			PyObject* value = QVariantToPyObject(propValue);
// 			if (!key || !value) {
// 				Py_XDECREF(key);
// 				Py_XDECREF(value);
// 				Py_DECREF(dictObj);
// 				return nullptr;
// 			}
// 			PyDict_SetItem(dictObj, key, value);
// 			Py_DECREF(key);
// 			Py_DECREF(value);
// 		}
// 
// 		return dictObj;
// 	}
// 	default:
// 		break;
// 	}
// 	Py_RETURN_NONE; // Return Py_None instead of a nullptr.
// }
// 
// QJsonValue DataConverter::PyObjectToJson(PyObject* obj) {
// 	if (PyLong_Check(obj)) {
// 		return QJsonValue(static_cast<qint64>(PyLong_AsLong(obj))); // Explicit cast to resolve ambiguity.
// 	}
// 	else if (PyFloat_Check(obj)) {
// 		return QJsonValue(PyFloat_AsDouble(obj));
// 	}
// 	else if (PyUnicode_Check(obj)) {
// 		return QJsonValue(QString::fromUtf8(PyUnicode_AsUTF8(obj)));
// 	}
// 	else if (PyBool_Check(obj)) {
// 		return QJsonValue(obj == Py_True);
// 	}
// 	else if (PyList_Check(obj)) {
// 		QJsonArray array;
// 		Py_ssize_t size = PyList_Size(obj);
// 		for (Py_ssize_t i = 0; i < size; ++i) {
// 			PyObject* item = PyList_GetItem(obj, i); // Borrowed reference.
// 			array.append(PyObjectToJson(item));
// 		}
// 		return QJsonValue(array);
// 	}
// 	else if (PyDict_Check(obj)) {
// 		QJsonObject jsonObj;
// 		PyObject* key;
// 		PyObject* value;
// 		Py_ssize_t pos = 0;
// 		while (PyDict_Next(obj, &pos, &key, &value)) {
// 			if (PyUnicode_Check(key)) {
// 				QString keyStr = QString::fromUtf8(PyUnicode_AsUTF8(key));
// 				jsonObj[keyStr] = PyObjectToJson(value);
// 			}
// 		}
// 		return QJsonValue(jsonObj);
// 	}
// 	return QJsonValue();
// }
