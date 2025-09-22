use crate::rc::{Array, Ref, RefCountable};
use crate::repository::Channel;
use crate::string::BnStrCompatible;
use binaryninjacore_sys::{
    BNGetExtensionManager,
    BNNewExtensionManagerReference, BNExtensionManagerGetChannelByName, BNExtensionManager,
    BNExtensionManagerAddChannel, BNExtensionManagerCheckForUpdates, BNExtensionManagerGetChannels,
    BNExtensionManagerFetchChannelsAsync
};
use std::ffi::c_char;
use std::fmt::Debug;
use std::ptr::NonNull;

/// Manages the list of channels and their extensions
#[repr(transparent)]
pub struct ExtensionManager {
    handle: NonNull<BNExtensionManager>,
}

impl ExtensionManager {
    #[allow(clippy::should_implement_trait)]
    pub fn default() -> Ref<Self> {
        let result = unsafe { BNGetExtensionManager() };
        unsafe { Self::ref_from_raw(NonNull::new(result).unwrap()) }
    }

    pub(crate) unsafe fn ref_from_raw(handle: NonNull<BNExtensionManager>) -> Ref<Self> {
        Ref::new(Self { handle })
    }

    /// Check for updates for all managed [`Channel`] objects
    pub fn check_for_updates(&self) -> bool {
        unsafe { BNExtensionManagerCheckForUpdates(self.handle.as_ptr()) }
    }

    /// Asynchronously fetch channels and their extensions
    pub fn fetch_channels_async(&self) -> bool {
        unsafe { BNExtensionManagerFetchChannelsAsync(self.handle.as_ptr()) }
    }

    /// List of [`Channel`] objects being managed
    pub fn channels(&self) -> Array<Channel> {
        let mut count = 0;
        let result =
            unsafe { BNExtensionManagerGetChannels(&mut count) };
        assert!(!result.is_null());
        unsafe { Array::new(result, count, ()) }
    }

    /// Adds a new extension channel for the manager to track.
    ///
    /// To remove a channel, restart Binary Ninja (and don't re-add the channel!).
    /// File artifacts will remain on disk under extensions/ file in the User Folder.
    ///
    /// Before you can query extension metadata from a channel, you need to call [`ExtensionManager::check_for_updates`].
    ///
    /// * `url` - URL to the extensions.json containing the records for this channel
    /// * `name` - name to identify the channel
    /// * `is_user_channel` - Whether this is a user channel (special handling)
    ///
    /// Returns true if the channel was successfully added, false otherwise.
    pub fn add_channel<U: BnStrCompatible, P: BnStrCompatible>(
        &self,
        url: U,
        name: P,
        is_user_channel: bool,
    ) -> bool {
        let url = url.into_bytes_with_nul();
        let name = name.into_bytes_with_nul();
        unsafe {
            BNExtensionManagerAddChannel(
                self.handle.as_ptr(),
                url.as_ref().as_ptr() as *const c_char,
                name.as_ref().as_ptr() as *const c_char,
                is_user_channel,
            )
        }
    }

    pub fn channel_by_name<P: BnStrCompatible>(&self, name: P) -> Option<Channel> {
        let name = name.into_bytes_with_nul();
        let result = unsafe {
            BNExtensionManagerGetChannelByName(
                self.handle.as_ptr(),
                name.as_ref().as_ptr() as *const c_char,
            )
        };
        NonNull::new(result).map(|raw| unsafe { Channel::from_raw(raw) })
    }
}

impl Debug for ExtensionManager {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ExtensionManager")
            .field("channels", &self.channels().to_vec())
            .finish()
    }
}

impl ToOwned for ExtensionManager {
    type Owned = Ref<Self>;

    fn to_owned(&self) -> Self::Owned {
        unsafe { RefCountable::inc_ref(self) }
    }
}