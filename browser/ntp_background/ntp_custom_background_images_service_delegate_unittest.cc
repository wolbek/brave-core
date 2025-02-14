/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ntp_background/ntp_custom_background_images_service_delegate.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "brave/browser/ntp_background/constants.h"
#include "brave/browser/ntp_background/custom_background_file_manager.h"
#include "brave/browser/ntp_background/ntp_background_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class NTPCustomBackgroundImagesServiceDelegateUnitTest : public testing::Test {
 public:
  NTPCustomBackgroundImagesServiceDelegateUnitTest() = default;
  ~NTPCustomBackgroundImagesServiceDelegateUnitTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Test");
    service_delegate_ =
        std::make_unique<NTPCustomBackgroundImagesServiceDelegate>(profile_);
  }

  void TearDown() override {
    service_delegate_.reset();
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    testing::Test::TearDown();
  }

 protected:
  TestingProfile& profile() { return *profile_; }
  NTPCustomBackgroundImagesServiceDelegate& service_delegate() {
    return *service_delegate_;
  }

 private:
  content::BrowserTaskEnvironment env;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;

  std::unique_ptr<NTPCustomBackgroundImagesServiceDelegate> service_delegate_;
};

TEST_F(NTPCustomBackgroundImagesServiceDelegateUnitTest, MigrationSuccess) {
  NTPBackgroundPrefs ntp_prefs(profile().GetPrefs());

  // Get ready for old data.
  {
    ntp_prefs.SetType(NTPBackgroundPrefs::Type::kCustomImage);
    ntp_prefs.SetShouldUseRandomValue(false);
    ntp_prefs.SetSelectedValue(std::string());

    base::ScopedAllowBlockingForTesting allow_blocking_call;
    ASSERT_TRUE(base::CopyFile(
        base::FilePath(
            FILE_PATH_LITERAL("brave/test/data/ntp_background/background.jpg")),
        profile().GetPath().AppendASCII(
            ntp_background_images::kSanitizedImageFileNameDeprecated)));
  }

  EXPECT_TRUE(service_delegate().ShouldMigrateCustomImagePref());

  base::RunLoop run_loop;
  auto check_migration = base::BindOnce([](bool result) {
                           EXPECT_TRUE(result);
                         }).Then(run_loop.QuitClosure());
  service_delegate().MigrateCustomImage(std::move(check_migration));
  run_loop.Run();

  {
    base::ScopedAllowBlockingForTesting allow_blocking_call;
    EXPECT_TRUE(base::PathExists(
        service_delegate()
            .file_manager_->GetCustomBackgroundDirectory()
            .AppendASCII(
                ntp_background_images::kSanitizedImageFileNameDeprecated)));
  }
  EXPECT_TRUE(ntp_prefs.IsCustomImageType());
  EXPECT_EQ(1u, ntp_prefs.GetCustomImageList().size());
  EXPECT_EQ(ntp_background_images::kSanitizedImageFileNameDeprecated,
            ntp_prefs.GetCustomImageList().front());
}

TEST_F(NTPCustomBackgroundImagesServiceDelegateUnitTest, MigrationFail) {
  NTPBackgroundPrefs ntp_prefs(profile().GetPrefs());

  // Get ready for old data but doesn't have image file
  {
    ntp_prefs.SetType(NTPBackgroundPrefs::Type::kCustomImage);
    ntp_prefs.SetShouldUseRandomValue(false);
    ntp_prefs.SetSelectedValue(std::string());
  }

  EXPECT_TRUE(service_delegate().ShouldMigrateCustomImagePref());

  base::RunLoop run_loop;
  auto check_migration = base::BindOnce([](bool result) {
                           EXPECT_FALSE(result);
                         }).Then(run_loop.QuitClosure());
  service_delegate().MigrateCustomImage(std::move(check_migration));
  run_loop.Run();

  // When failed, should be reset to default.
  EXPECT_TRUE(ntp_prefs.IsBraveType());
  EXPECT_TRUE(ntp_prefs.ShouldUseRandomValue());
}
