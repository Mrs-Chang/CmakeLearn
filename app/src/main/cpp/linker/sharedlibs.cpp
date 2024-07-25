/**
 * Copyright 2004-present, Facebook, Inc.
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

#include "sharedlibs.h"
#include "locks.h"

#include "Build.h"

#include <sys/system_properties.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unordered_map>
#include <libgen.h>

#define DT_GNU_HASH    0x6ffffef5    /* GNU-style hash table.  */
#define  LOG_TAG    "aaa"
#define  ALOG(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

namespace facebook {
    namespace linker {

        namespace {

            static pthread_rwlock_t sharedLibsMutex_ = PTHREAD_RWLOCK_INITIALIZER;

            static std::unordered_map<std::string, elfSharedLibData> &sharedLibData() {
                static std::unordered_map<std::string, elfSharedLibData> sharedLibData_;
                return sharedLibData_;
            }

            template<typename Arg>
            bool addSharedLib(char const *libname, Arg &&arg) {

                auto baseName = basename(libname);
                ALOG("Attempting to add shared library: %s", baseName);
                if (sharedLibData().find(baseName) == sharedLibData().end()) {
                    try {
                        elfSharedLibData data(std::forward<Arg>(arg));
                        WriterLock wl(&sharedLibsMutex_);
                        auto result = sharedLibData().insert(
                                std::make_pair(baseName, std::move(data))).second;
                        if (result) {
                            ALOG("Successfully added shared library: %s", baseName);
                        } else {
                            ALOG("Failed to add shared library: %s", baseName);
                        }
                        return result;
                    } catch (input_parse_error &) {
                        ALOG("Failed to parse shared library: %s", baseName);
                    }
                } else {
                    ALOG("Shared library already exists: %s", baseName);
                }

                return false;
            }

            bool ends_with(const char *str, const char *ending) {
                size_t str_len = strlen(str);
                size_t ending_len = strlen(ending);

                if (ending_len > str_len) {
                    return false;
                }
                return strcmp(str + (str_len - ending_len), ending) == 0;
            }

        } // namespace (anonymous)

        elfSharedLibData sharedLib(char const *libname) {
            ReaderLock rl(&sharedLibsMutex_);
            auto baseName = basename(libname);
            ALOG("Attempting to find shared library: %s", baseName);
            auto it = sharedLibData().find(baseName);
            if (it == sharedLibData().end()) {
                ALOG("Library %s not found in sharedLibData", baseName);
                throw std::out_of_range(libname);
            }
            return it->second;
        }

        std::vector<std::pair<std::string, elfSharedLibData>> allSharedLibs() {
            ReaderLock rl(&sharedLibsMutex_);
            std::vector<std::pair<std::string, elfSharedLibData>> libs;
            libs.reserve(sharedLibData().size());
            std::copy(sharedLibData().begin(), sharedLibData().end(), std::back_inserter(libs));
            return libs;
        }

// for testing. not exposed via header.
        void clearSharedLibs() {
            WriterLock wl(&sharedLibsMutex_);
            sharedLibData().clear();
        }

    }
} // namespace facebook::linker

extern "C" {

using namespace facebook::linker;
#define ANDROID_L  21

int
refresh_shared_libs() {
    ALOG("Refreshing shared libraries");
    if (facebook::build::Build::getAndroidSdk() >= ANDROID_L) {
        static auto dl_iterate_phdr =
                reinterpret_cast<int (*)(int(*)(dl_phdr_info *, size_t, void *), void *)>(
                        dlsym(RTLD_DEFAULT, "dl_iterate_phdr"));

        if (dl_iterate_phdr == nullptr) {
            ALOG("dl_iterate_phdr is nullptr");
            return 1;
        }

        dl_iterate_phdr(+[](dl_phdr_info *info, size_t, void *) {
            if (info->dlpi_name && ends_with(info->dlpi_name, ".so")) {
                ALOG("Found shared library: %s", info->dlpi_name);
                addSharedLib(info->dlpi_name, info);
            }
            return 0;
        }, nullptr);
    } else {
#ifndef __LP64__
        soinfo *si = reinterpret_cast<soinfo *>(dlopen(nullptr, RTLD_LOCAL));

        if (si == nullptr) {
            ALOG("soinfo is nullptr");
            return 1;
        }

        for (; si != nullptr; si = si->next) {
            if (si->link_map.l_name && ends_with(si->link_map.l_name, ".so")) {
                ALOG("Found shared library: %s", si->link_map.l_name);
                addSharedLib(si->link_map.l_name, si);
            }
        }
#endif
    }
    ALOG("Finished refreshing shared libraries");
    return 0;
}

} // extern C
