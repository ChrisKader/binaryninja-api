use crate::rc::{Array, CoreArrayProvider, CoreArrayProviderInner, Guard, Ref, RefCountable};
use crate::repository::{PluginStatus};
use crate::repository::version::ExtensionVersion;
use crate::string::BnString;
use crate::VersionInfo;
use binaryninjacore_sys::*;
use std::ffi::c_char;
use std::fmt::Debug;
use std::ptr::NonNull;

#[repr(transparent)]
pub struct Extension {
    handle: NonNull<BNExtension>,
}

impl Extension {
    pub(crate) unsafe fn from_raw(handle: NonNull<BNExtension>) -> Self {
        Self { handle }
    }

    pub(crate) unsafe fn ref_from_raw(handle: NonNull<BNExtension>) -> Ref<Self> {
        Ref::new(Self { handle })
    }

    /// String of the extension author
    pub fn author(&self) -> BnString {
        let result = unsafe { BNExtensionGetAuthor(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// String short description of the extension
    pub fn description(&self) -> BnString {
        let result = unsafe { BNExtensionGetDescription(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// String extension name
    pub fn name(&self) -> BnString {
        let result = unsafe { BNExtensionGetName(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// String URL of the extension's git repository
    pub fn project_url(&self) -> BnString {
        let result = unsafe { BNExtensionGetProjectUrl(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// Get all available versions of this extension
    pub fn versions(&self) -> Array<ExtensionVersion> {
        let mut count = 0;
        let result = unsafe { BNExtensionGetVersions(self.handle.as_ptr(), &mut count) };
        assert!(!result.is_null());
        unsafe { Array::new(result, count, ()) }
    }

    /// Get the currently installed version of this extension
    pub fn current_version(&self) -> Option<Ref<ExtensionVersion>> {
        let result = unsafe { BNExtensionGetCurrentVersion(self.handle.as_ptr()) };
        NonNull::new(result).map(|h| unsafe { ExtensionVersion::ref_from_raw(h) })
    }

    /// Get the latest available version of this extension
    pub fn latest_version(&self) -> Option<Ref<ExtensionVersion>> {
        let result = unsafe { BNExtensionGetLatestVersion(self.handle.as_ptr()) };
        NonNull::new(result).map(|h| unsafe { ExtensionVersion::ref_from_raw(h) })
    }

    /// Relative path from the base of the channel to the actual extension
    pub fn path(&self) -> BnString {
        let result = unsafe { BNExtensionGetPath(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// Name of the channel containing this extension
    pub fn channel_name(&self) -> BnString {
        let result = unsafe { BNExtensionGetChannelName(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// true if the extension is installed, false otherwise
    pub fn is_installed(&self) -> bool {
        unsafe { BNExtensionIsInstalled(self.handle.as_ptr()) }
    }

    /// true if the extension is enabled, false otherwise
    pub fn is_enabled(&self) -> bool {
        unsafe { BNExtensionIsEnabled(self.handle.as_ptr()) }
    }

    pub fn status(&self) -> PluginStatus {
        unsafe { BNExtensionGetPluginStatus(self.handle.as_ptr()) }
    }

    /// Enable this extension
    pub fn enable(&self) -> bool {
        unsafe { BNExtensionEnable(self.handle.as_ptr()) }
    }

    pub fn disable(&self) -> bool {
        unsafe { BNExtensionDisable(self.handle.as_ptr()) }
    }

    /// Attempt to uninstall the extension
    pub fn uninstall(&self) -> bool {
        unsafe { BNExtensionUninstall(self.handle.as_ptr()) }
    }

    /// Install the extension with the specified version or latest if None
    pub fn install(&self, version: Option<&ExtensionVersion>) -> bool {
        match version {
            Some(v) => unsafe { BNExtensionInstall(self.handle.as_ptr(), v.handle()) },
            None => unsafe { BNExtensionInstall(self.handle.as_ptr(), std::ptr::null_mut()) }
        }
    }

    /// Update the extension to the specified version or latest if None
    pub fn update(&self, version: Option<&ExtensionVersion>) -> bool {
        match version {
            Some(v) => unsafe { BNExtensionUpdate(self.handle.as_ptr(), v.handle()) },
            None => unsafe { BNExtensionUpdate(self.handle.as_ptr(), std::ptr::null_mut()) }
        }
    }

    /// Install the extension's dependencies
    pub fn install_dependencies(&self) -> bool {
        unsafe { BNExtensionInstallDependencies(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension is being deleted
    pub fn is_being_deleted(&self) -> bool {
        unsafe { BNExtensionIsBeingDeleted(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension is being updated
    pub fn is_being_updated(&self) -> bool {
        unsafe { BNExtensionIsBeingUpdated(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension is currently running
    pub fn is_running(&self) -> bool {
        unsafe { BNExtensionIsRunning(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension has updates will be installed after the next restart
    pub fn is_update_pending(&self) -> bool {
        unsafe { BNExtensionIsUpdatePending(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension will be disabled after the next restart
    pub fn is_disable_pending(&self) -> bool {
        unsafe { BNExtensionIsDisablePending(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension will be deleted after the next restart
    pub fn is_delete_pending(&self) -> bool {
        unsafe { BNExtensionIsDeletePending(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension has updates available
    pub fn is_update_available(&self) -> bool {
        unsafe { BNExtensionIsUpdateAvailable(self.handle.as_ptr()) }
    }

    /// Boolean status indicating that the extension's dependencies are currently being installed
    pub fn are_dependencies_being_installed(&self) -> bool {
        unsafe { BNExtensionAreDependenciesBeingInstalled(self.handle.as_ptr()) }
    }
}

impl Debug for Extension {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Extension")
            .field("name", &self.name())
            .field("author", &self.author())
            .field("description", &self.description())
            .field("status", &self.status())
            .finish()
    }
}

impl ToOwned for Extension {
    type Owned = Ref<Self>;

    fn to_owned(&self) -> Self::Owned {
        unsafe { RefCountable::inc_ref(self) }
    }
}

unsafe impl RefCountable for Extension {
    unsafe fn inc_ref(handle: &Self) -> Ref<Self> {
        Self::ref_from_raw(NonNull::new(BNNewExtensionReference(handle.handle.as_ptr())).unwrap())
    }

    unsafe fn dec_ref(handle: &Self) {
        BNFreeExtension(handle.handle.as_ptr())
    }
}

impl CoreArrayProvider for Extension {
    type Raw = *mut BNExtension;
    type Context = ();
    type Wrapped<'a> = Guard<'a, Self>;
}

unsafe impl CoreArrayProviderInner for Extension {
    unsafe fn free(raw: *mut Self::Raw, _count: usize, _context: &Self::Context) {
        BNFreeChannelExtensionList(raw)
    }

    unsafe fn wrap_raw<'a>(raw: &'a Self::Raw, context: &'a Self::Context) -> Self::Wrapped<'a> {
        Guard::new(Self::from_raw(NonNull::new(*raw).unwrap()), context)
    }
}