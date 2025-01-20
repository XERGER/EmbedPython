// PythonPackagesTest.cpp
#include "../pch.h"
#include <QSignalSpy>
#include "Library/PythonEnvironment.h"
#include "Library/PythonRunner.h"
#include "Library/PythonResult.h"
#include <gtest/gtest.h>

std::ostream& operator<<(std::ostream& os, const QString& str) {
	os << str.toStdString();
	return os;
}

class PythonPackagesTest : public ::testing::Test {
protected:
	void SetUp() override {
		pythonEnv = std::make_shared<PythonEnvironment>();

		runner = new PythonRunner(nullptr);
	}

	void TearDown() override {
		delete runner;
	}

	std::shared_ptr<PythonEnvironment> pythonEnv;
	PythonRunner* runner;
};

bool waitForSignal(QSignalSpy& spy, int timeout = 30000) { // 2 minutes by default
	// Wait for the signal
	return spy.wait(timeout);
}


TEST_F(PythonPackagesTest, InstallRunUninstallRequests) {
	// Arrange
	QString package = "requests";
	QSignalSpy installSpy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
	QSignalSpy uninstallSpy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
	QSignalSpy scriptSpy(runner, &PythonRunner::scriptFinished);
	QString installExecutionId = QUuid::createUuid().toString();
	QString uninstallExecutionId = QUuid::createUuid().toString();

	// Step 1: Uninstall requests if already installed
	{
		// Act
		bool signalCaptured = false;
		PythonResult uninstallResult;

		QObject::connect(pythonEnv.get(), &PythonEnvironment::packageOperationFinished,
			[&signalCaptured, &uninstallResult](QString const& executionId, OperationType operation, const QString& packageName, const PythonResult& result) {
				uninstallResult = result;
				signalCaptured = true;
		});
		pythonEnv->uninstallPackage(uninstallExecutionId, package);

		// Assert
		EXPECT_TRUE(signalCaptured);
		ASSERT_TRUE(uninstallResult.isSuccess()) << "Failed to uninstall package 'requests': " << uninstallResult.getErrorOutput();
		EXPECT_EQ(uninstallResult.getExecutionId(), uninstallExecutionId);
		EXPECT_TRUE(uninstallResult.getOutput().contains("Uninstalled package: requests"));
		EXPECT_EQ(uninstallResult.getErrorOutput(), "");
		EXPECT_EQ(uninstallResult.getErrorCode(), 0);
		EXPECT_GT(uninstallResult.getExecutionTime(), 0);

		qDebug() << "Uninstall package 'requests' completed successfully.";
	}

	// Step 2: Install requests
	{
		// Act
	
		bool signalCaptured = false;
		PythonResult installResult;

		QObject::connect(pythonEnv.get(), &PythonEnvironment::packageOperationFinished,
			[&signalCaptured, &installResult](QString const& executionId, OperationType operation, const QString& packageName, const PythonResult& result) {
				installResult = result;
				qDebug() << "Install result: " << result.toJson() << signalCaptured;
				signalCaptured = true;
		});

		pythonEnv->installPackage(installExecutionId, package);

		// Assert
		if (!signalCaptured) //#TODO get rid of ifs ...
		{
			ASSERT_TRUE(waitForSignal(installSpy)) << "Did not receive installPackage signal within timeout.";

			ASSERT_GT(installSpy.count(), 0) << "No signals captured for installPackage.";

			if (!signalCaptured)
			{
				QList<QVariant> arguments = installSpy.takeFirst();
				installResult = arguments.at(2).value<PythonResult>();
			}


		}

		qDebug() << "Install result: " << installResult.toJson() << signalCaptured;
		ASSERT_TRUE(installResult.isSuccess()) << "Failed to install package 'requests': " << installResult.getErrorOutput();
		EXPECT_EQ(installResult.getExecutionId(), installExecutionId);
	//	EXPECT_TRUE(installResult.getOutput().contains("Successfully installed"));
		EXPECT_EQ(installResult.getErrorOutput(), "");
		EXPECT_EQ(installResult.getErrorCode(), 0);
		EXPECT_GT(installResult.getExecutionTime(), 0);

		qDebug() << "Install package 'requests' completed successfully.";
	}

	// Step 3: Verify requests installation
	{
		// Act
		bool isInstalled = pythonEnv->isPackageInstalled(package);

		// Assert
		EXPECT_TRUE(isInstalled) << "'requests' should be installed.";
	}

	// Step 4: Run script using requests
	{
		// Arrange
		QString script =
			"import requests\n"
			"def fetch_status(url):\n"
			"    try:\n"
			"        response = requests.get(url)\n"
			"        return response.status_code\n"
			"    except Exception as e:\n"
			"        return str(e)\n"
			"result = fetch_status('https://httpbin.org/get')\n"
			"print(result)\n";
		QVariantList arguments;

		// Act
		QFuture<PythonResult> future = runner->runScriptAsync(installExecutionId, script);

		// Assert
		ASSERT_TRUE(scriptSpy.wait(30000)) << "Did not receive script execution signal within timeout.";
		ASSERT_GT(scriptSpy.count(), 0) << "No signals captured for scriptFinished.";

		PythonResult scriptResult = future.result();
		EXPECT_EQ(scriptResult.getExecutionId(), installExecutionId);
		EXPECT_TRUE(scriptResult.isSuccess()) << "Script execution failed: " << scriptResult.getErrorOutput();
		EXPECT_EQ(scriptResult.getOutput().trimmed(), "200") << "Expected HTTP status code to be 200.";
		EXPECT_EQ(scriptResult.getErrorOutput(), "");
		EXPECT_EQ(scriptResult.getErrorCode(), 0);
		EXPECT_GT(scriptResult.getExecutionTime(), 0);

		qDebug() << "Script executed successfully:" << scriptResult.toJson();
	}

	// Step 5: Uninstall requests
	{
		// Act
		pythonEnv->uninstallPackage(uninstallExecutionId, package);

		// Assert
		ASSERT_TRUE(waitForSignal(uninstallSpy)) << "Did not receive uninstallPackage signal within timeout.";
		ASSERT_GT(uninstallSpy.count(), 0) << "No signals captured for uninstallPackage.";

		QList<QVariant> arguments = uninstallSpy.takeFirst();
		PythonResult uninstallResult = arguments.at(2).value<PythonResult>();
		ASSERT_TRUE(uninstallResult.isSuccess()) << "Failed to uninstall package 'requests': " << uninstallResult.getErrorOutput();
		EXPECT_EQ(uninstallResult.getExecutionId(), uninstallExecutionId);
		EXPECT_TRUE(uninstallResult.getOutput().contains("Uninstalled package: requests"));
		EXPECT_EQ(uninstallResult.getErrorOutput(), "");
		EXPECT_EQ(uninstallResult.getErrorCode(), 0);
		EXPECT_GT(uninstallResult.getExecutionTime(), 0);

		qDebug() << "Uninstall package 'requests' completed successfully.";
	}

	qDebug() << "Install, run, and uninstall 'requests' package test completed successfully.";
}

