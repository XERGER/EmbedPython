// PythonEdgeCasesTest.cpp
#include "../pch.h"
#include "Library/PythonRunner.h"
#include "Library/PythonEnvironment.h"
#include "Library/PythonResult.h"
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QFutureWatcher>
#include <QJsonDocument>

// Define error codes (replace these with actual values from your implementation)
constexpr int ERROR_CODE_TIMEOUT = 1;
constexpr int ERROR_CODE_CANCELLED = 2;

class PythonEdgeCasesTest : public ::testing::Test {
protected:
	void SetUp() override {
		pythonEnv = std::make_shared<PythonEnvironment>();
		runner = new PythonRunner( nullptr);
	}

	void TearDown() override {
		delete runner;
	}

	std::shared_ptr<PythonEnvironment> pythonEnv;
	PythonRunner* runner;
};

// Test case for a script with a syntax error
TEST_F(PythonEdgeCasesTest, SyntaxErrorInScript) {
	// Arrange
	QString script = "def func()\n    return 42"; // Missing colon
	QSignalSpy spy(runner, &PythonRunner::scriptFinished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);

	// Wait for the result
	ASSERT_TRUE(spy.wait(5000));

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_FALSE(result.isSuccess());
	EXPECT_GT(result.getErrorOutput().length(), 0);
	// Optionally, check if the error message contains specific keywords
	EXPECT_NE(result.getErrorOutput().indexOf("SyntaxError"), -1);
}

// Test case for a script that exceeds the timeout
// TEST_F(PythonEdgeCasesTest, AsyncScriptTimeout) {
// 	// Arrange
// 	QString script = "import time\ntime.sleep(5)\nprint('Done')";
// 	int timeout = 2000; // 1 second
// 	QFutureWatcher<PythonResult> watcher;
// 	QSignalSpy spy(&watcher, &QFutureWatcher<PythonResult>::finished);
// 
// 	// Act
// 	QFuture<PythonResult> future = runner->runScriptAsync("uniqueid", script, {}, timeout);
// 	watcher.setFuture(future);
// 
// 	// Wait for the signal or timeout
// 	ASSERT_FALSE(spy.wait(timeout + 100)); // Ensure it does not finish in time
// 
// 	// Get the result (if watcher completes, which shouldn't happen)
// 	PythonResult result = future.isFinished() ? future.result() : PythonResult(); // Empty if not finished
// 
// 	// Assert
// 	if (future.isFinished()) {
// 		EXPECT_FALSE(result.isSuccess());
// 		EXPECT_EQ(result.getErrorCode(), ERROR_CODE_TIMEOUT);
// 		EXPECT_NE(result.getErrorOutput().indexOf("timeout"), -1);
// 	}
// 	else {
// 		SUCCEED() << "Script correctly exceeded timeout without completing.";
// 	}
// }


// Test case for executing an empty script
TEST_F(PythonEdgeCasesTest, EmptyScriptExecution) {
	// Arrange
	QString script = "";
	QSignalSpy spy(runner, &PythonRunner::scriptFinished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);

	// Wait for the result
	ASSERT_TRUE(spy.wait(5000));

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_FALSE(result.isSuccess());
	EXPECT_TRUE(result.getOutput().isEmpty());
	EXPECT_FALSE(result.getErrorOutput().isEmpty());

}

// Test case for a script that returns None
TEST_F(PythonEdgeCasesTest, ScriptReturnsNone) {
	// Arrange
	QString script = "result = None\nprint(result)";
	QSignalSpy spy(runner, &PythonRunner::scriptFinished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);

	// Wait for the result
	ASSERT_TRUE(spy.wait(5000));

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().trimmed(), "None");
	//EXPECT_TRUE(result.getReturnValue().isEmpty());
}

