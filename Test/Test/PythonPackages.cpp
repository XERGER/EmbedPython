// PythonPackagesTest.cpp
#include "../pch.h"
#include "Library/PythonEnvironment.h"
#include "Library/PythonRunner.h"
#include "Library/PythonResult.h"
#include <gtest/gtest.h>

class PythonPackagesTest : public ::testing::Test {
protected:
	void SetUp() override {
		pythonEnv = std::make_shared<PythonEnvironment>();
		ASSERT_TRUE(pythonEnv->init());
		runner = new PythonRunner(pythonEnv, nullptr);
	}

	void TearDown() override {
		delete runner;
	}

	std::shared_ptr<PythonEnvironment> pythonEnv;
	PythonRunner* runner;
};
TEST_F(PythonPackagesTest, InstallAndVerifyPackage_numpy) {
	// Arrange
	QString package = "numpy";

	// Act
	bool installSuccess = pythonEnv->installPackage(package);

	// Assert
	EXPECT_TRUE(installSuccess);
	EXPECT_TRUE(pythonEnv->isPackageInstalled(package));
}

// Test using an installed package
TEST_F(PythonPackagesTest, UseInstalledPackage_numpy) {
	// Arrange
	QString package = "numpy";
	ASSERT_TRUE(pythonEnv->installPackage(package));

	QString script =
		"import numpy as np\n"
		"arr = np.array([1, 2, 3, 4])\n"
		"print(arr.sum())\n";

	// Act
	PythonResult result = runner->runScript(script);

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().trimmed(), "10"); // 1 + 2 + 3 + 4 = 10
}
TEST_F(PythonPackagesTest, InstallAndVerifyPackage) {
	// Arrange
	QString package = "requests";

	// Act
	bool installSuccess = pythonEnv->installPackage(package);

	// Assert
	EXPECT_TRUE(installSuccess);
	EXPECT_TRUE(pythonEnv->isPackageInstalled(package));
}

TEST_F(PythonPackagesTest, UseInstalledPackage) {
	// Arrange
	QString package = "requests";
	ASSERT_TRUE(pythonEnv->installPackage(package));
	QString script =
		"import requests\n"
		"response = requests.get('https://httpbin.org/get')\n"
		"print(response.status_code)\n";

	// Act
	PythonResult result = runner->runScript(script);

	// Assert
	EXPECT_TRUE(result.isSuccess());
	EXPECT_EQ(result.getOutput().trimmed(), "200");
}

TEST_F(PythonPackagesTest, InstallNonExistentPackage) {
	// Arrange
	QString package = "nonexistentpackage12345";

	// Act
	bool installSuccess = pythonEnv->installPackage(package);

	// Assert
	EXPECT_FALSE(installSuccess);
	EXPECT_FALSE(pythonEnv->isPackageInstalled(package));
}
