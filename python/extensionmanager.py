# Copyright (c) 2015-2025 Vector 35 Inc
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

import ctypes
from typing import List, Optional

import binaryninja
from . import _binaryninjacore as core

# ExtensionVersion class commented out as core APIs not available
# class ExtensionVersion:
#     """
#     ``ExtensionVersion`` represents a specific version of an extension.
#     """
#     ...


class Extension:
    """
    ``Extension`` is mostly read-only, however you can install/uninstall enable/disable extensions.
    """

    def __init__(self, handle: core.BNExtensionHandle):
        self.handle = handle

    def __del__(self):
        if core is not None:
            core.BNFreeExtension(self.handle)

    def __repr__(self):
        return f"<{self.name}>"

    @property
    def path(self) -> str:
        """Relative path from the base of the channel to the actual extension"""
        result = core.BNExtensionGetPath(self.handle)
        assert result is not None, "core.BNExtensionGetPath returned None"
        return result

    @property
    def installed(self) -> bool:
        """Boolean True if the extension is installed, False otherwise"""
        return core.BNExtensionIsInstalled(self.handle)
    #
    # def uninstall(self) -> bool:
    #     """Attempt to uninstall the given extension"""
    #     return core.BNExtensionUninstall(self.handle)
    #
    # def install(self, version=None) -> bool:
    #     """
    #     Install this extension using the specified version or the latest if not specified
    #
    #     :param ExtensionVersion version: Optional specific version to install
    #     :return: True if the extension was installed successfully, False otherwise
    #     :rtype: bool
    #     """
    #     if version is None:
    #         return core.BNExtensionInstall(self.handle, None)
    #     else:
    #         return core.BNExtensionInstall(self.handle, version.handle)
    #
    # def install_dependencies(self) -> bool:
    #     """
    #     Install dependencies for this extension
    #
    #     :return: True if dependencies were installed successfully, False otherwise
    #     :rtype: bool
    #     """
    #     return core.BNExtensionInstallDependencies(self.handle)
    #

    @property
    def enabled(self) -> bool:
        """Boolean True if the extension is currently enabled, False otherwise"""
        return core.BNExtensionIsEnabled(self.handle)

    @enabled.setter
    def enabled(self, state: bool):
        if state:
            core.BNExtensionEnable(self.handle)
        else:
            core.BNExtensionDisable(self.handle)

    def enable(self) -> bool:
        """
        Enable this extension
        """
        return core.BNExtensionEnable(self.handle)
    #
    # def update(self, version=None) -> bool:
    #     """
    #     Update this extension to the specified version or the latest if not specified
    #
    #     :param ExtensionVersion version: Optional specific version to update to
    #     :return: True if the update was successful, False otherwise
    #     :rtype: bool
    #     """
    #     if version is None:
    #         return core.BNExtensionUpdate(self.handle, None)
    #     else:
    #         return core.BNExtensionUpdate(self.handle, version.handle)
    #

    @property
    def description(self) -> Optional[str]:
        """String short description of the extension"""
        return core.BNExtensionGetDescription(self.handle)

    @property
    def project_url(self) -> Optional[str]:
        """String URL of the extension's git repository"""
        return core.BNExtensionGetProjectUrl(self.handle)

    @property
    def author(self) -> Optional[str]:
        """String of the extension author"""
        return core.BNExtensionGetAuthor(self.handle)

    @property
    def name(self) -> Optional[str]:
        """String name of the extension"""
        return core.BNExtensionGetName(self.handle)

    @property
    def channel_name(self) -> Optional[str]:
        """String name of the channel containing this extension"""
        return core.BNExtensionGetChannelName(self.handle)

    @property
    def plugin_type(self) -> Optional[str]:
        """String type of the plugin (native, python, etc)"""
        return core.BNExtensionGetPluginType(self.handle)

    # @property
    # def versions(self) -> List[ExtensionVersion]:
    #     """List of ExtensionVersion objects available for this extension"""
    #     versions = []
    #     count = ctypes.c_ulonglong(0)
    #     version_list = core.BNExtensionGetVersions(self.handle, count)
    #     assert version_list is not None, "core.BNExtensionGetVersions returned None"
    #     try:
    #         for i in range(count.value):
    #             version_ref = core.BNNewExtensionVersionReference(version_list[i])
    #             assert version_ref is not None, "core.BNNewExtensionVersionReference returned None"
    #             versions.append(ExtensionVersion(version_ref))
    #         return versions
    #     finally:
    #         core.BNFreeExtensionVersionList(version_list)
    #
    # @property
    # def current_version(self) -> Optional[ExtensionVersion]:
    #     """ExtensionVersion of the currently installed version, or None if not installed"""
    #     version = core.BNExtensionGetCurrentVersion(self.handle)
    #     if version is None:
    #         return None
    #     version_ref = core.BNNewExtensionVersionReference(version)
    #     assert version_ref is not None, "core.BNNewExtensionVersionReference returned None"
    #     return ExtensionVersion(version_ref)
    #
    # @property
    # def latest_version(self) -> Optional[ExtensionVersion]:
    #     """Latest available ExtensionVersion, or None if no versions are available"""
    #     version = core.BNExtensionGetLatestVersion(self.handle)
    #     if version is None:
    #         return None
    #     version_ref = core.BNNewExtensionVersionReference(version)
    #     assert version_ref is not None, "core.BNNewExtensionVersionReference returned None"
    #     return ExtensionVersion(version_ref)
    #
    # @property
    # def being_deleted(self) -> bool:
    #     """Boolean status indicating that the extension is being deleted"""
    #     return core.BNExtensionIsBeingDeleted(self.handle)
    #
    # @property
    # def being_updated(self) -> bool:
    #     """Boolean status indicating that the extension is being updated"""
    #     return core.BNExtensionIsBeingUpdated(self.handle)
    #
    @property
    def running(self) -> bool:
        """Boolean status indicating that the extension is currently running"""
        return core.BNExtensionIsRunning(self.handle)
    #
    # @property
    # def update_pending(self) -> bool:
    #     """Boolean status indicating that the extension has updates will be installed after the next restart"""
    #     return core.BNExtensionIsUpdatePending(self.handle)
    #
    # @property
    # def disable_pending(self) -> bool:
    #     """Boolean status indicating that the extension will be disabled after the next restart"""
    #     return core.BNExtensionIsDisablePending(self.handle)
    #
    # @property
    # def delete_pending(self) -> bool:
    #     """Boolean status indicating that the extension will be deleted after the next restart"""
    #     return core.BNExtensionIsDeletePending(self.handle)
    #
    # @property
    # def update_available(self) -> bool:
    #     """Boolean status indicating that the extension has updates available"""
    #     return core.BNExtensionIsUpdateAvailable(self.handle)
    #
    # @property
    # def dependencies_being_installed(self) -> bool:
    #     """Boolean status indicating that the extension's dependencies are currently being installed"""
    #     return core.BNExtensionAreDependenciesBeingInstalled(self.handle)


