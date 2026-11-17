# Linux overlay. Same package version as registry 3548d22. Source is the verified
# public FE-7 candidate. fc295112 does not compile on LP64 because long and int64_t
# are the same type. The registry port URL remains an SSH carbonengine URL.
vcpkg_from_git(
  OUT_SOURCE_PATH SOURCE_PATH
  URL https://github.com/rediai-carbon/carbon-blueexposure.git
  REF 3de62e67368b3cbc328388bd5aaf40b880002cf0
  HEAD_REF main
)

vcpkg_cmake_configure(
  SOURCE_PATH ${SOURCE_PATH}
  OPTIONS
  -DBUILD_TESTING=OFF
  -DVCPKG_USE_HOST_TOOLS=ON
  -DVCPKG_HOST_TRIPLET=${HOST_TRIPLET}
  -DCMAKE_BUILD_TYPE=${CARBON_BUILD_TYPE}
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup()
vcpkg_copy_pdbs()