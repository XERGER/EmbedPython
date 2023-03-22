#include "../pch.h"
#include "TestQObject.h"

#include "Library/Python.h"
#include <QJsonArray>

class PythonForLinuxTest : public ::testing::Test
{
public:

protected:


};

TEST_F(PythonForLinuxTest, simple_python_script_runs)
{
	//Arrange
	const auto python = Python::createPythonForOs();

	//Act
	const auto result = python->runScript("return a * b;", { 3, 123 });

	//Assert
	EXPECT_EQ(result.toInt(), 3 * 123);
}

TEST_F(PythonForLinuxTest, simple_python_script_method_call_works)
{
	//Arrange
	const auto python = Python::createPythonForOs();
	python->addQObjectToPython("test", new TestQObject);

	//Act
	const auto result = python->runScript("def multiply(a,b):\n  return a*b;\n", "multiply", { 3, 123 });

	//Assert
	EXPECT_EQ(result.toInt(), 3 * 123);
}


TEST_F(PythonForLinuxTest, python_with_package_runs)
{
	//Arrange
	const auto python = Python::createPythonForOs();
	python->installPackage("psutil");

	//Act
	const auto result = python->runScript("import psutil \n psutil.pids();");

	//Assert
	EXPECT_GT(result.toJsonArray().size(), 3);
}


TEST_F(PythonForLinuxTest, python_can_call_qObject_func)
{
	//Arrange
	const auto python = Python::createPythonForOs();
	python->addQObjectToPython("test", new TestQObject);

	//Act
	const auto result = python->runScript("test.callThisFunctionFromPython('okay');");

	//Assert
	EXPECT_GT(result.toString(), "output is okay");
}