class Channel:
    """
    ``Channel`` is a read-only class. Use ExtensionManager to Enable/Disable/Install/Uninstall extensions.
    """

    def __init__(self, handle: core.BNExtensionChannel) -> None:
        self.handle = handle

    def __del__(self) -> None:
        if core is not None:
            core.BNFreeChannel(self.handle)

    def __repr__(self) -> str:
        return f"<Channel: {self.name}>"

    # def __getitem__(self, extension_path: str):
    #     for extension in self.extensions:
    #         if extension_path == extension.path:
    #             return extension
    #     raise KeyError()
    #
    # @property
    # def url(self) -> str:
    #     """String URL of the git repository where the channel's extensions are stored"""
    #     result = core.BNChannelGetUrl(self.handle)
    #     assert result is not None
    #     return result
    #
    @property
    def name(self) -> str:
        """String name of the channel"""
        result = core.BNChannelGetName(self.handle)
        assert result is not None
        return result
    #
    # @property
    # def full_path(self) -> str:
    #     """String full path to the channel directory"""
    #     result = core.BNChannelGetFullPath(self.handle)
    #     assert result is not None
    #     return result

    @property
    def extensions(self) -> List[Extension]:
        """List of Extension objects contained within this channel"""
        extension_list = []
        count = ctypes.c_ulonglong(0)
        result = core.BNChannelGetExtensions(self.handle, count)
        assert result is not None, "core.BNChannelGetExtensions returned None"
        try:
            for i in range(count.value):
                extension_ref = core.BNNewExtensionReference(result[i])
                assert extension_ref is not None, "core.BNNewExtensionReference returned None"
                extension_list.append(Extension(extension_ref))
            return extension_list
        finally:
            core.BNFreeChannelExtensionList(result)

    # def get_extension_by_path(self, path: str) -> Optional[Extension]:
    #     """
    #     Get an extension by its path
    #
    #     :param str path: Path to the extension
    #     :return: Extension if found, None otherwise
    #     :rtype: Extension or None
    #     """
    #     extension = core.BNChannelGetExtensionByPath(self.handle, path)
    #     if extension is None:
    #         return None
    #     extension_ref = core.BNNewExtensionReference(extension)
    #     assert extension_ref is not None, "core.BNNewExtensionReference returned None"
    #     return Extension(extension_ref)


