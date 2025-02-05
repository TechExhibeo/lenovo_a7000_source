#
# Copyright (C) 2014 MediaTek Inc.
# Modification based on code covered by the mentioned copyright
# and/or permission notice(s).
#
#
# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := ion-unit-tests
LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_CFLAGS += -g -Wall -Werror -std=gnu++11 -Wno-missing-field-initializers
LOCAL_SHARED_LIBRARIES += libion
LOCAL_STATIC_LIBRARIES += libgtest_main
LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := eng
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../kernel-headers
LOCAL_SRC_FILES := \
	ion_test_fixture.cpp \
	allocate_test.cpp \
	formerly_valid_handle_test.cpp \
	invalid_values_test.cpp \
	map_test.cpp \
	device_test.cpp \
	exit_test.cpp
include $(BUILD_NATIVE_TEST)