// Test case for a script with a very large output
TEST_F(PythonEdgeCasesTest, ScriptWithLargeOutput) {
	// Arrange
	QString script = "print('A' * 1000000)"; // Print a string with 1 million 'A's

	QSignalSpy spy(runner, &PythonRunner::scriptFinished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);

	// Wait for the result
	ASSERT_TRUE(spy.wait(5000));

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().length(), 1000002);
	// Optionally, verify a portion of the output
	EXPECT_EQ(result.getOutput().left(10), QString("AAAAAAAAAA"));
}
// 
// TEST_F(PythonEdgeCasesTest, ScriptWithWorkingArguments) {
// 	// Arrange
// 	QString script = R"(
// import sys
// def add(a, b):
//     return a + b
// 
// a = sys.argv[1]
// b = sys.argv[2]
// result = add(a, b)
// print(result)
// )";
// 
// 	// Case 1: Positive integers
// 	{
// 		QVariantList arguments = { 10, 20 }; // Valid integers
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "30");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 2: Negative integers
// 	{
// 		QVariantList arguments = { -5, -15 }; // Valid negative integers
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "-20");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 3: Floating-point numbers
// 	{
// 		QVariantList arguments = { 1.5, 2.5 }; // Valid floating-point numbers
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "4.0");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 4: Mixing integers and floating-point numbers
// 	{
// 		QVariantList arguments = { 5, 3.2 }; // Mixed types
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "8.2");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 5: Large numbers
// 	{
// 		QVariantList arguments = { 1000000, 2000000 }; // Large integers
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "3000000");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 6: Zero values
// 	{
// 		QVariantList arguments = { 0, 0 }; // Both arguments are zero
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "0");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// }
//  
// TEST_F(PythonEdgeCasesTest, ScriptWithStringAndObjectArguments) {
// 	// Case 1: Concatenate strings
// 	{
// 		QString script = R"(
// import sys
// def concat_strings(a, b):
//     return a + b
// 
// a = sys.argv[1]
// b = sys.argv[2]
// result = concat_strings(a, b)
// print(result)
// )";
// 		QVariantList arguments = { "Hello", " World" };
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "Hello World");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 2: Repeat string
// 	{
// 		QString script = R"(
// import sys
// def repeat_string(s, n):
//     return s * int(n)
// 
// s = sys.argv[1]
// n = sys.argv[2]
// result = repeat_string(s, n)
// print(result)
// )";
// 		QVariantList arguments = { "Hi", 3 };
// 
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script, arguments);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "HiHiHi");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 3: Access dictionary-like object
// 	{
// 		QString script = R"(
// import sys
// def get_value_from_dict(d, key):
//     return d[key]
// 
// d = sys.argv[1]  # Access the dictionary directly
// key = sys.argv[2]      # Key to access
// result = get_value_from_dict(d, key)
// print(result)
// )";
// 		QVariantMap dictionary;
// 		dictionary["name"] = "John Doe";
// 		QVariantList arguments = { dictionary, "name" };  // Pass dictionary and key directly
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "John Doe");
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// 
// 	// Case 4: Join list of strings
// 	{
// 		QString script = R"(
// import sys
// def join_strings(strings):
//     return ", ".join(strings)
// 
// strings = arg1  # Access the list directly
// result = join_strings(strings)
// print(result)
// )";
// 		QStringList stringList = { "Apple", "Banana", "Cherry" };
// 		QVariantList arguments = { stringList };  // Pass list directly
// 		QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);
// 
// 		// Wait for the result
// 		future.waitForFinished();
// 
// 		// Get the result
// 		PythonResult result = future.result();
// 
// 		EXPECT_TRUE(result.isSuccess());
// 		EXPECT_EQ(result.getOutput().trimmed(), "Apple, Banana, Cherry");
// 
// 		EXPECT_EQ(result.getErrorOutput().length(), 0);
// 	}
// }

// Test case for executing a script with invalid arguments
TEST_F(PythonEdgeCasesTest, ScriptWithInvalidArguments) {
	// Arrange
	QString script = "def add(a, b):\n    return a + b\nresult = add(a, b)\nprint(result)";
	QVariantList arguments = { 10 }; // Missing one argument
	QSignalSpy spy(runner, &PythonRunner::scriptFinished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);

	// Wait for the result
	ASSERT_TRUE(spy.wait(5000));

	// Get the result
	PythonResult result = future.result();
	// Assert
	EXPECT_FALSE(result.isSuccess());
	EXPECT_GT(result.getErrorOutput().length(), 0);
}

// Test case for a script that raises an exception
TEST_F(PythonEdgeCasesTest, ScriptRaisesException) {
	// Arrange
	QString script = "raise ValueError('An error occurred')";

	QSignalSpy spy(runner, &PythonRunner::scriptFinished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);

	// Wait for the result
	ASSERT_TRUE(spy.wait(5000));

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_FALSE(result.isSuccess());
	EXPECT_GT(result.getErrorOutput().length(), 0);
	EXPECT_NE(result.getErrorOutput().indexOf("ValueError"), -1);
}