class ExtensionManager:
    """
    ``ExtensionManager`` manages the list of channels and their extensions
    """

    def __init__(self):
        binaryninja._init_plugins()
        self.handle = core.BNGetExtensionManager()

    # def __getitem__(self, channel_name: str) -> Channel:
    #     for channel in self.channels:
    #         if channel_name == channel.name:
    #             return channel
    #     raise KeyError()
    #
    # def check_for_updates(self) -> bool:
    #     """Check for updates for all managed Channel objects"""
    #     return core.BNExtensionManagerCheckForUpdates(self.handle)
    #
    # def fetch_channels_async(self) -> bool:
    #     """Asynchronously fetch channels and their extensions"""
    #     return core.BNExtensionManagerFetchChannelsAsync(self.handle)

    @property
    def channels(self) -> List[Channel]:
        """List of Channel objects being managed"""
        result = []
        count = ctypes.c_ulonglong(0)
        channels = core.BNExtensionManagerGetChannels(count)
        assert channels is not None, "core.BNExtensionManagerGetChannels returned None"
        try:
            for i in range(count.value):
                channel_ref = core.BNNewChannelReference(channels[i])
                assert channel_ref is not None, "core.BNNewChannelReference returned None"
                result.append(Channel(channel_ref))
            return result
        finally:
            core.BNFreeExtensionManagerChannelsList(channels)

    # @property
    # def extensions(self) -> Dict[str, List[Extension]]:
    #     """List of all Extensions in each channel"""
    #     extension_list = {}
    #     for channel in self.channels:
    #         extension_list[channel.name] = channel.extensions
    #     return extension_list
    #
    # def add_channel(self, url: Optional[str] = None, name: Optional[str] = None, is_user_channel: bool = False) -> bool:
    #     """
    #     ``add_channel`` adds a new extension channel for the manager to track.
    #
    #     :param str url: URL to the extensions.json containing the records for this channel
    #     :param str name: name to identify the channel
    #     :param bool is_user_channel: Whether this is a user channel (special handling)
    #     :return: Boolean value True if the channel was successfully added, False otherwise.
    #     :rtype: Boolean
    #     :Example:
    #
    #         >>> mgr = ExtensionManager()
    #         >>> mgr.add_channel("https://binary.ninja/v2/manifests/c934585c-5227-4b3e-ab02-6b4dab1d5916/extensions", "official")
    #         True
    #         >>> mgr.check_for_updates()
    #         >>>
    #     """
    #     if not isinstance(url, str) or not isinstance(name, str):
    #         raise ValueError("Expected url or name to be of type str.")
    #
    #     return core.BNExtensionManagerAddChannel(self.handle, url, name, is_user_channel)
    #
    # def get_channel_by_name(self, name: str) -> Optional[Channel]:
    #     """
    #     Get a channel by its name
    #
    #     :param str name: Name of the channel
    #     :return: Channel if found, None otherwise
    #     :rtype: Channel or None
    #     """
    #     channel = core.BNExtensionManagerGetChannelByName(self.handle, name)
    #     if channel is None:
    #         return None
    #     channel_ref = core.BNNewChannelReference(channel)
    #     assert channel_ref is not None, "core.BNNewChannelReference returned None"
    #     return Channel(channel_ref)