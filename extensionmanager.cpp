#include "binaryninjaapi.h"

using namespace BinaryNinja;
using namespace std;

#define RETURN_STRING(s) \
	do \
	{ \
		char* contents = (char*)(s); \
		string result(contents); \
		BNFreeString(contents); \
		return result; \
	} while (0)

// ExtensionVersion implementation
// ExtensionVersion::ExtensionVersion(BNExtensionVersion* version)
// {
// 	m_object = version;
// }

// string ExtensionVersion::GetVersionString() const
// {
// 	RETURN_STRING(BNExtensionVersionGetVersionString(m_object));
// }

// string ExtensionVersion::GetLongDescription() const
// {
// 	RETURN_STRING(BNExtensionVersionGetLongDescription(m_object));
// }

// string ExtensionVersion::GetChangelog() const
// {
// 	RETURN_STRING(BNExtensionVersionGetChangelog(m_object));
// }

// BNVersionInfo ExtensionVersion::GetMinimumVersionInfo() const
// {
// 	return BNExtensionVersionGetMinimumVersionInfo(m_object);
// }

// BNVersionInfo ExtensionVersion::GetMaximumVersionInfo() const
// {
// 	return BNExtensionVersionGetMaximumVersionInfo(m_object);
// }

// string ExtensionVersion::GetDependencies() const
// {
// 	RETURN_STRING(BNExtensionVersionGetDependencies(m_object));
// }

// string ExtensionVersion::GetDownloadUrl(bool contributeToInstallCounts) const
// {
// 	RETURN_STRING(BNExtensionVersionGetDownloadUrl(m_object, contributeToInstallCounts));
// }

// bool ExtensionVersion::InstallDependencies() const
// {
// 	return BNExtensionVersionInstallDependencies(m_object);
// }

// Extension implementation
Extension::Extension(BNExtension* extension)
{
	m_object = extension;
}

string Extension::GetPath() const
{
	RETURN_STRING(BNExtensionGetPath(m_object));
}

bool Extension::IsInstalled() const
{
    return BNExtensionIsInstalled(m_object);
}

bool Extension::IsEnabled() const
{
    return BNExtensionIsEnabled(m_object);
}

bool Extension::Enable()
{
    return BNExtensionEnable(m_object);
}

bool Extension::Disable()
{
    return BNExtensionDisable(m_object);
}

string Extension::GetAuthor() const
{
    RETURN_STRING(BNExtensionGetAuthor(m_object));
}

string Extension::GetDescription() const
{
    RETURN_STRING(BNExtensionGetDescription(m_object));
}

string Extension::GetProjectUrl() const
{
    RETURN_STRING(BNExtensionGetProjectUrl(m_object));
}

// PluginStatus Extension::GetPluginStatus() const
// {
// 	return BNExtensionGetPluginStatus(m_object);
// }

string Extension::GetName() const
{
	RETURN_STRING(BNExtensionGetName(m_object));
}

string Extension::GetPluginType() const
{
	RETURN_STRING(BNExtensionGetPluginType(m_object));
}

// vector<Ref<ExtensionVersion>> Extension::GetVersions() const
// {
// 	size_t count;
// 	BNExtensionVersion** versions = BNExtensionGetVersions(m_object, &count);
// 	vector<Ref<ExtensionVersion>> result;
// 	result.reserve(count);
// 	for (size_t i = 0; i < count; i++)
// 		result.push_back(new ExtensionVersion(BNNewExtensionVersionReference(versions[i])));
// 	BNFreeExtensionVersionList(versions);
// 	return result;
// }

// Ref<ExtensionVersion> Extension::GetCurrentVersion() const
// {
// 	BNExtensionVersion* version = BNExtensionGetCurrentVersion(m_object);
// 	if (!version)
// 		return nullptr;
// 	return new ExtensionVersion(version);
// }

// Ref<ExtensionVersion> Extension::GetLatestVersion() const
// {
// 	BNExtensionVersion* version = BNExtensionGetLatestVersion(m_object);
// 	if (!version)
// 		return nullptr;
// 	return new ExtensionVersion(version);
// }

string Extension::GetChannelName() const
{
	RETURN_STRING(BNExtensionGetChannelName(m_object));
}

