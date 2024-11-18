// PythonRunnerTest.cpp
#include "../pch.h"
#include "Library/PythonRunner.h"
#include "Library/PythonEnvironment.h"
#include "Library/PythonResult.h"
#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QFutureWatcher>

class PythonRunnerTest : public ::testing::Test {
protected:
	void SetUp() override {
		pythonEnv = std::make_shared<PythonEnvironment>();
		runner = new PythonRunner(pythonEnv, nullptr);
	}

	void TearDown() override {
		delete runner;
	}

	std::shared_ptr<PythonEnvironment> pythonEnv;
	PythonRunner* runner;
};

TEST_F(PythonRunnerTest, SynchronousExecutionSuccess) {
	// Arrange
	QString script = "result = 10 + 20\nprint(result)";

	// Act
	PythonResult result = runner->runScript(script);

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().trimmed(), "30");
}

TEST_F(PythonRunnerTest, SynchronousExecutionFailure) {
	// Arrange
	QString script = "raise Exception('Test error')";

	// Act
	PythonResult result = runner->runScript(script);

	// Assert
	EXPECT_FALSE(result.isSuccess());
	EXPECT_GT(result.getErrorOutput().length(), 0);
}

TEST_F(PythonRunnerTest, AsynchronousExecutionSuccess) {
	// Arrange
	QString script = "import time\ntime.sleep(1)\nprint('Async Done')";
	QFutureWatcher<PythonResult> watcher;
	QSignalSpy spy(&watcher, &QFutureWatcher<PythonResult>::finished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync(script);
	watcher.setFuture(future);

	// Wait for the signal
	ASSERT_TRUE(spy.wait(3000)); // Wait up to 3 seconds

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().trimmed(), "Async Done");
}

TEST_F(PythonRunnerTest, AsynchronousExecutionWithCallback) {
	// Arrange
	QString script = "result = 5 * 5\nprint(result)";
	QFutureWatcher<PythonResult> watcher;
	QSignalSpy spy(&watcher, &QFutureWatcher<PythonResult>::finished);

	// Act
	QFuture<PythonResult> future = runner->runScriptAsync(script);
	watcher.setFuture(future);

	// Wait for the signal
	ASSERT_TRUE(spy.wait(2000)); // Wait up to 2 seconds

	// Get the result
	PythonResult result = future.result();

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().trimmed(), "25");
}

TEST_F(PythonRunnerTest, ExecutionTimeMeasurement) {
	// Arrange
	QString script = "import time\ntime.sleep(2)\nprint('Done')";
	int timeout = 3000; // 3 seconds

	// Act
	PythonResult result = runner->runScript(script, {}, timeout);

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_GE(result.getExecutionTime(), 2000);
}
