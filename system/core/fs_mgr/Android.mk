#
# Copyright (C) 2014 MediaTek Inc.
# Modification based on code covered by the mentioned copyright
# and/or permission notice(s).
#
# Copyright 2011 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= fs_mgr.c fs_mgr_verity.c fs_mgr_fstab.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_MODULE:= libfs_mgr
LOCAL_STATIC_LIBRARIES := liblogwrap libmincrypt libext4_utils_static
LOCAL_C_INCLUDES += system/extras/ext4_utils
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include
LOCAL_CFLAGS := -Werror
# add mtk fstab flags support
LOCAL_CFLAGS += -DMTK_FSTAB_FLAGS
# end

include $(BUILD_STATIC_LIBRARY)



include $(CLEAR_VARS)

LOCAL_SRC_FILES:= fs_mgr_main.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_MODULE:= fs_mgr

LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)/sbin
LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_UNSTRIPPED)

LOCAL_STATIC_LIBRARIES := libfs_mgr liblogwrap libcutils liblog libc libmincrypt libext4_utils_static

LOCAL_CFLAGS := -Werror

include $(BUILD_EXECUTABLE)