// bool Extension::IsBeingDeleted() const
// {
// 	return BNExtensionIsBeingDeleted(m_object);
// }

// bool Extension::IsBeingUpdated() const
// {
// 	return BNExtensionIsBeingUpdated(m_object);
// }

bool Extension::IsRunning() const
{
	return BNExtensionIsRunning(m_object);
}

// bool Extension::IsUpdatePending() const
// {
// 	return BNExtensionIsUpdatePending(m_object);
// }

// bool Extension::IsDisablePending() const
// {
// 	return BNExtensionIsDisablePending(m_object);
// }

// bool Extension::IsDeletePending() const
// {
// 	return BNExtensionIsDeletePending(m_object);
// }

// bool Extension::IsUpdateAvailable() const
// {
// 	return BNExtensionIsUpdateAvailable(m_object);
// }

// bool Extension::AreDependenciesBeingInstalled() const
// {
// 	return BNExtensionAreDependenciesBeingInstalled(m_object);
// }

// bool Extension::Uninstall()
// {
// 	return BNExtensionUninstall(m_object);
// }

// bool Extension::Install(Ref<ExtensionVersion> version)
// {
// 	BNExtensionVersion* versionObj = version ? version->GetObject() : nullptr;
// 	return BNExtensionInstall(m_object, versionObj);
// }

// bool Extension::InstallDependencies()
// {
// 	return BNExtensionInstallDependencies(m_object);
// }

// bool Extension::Enable()
// {
// 	return BNExtensionEnable(m_object);
// }

// bool Extension::Update(Ref<ExtensionVersion> version)
// {
// 	BNExtensionVersion* versionObj = version ? version->GetObject() : nullptr;
// 	return BNExtensionUpdate(m_object, versionObj);
// }

// bool Extension::Disable()
// {
// 	return BNExtensionDisable(m_object);
// }

// ExtensionChannel implementation
ExtensionChannel::ExtensionChannel(BNExtensionChannel* c)
{
	m_object = c;
}

// string ExtensionChannel::GetUrl() const
// {
// 	RETURN_STRING(BNChannelGetUrl(m_object));
// }

// string ExtensionChannel::GetName() const
// {
// 	RETURN_STRING(BNChannelGetName(m_object));
// }

vector<Ref<Extension>> ExtensionChannel::GetExtensions() const
{
	vector<Ref<Extension>> extensions;
	size_t count = 0;
	BNExtension** extensionsPtr = BNChannelGetExtensions(m_object, &count);
	extensions.reserve(count);
	for (size_t i = 0; i < count; i++)
		extensions.push_back(new Extension(BNNewExtensionReference(extensionsPtr[i])));
	BNFreeChannelExtensionList(extensionsPtr);
	return extensions;
}

// Ref<Extension> ExtensionChannel::GetExtensionByPath(const string& pluginPath)
// {
// 	return new Extension(BNChannelGetExtensionByPath(m_object, pluginPath.c_str()));
// }

// string ExtensionChannel::GetFullPath() const
// {
// 	RETURN_STRING(BNChannelGetFullPath(m_object));
// }

// ExtensionManager implementation
ExtensionManager::ExtensionManager(BNExtensionManager* mgr)
{
	m_object = mgr;
}

ExtensionManager::ExtensionManager()
{
	m_object = BNGetExtensionManager();
}

// bool ExtensionManager::CheckForUpdates()
// {
// 	return BNExtensionManagerCheckForUpdates(m_object);
// }

// bool ExtensionManager::FetchChannelsAsync()
// {
// 	return BNExtensionManagerFetchChannelsAsync(m_object);
// }

vector<Ref<ExtensionChannel>> ExtensionManager::GetChannels()
{
	vector<Ref<ExtensionChannel>> channels;
	size_t count = 0;
	BNExtensionChannel** channelsPtr = BNExtensionManagerGetChannels(&count);
	for (size_t i = 0; i < count; i++)
		channels.push_back(new ExtensionChannel(BNNewChannelReference(channelsPtr[i])));
	BNFreeExtensionManagerChannelsList(channelsPtr);
	return channels;
}

// Ref<ExtensionChannel> ExtensionManager::GetChannelByName(const std::string& name)
// {
// 	return new ExtensionChannel(BNExtensionManagerGetChannelByName(m_object, name.c_str()));
// }