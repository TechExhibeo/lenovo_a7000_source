/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright 2011, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CUTILS_PARTITION_WIPED_H__
#define __CUTILS_PARTITION_WIPED_H__

__BEGIN_DECLS

int partition_wiped(char *source);
void erase_footer(const char *dev_path, long long size);


#define PHONE_ENCRYPTED (0x4321)
#define PHONE_UNCRYPTED (0x0)
#define PHONE_ENCRYPT_OFFSET (2048)
struct phone_encrypt_state {
    int state;
};

int misc_set_phone_encrypt_state(const struct phone_encrypt_state *in);
int misc_get_phone_encrypt_state(struct phone_encrypt_state *in);


__END_DECLS

#endif /* __CUTILS_PARTITION_WIPED_H__ */