// Test: Install and Verify Package (numpy)
// TEST_F(PythonPackagesTest, InstallAndVerifyPackage_numpy) {
// 	// Arrange
// 	QString package = "numpy";
// 
// 	// Set up QSignalSpy to listen to packageInstalled signal
// 	QSignalSpy spy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
// 
// 
// 
// 
// 	// Act
// 	pythonEnv->uninstallPackage(package);
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	pythonEnv->installPackage(package);
// 
// 	// Wait for the signal
// 
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 	
// 	// Assert
// 	ASSERT_TRUE(spy.count()) << "packageInstalled signal for package " << package << " not received.";
// 
// 	QList<QVariant> arguments = spy.takeFirst();
// 	const auto uninstallation = arguments.at(2).value<PythonResult>();
// 	
// 	arguments = spy.takeFirst();
// 	const auto installation = arguments.at(2).value<PythonResult>();
// 
// 
// 	EXPECT_TRUE(installation.isSuccess()) << "Installation of package 'numpy' failed: " << installation.getErrorOutput();
// 	EXPECT_TRUE(pythonEnv->isPackageInstalled(package)) << "'numpy' should be installed.";
// }

// Test: Use Installed Package (numpy)
// TEST_F(PythonPackagesTest, UseInstalledPackage_numpy) {
// 	// Arrange
// 	QString package = "numpy";
// 	QSignalSpy spy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
// 	QSignalSpy spy2(runner, &PythonRunner::scriptFinished);
// 
// 	auto executionId = QUuid::createUuid().toString();
// 
// 	pythonEnv->uninstallPackage(executionId, package);
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	// Act
// 	pythonEnv->installPackage(executionId, package);
// 
// 	// Wait for the signal
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	// Assert
// 	// Iterate through the captured signals to find the one for 'requests'
// 	bool found = false;
// 	PythonResult result1;
// 	for (int i = 0; i < spy.count(); ++i) {
// 		QList<QVariant> arguments = spy.takeFirst();
// 		QString receivedPackage = arguments.at(0).toString();
// 		if (receivedPackage == package) {
// 			result1 = arguments.at(1).value<PythonResult>();
// 			found = true;
// 			break;
// 		}
// 	}
// 
// 	QString script =
// 		"import numpy as np\n"
// 		"arr = np.array([1, 2, 3, 4])\n"
// 		"print(arr.sum())\n";
// 
// 
// 	// Act
// 	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);
// 
// 	// Wait for the result
// 	ASSERT_TRUE(spy2.wait(5000));
// 
// 	// Get the result
// 	PythonResult result = future.result();
// 
// 
// 	// Assert
// 	EXPECT_TRUE(result.isSuccess()) << "Script execution failed: " << result.getErrorOutput();
// 	EXPECT_EQ(result.getOutput().trimmed(), "10") << "Expected sum of array to be 10.";
// 	EXPECT_EQ(result.getExecutionId().trimmed(), executionId) << "Expected correct executionid";
// }
// 
// // Test: Install and Verify Package (requests)
// TEST_F(PythonPackagesTest, InstallAndVerifyPackage_requests) {
// 	// Arrange
// 	QString package = "requests";
// 
// 	// Set up QSignalSpy to listen to packageInstalled signal
// 	QSignalSpy spy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
// 
// 	pythonEnv->uninstallPackage(package);
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	// Act
// 	pythonEnv->installPackage(package);
// 
// 	// Wait for the signal
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	// Assert
// 	// Iterate through the captured signals to find the one for 'requests'
// 	bool found = false;
// 	PythonResult result;
// 	for (int i = 0; i < spy.count(); ++i) {
// 		QList<QVariant> arguments = spy.takeFirst();
// 		QString receivedPackage = arguments.at(1).toString();
// 		if (receivedPackage == package) {
// 			result = arguments.at(2).value<PythonResult>();
// 			found = true;
// 			break;
// 		}
// 	}
// 
// 	ASSERT_TRUE(found) << "packageInstalled signal for package " << package << " not received.";
// 
// 	EXPECT_TRUE(result.isSuccess()) << "Installation of package 'requests' failed: " << result.getErrorOutput();
// 	EXPECT_TRUE(pythonEnv->isPackageInstalled(package)) << "'requests' should be installed.";
// }
// 
// Test: Use Installed Package (requests)
// TEST_F(PythonPackagesTest, UseInstalledPackage_requests) {
// 	// Arrange
// 	QString package = "requests";
// 
// 	QSignalSpy spy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
// 	QSignalSpy spy2(runner, &PythonRunner::scriptFinished);
// 	auto executionId = QUuid::createUuid().toString();
// 	pythonEnv->uninstallPackage(executionId, package);
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	pythonEnv->installPackage(executionId, package);
// 
// 	// Wait for the signal
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 
// 	QString script =
// 		"import requests\n"
// 		"response = requests.get('https://httpbin.org/get')\n"
// 		"print(response.status_code)\n";
// 
// 	// Act
// 	QFuture<PythonResult> future = runner->runScriptAsync("uniqueExecutionId", script);
// 
// 	// Wait for the result
// 	ASSERT_TRUE(spy2.wait(5000));
// 
// 
// 	// Get the result
// 	PythonResult result = future.result();
// 
// 	// Assert
// 	EXPECT_TRUE(result.isSuccess()) << "Script execution failed: " << result.getErrorOutput();
// 	EXPECT_EQ(result.getOutput().trimmed(), "200") << "Expected HTTP status code to be 200.";
// 	EXPECT_EQ(result.getExecutionId().trimmed(), executionId) << "Expected correct executionid";
// }
// 
// // Test: Install Non-Existent Package
// TEST_F(PythonPackagesTest, InstallNonExistentPackage) {
// 	// Arrange
// 	QString package = "nonexistentpackage12345";
// 
// 	// Set up QSignalSpy to listen to packageInstalled signal
// 	QSignalSpy spy(pythonEnv.get(), &PythonEnvironment::packageOperationFinished);
// 	auto executionId = QUuid::createUuid().toString();
// 	// Act
// 	pythonEnv->installPackage(executionId, package);
// 
// 	// Wait for the signal
// 	ASSERT_TRUE(waitForSignal(spy)) << "Did not receive packageInstalled signal within timeout.";
// 
// 	// Assert
// 
// 	QList<QVariant> arguments = spy.takeFirst();
// 	const auto result = arguments.at(2).value<PythonResult>();
// 
// 	ASSERT_TRUE(spy.count()) << "packageInstalled signal for package " << package << " not received.";
// 
// 	EXPECT_FALSE(result.isSuccess()) << "Installation of non-existent package should fail.";
// 	EXPECT_FALSE(pythonEnv->isPackageInstalled(package)) << "Non-existent package should not be installed.";
// 	EXPECT_EQ(result.getExecutionId().trimmed(), executionId) << "Expected correct executionid";
// }

