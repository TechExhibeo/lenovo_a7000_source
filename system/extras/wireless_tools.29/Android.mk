# Copyright (C) 2008 The Android Open Source Project
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


LOCAL_PATH := $(call my-dir)
ifneq ($(TARGET_SIMULATOR),true)

include $(CLEAR_VARS)

LOCAL_MODULE := libiw

LOCAL_MODULE_TAGS := eng

LOCAL_SRC_FILES := iwlib.c

LOCAL_C_INCLUDE += LOCAL_PATH

#LOCAL_SHARED_LIBRARIES := libiw
LOCAL_PRELINK_MODULE := false

#include $(BUILD_SHARED_LIBRARY)
include $(BUILD_STATIC_LIBRARY)

#================================================
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := iwpriv.c

LOCAL_STATIC_LIBRARIES := libiw
LOCAL_MODULE := iwpriv
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)
#================================================


#================================================
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := iwlist.c

LOCAL_STATIC_LIBRARIES := libiw
LOCAL_MODULE := iwlist
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)
#================================================

#================================================
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := iwconfig.c

LOCAL_STATIC_LIBRARIES := libiw
LOCAL_MODULE := iwconfig
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)
#================================================

#================================================
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := iwspy.c

LOCAL_STATIC_LIBRARIES := libiw
LOCAL_MODULE := iwspy
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)
#================================================

#================================================
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := iwgetid.c

LOCAL_STATIC_LIBRARIES := libiw
LOCAL_MODULE := iwgetid
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)
#================================================

#================================================
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES := iwevent.c

LOCAL_STATIC_LIBRARIES := libiw
LOCAL_MODULE := iwevent
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)
#================================================

endif # !TARGET_SIMULATOR
