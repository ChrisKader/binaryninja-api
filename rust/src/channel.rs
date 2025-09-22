mod manager;
mod extension;
mod version;

use std::ffi::c_char;
use std::fmt::Debug;
use std::ptr::NonNull;

use binaryninjacore_sys::*;

use crate::rc::{Array, CoreArrayProvider, CoreArrayProviderInner, Guard, Ref, RefCountable};
use crate::repository::extension::Extension;
use crate::string::{BnStrCompatible, BnString};

pub use manager::ExtensionManager;
pub use version::ExtensionVersion;

pub type PluginStatus = BNPluginStatus;

#[repr(transparent)]
pub struct Channel {
    handle: NonNull<BNExtensionChannel>,
}

impl Channel {
    pub(crate) unsafe fn from_raw(handle: NonNull<BNExtensionChannel>) -> Self {
        Self { handle }
    }

    pub(crate) unsafe fn ref_from_raw(handle: NonNull<BNExtensionChannel>) -> Ref<Self> {
        Ref::new(Self { handle })
    }

    /// String URL of the git repository where the channel's extensions are stored
    pub fn url(&self) -> BnString {
        let result = unsafe { BNChannelGetUrl(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// String name of the channel
    pub fn name(&self) -> BnString {
        let result = unsafe { BNChannelGetName(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }

    /// List of Extension objects contained within this channel
    pub fn extensions(&self) -> Array<Extension> {
        let mut count = 0;
        let result = unsafe { BNChannelGetExtensions(self.handle.as_ptr(), &mut count) };
        assert!(!result.is_null());
        unsafe { Array::new(result, count, ()) }
    }

    pub fn extension_by_path<S: BnStrCompatible>(&self, path: S) -> Option<Ref<Extension>> {
        let path = path.into_bytes_with_nul();
        let result = unsafe {
            BNChannelGetExtensionByPath(
                self.handle.as_ptr(),
                path.as_ref().as_ptr() as *const c_char,
            )
        };
        NonNull::new(result).map(|h| unsafe { Extension::ref_from_raw(h) })
    }

    /// String full path the channel directory
    pub fn full_path(&self) -> BnString {
        let result = unsafe { BNChannelGetFullPath(self.handle.as_ptr()) };
        assert!(!result.is_null());
        unsafe { BnString::from_raw(result as *mut c_char) }
    }
}

impl Debug for Channel {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Channel")
            .field("url", &self.url())
            .field("name", &self.name())
            .field("full_path", &self.full_path())
            .field("extensions", &self.extensions().to_vec())
            .finish()
    }
}

impl ToOwned for Channel {
    type Owned = Ref<Self>;

    fn to_owned(&self) -> Self::Owned {
        unsafe { <Self as RefCountable>::inc_ref(self) }
    }
}

unsafe impl RefCountable for Channel {
    unsafe fn inc_ref(handle: &Self) -> Ref<Self> {
        Self::ref_from_raw(NonNull::new(BNNewChannelReference(handle.handle.as_ptr())).unwrap())
    }

    unsafe fn dec_ref(handle: &Self) {
        BNFreeChannel(handle.handle.as_ptr())
    }
}

impl CoreArrayProvider for Channel {
    type Raw = *mut BNExtensionChannel;
    type Context = ();
    type Wrapped<'a> = Guard<'a, Self>;
}

unsafe impl CoreArrayProviderInner for Channel {
    unsafe fn free(raw: *mut Self::Raw, _count: usize, _context: &Self::Context) {
        BNFreeExtensionManagerChannelsList(raw)
    }

    unsafe fn wrap_raw<'a>(raw: &'a Self::Raw, context: &'a Self::Context) -> Self::Wrapped<'a> {
        Guard::new(Self::from_raw(NonNull::new(*raw).unwrap()), context)
    }
}