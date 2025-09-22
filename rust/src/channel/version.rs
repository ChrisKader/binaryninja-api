// use crate::string::BnString;
// use crate::VersionInfo;
// use binaryninjacore_sys::*;
// use std::ffi::c_char;
// use std::fmt::Debug;
// use std::ptr::NonNull;

// #[repr(transparent)]
// pub struct ExtensionVersion {
//     handle: NonNull<BNExtensionVersion>,
// }

// impl ExtensionVersion {
//     pub(crate) unsafe fn from_raw(handle: NonNull<BNExtensionVersion>) -> Self {
//         Self { handle }
//     }

//     pub(crate) unsafe fn ref_from_raw(handle: NonNull<BNExtensionVersion>) -> Ref<Self> {
//         Ref::new(Self { handle })
//     }

//     pub(crate) fn handle(&self) -> *mut BNExtensionVersion {
//         self.handle.as_ptr()
//     }

//     /// String version of the extension
//     pub fn version_string(&self) -> BnString {
//         let result = unsafe { BNExtensionVersionGetVersionString(self.handle.as_ptr()) };
//         assert!(!result.is_null());
//         unsafe { BnString::from_raw(result as *mut c_char) }
//     }

//     /// String long description of the extension
//     pub fn long_description(&self) -> BnString {
//         let result = unsafe { BNExtensionVersionGetLongDescription(self.handle.as_ptr()) };
//         assert!(!result.is_null());
//         unsafe { BnString::from_raw(result as *mut c_char) }
//     }

//     /// String changelog for this version
//     pub fn changelog(&self) -> BnString {
//         let result = unsafe { BNExtensionVersionGetChangelog(self.handle.as_ptr()) };
//         assert!(!result.is_null());
//         unsafe { BnString::from_raw(result as *mut c_char) }
//     }

//     /// VersionInfo object representing the minimum client version supported
//     pub fn minimum_version_info(&self) -> VersionInfo {
//         VersionInfo::from_raw(unsafe { BNExtensionVersionGetMinimumVersionInfo(self.handle.as_ptr()) })
//     }

//     /// VersionInfo object representing the maximum client version supported
//     pub fn maximum_version_info(&self) -> VersionInfo {
//         VersionInfo::from_raw(unsafe { BNExtensionVersionGetMaximumVersionInfo(self.handle.as_ptr()) })
//     }

//     /// String dependencies required by this version
//     pub fn dependencies(&self) -> BnString {
//         let result = unsafe { BNExtensionVersionGetDependencies(self.handle.as_ptr()) };
//         assert!(!result.is_null());
//         unsafe { BnString::from_raw(result as *mut c_char) }
//     }

//     /// Get the download URL for this version for the current platform
//     pub fn download_url(&self, use_short_url: bool) -> BnString {
//         let result = unsafe { BNExtensionVersionGetDownloadUrl(self.handle.as_ptr(), use_short_url) };
//         assert!(!result.is_null());
//         unsafe { BnString::from_raw(result as *mut c_char) }
//     }

//     /// Install dependencies for this version
//     pub fn install_dependencies(&self) -> bool {
//         unsafe { BNExtensionVersionInstallDependencies(self.handle.as_ptr()) }
//     }
// }

// impl Debug for ExtensionVersion {
//     fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
//         f.debug_struct("ExtensionVersion")
//             .field("version_string", &self.version_string())
//             .field("minimum_version_info", &self.minimum_version_info())
//             .finish()
//     }
// }

// impl ToOwned for ExtensionVersion {
//     type Owned = Ref<Self>;

//     fn to_owned(&self) -> Self::Owned {
//         unsafe { RefCountable::inc_ref(self) }
//     }
// }

// unsafe impl RefCountable for ExtensionVersion {
//     unsafe fn inc_ref(handle: &Self) -> Ref<Self> {
//         Self::ref_from_raw(NonNull::new(BNNewExtensionVersionReference(handle.handle.as_ptr())).unwrap())
//     }

//     unsafe fn dec_ref(handle: &Self) {
//         BNFreeExtensionVersion(handle.handle.as_ptr())
//     }
// }