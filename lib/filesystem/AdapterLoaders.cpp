#include "StdInc.h"
#include "AdapterLoaders.h"

#include "../JsonNode.h"

CMappedFileLoader::CMappedFileLoader(const std::string & mountPoint, const JsonNode &config)
{
	for(auto entry : config.Struct())
	{
		fileList[ResourceID(mountPoint + entry.first)] = ResourceID(mountPoint + entry.second.String());
	}
}

std::unique_ptr<CInputStream> CMappedFileLoader::load(const ResourceID & resourceName) const
{
	return CResourceHandler::get()->load(fileList.at(resourceName));
}

bool CMappedFileLoader::existsResource(const ResourceID & resourceName) const
{
	return fileList.count(resourceName) != 0;
}

std::string CMappedFileLoader::getMountPoint() const
{
	return ""; // does not have any meaning with this type of data source
}

boost::optional<std::string> CMappedFileLoader::getResourceName(const ResourceID & resourceName) const
{
	return CResourceHandler::get()->getResourceName(fileList.at(resourceName));
}

std::unordered_set<ResourceID> CMappedFileLoader::getFilteredFiles(std::function<bool(const ResourceID &)> filter) const
{
	std::unordered_set<ResourceID> foundID;

	for (auto & file : fileList)
	{
		if (filter(file.first))
			foundID.insert(file.first);
	}
	return foundID;
}

CFilesystemList::CFilesystemList()
{
	loaders = new std::vector<std::unique_ptr<ISimpleResourceLoader> >;
}

std::unique_ptr<CInputStream> CFilesystemList::load(const ResourceID & resourceName) const
{
	// load resource from last loader that have it (last overriden version)
	for (auto & loader : boost::adaptors::reverse(*loaders))
	{
		if (loader->existsResource(resourceName))
			return loader->load(resourceName);
	}

	throw std::runtime_error("Resource with name " + resourceName.getName() + " and type "
		+ EResTypeHelper::getEResTypeAsString(resourceName.getType()) + " wasn't found.");
}

bool CFilesystemList::existsResource(const ResourceID & resourceName) const
{
	return !getResourcesWithName(resourceName).empty();
}

std::string CFilesystemList::getMountPoint() const
{
	return "";
}

boost::optional<std::string> CFilesystemList::getResourceName(const ResourceID & resourceName) const
{
	if (existsResource(resourceName))
		return getResourcesWithName(resourceName).back()->getResourceName(resourceName);
	return boost::optional<std::string>();
}

std::unordered_set<ResourceID> CFilesystemList::getFilteredFiles(std::function<bool(const ResourceID &)> filter) const
{
	std::unordered_set<ResourceID> ret;

	for (auto & loader : *loaders)
		for (auto & entry : loader->getFilteredFiles(filter))
			ret.insert(entry);

	return ret;
}

bool CFilesystemList::createResource(std::string filename, bool update)
{
	logGlobal->traceStream()<< "Creating " << filename;
	for (auto & loader : boost::adaptors::reverse(*loaders))
	{
		if (writeableLoaders.count(loader.get()) != 0                       // writeable,
			&& loader->createResource(filename, update))          // successfully created
		{
			// Check if resource was created successfully. Possible reasons for this to fail
			// a) loader failed to create resource (e.g. read-only FS)
			// b) in update mode, call with filename that does not exists
			assert(load(ResourceID(filename)));

			logGlobal->traceStream()<< "Resource created successfully";
			return true;
		}
	}
	logGlobal->traceStream()<< "Failed to create resource";
	return false;
}

std::vector<const ISimpleResourceLoader *> CFilesystemList::getResourcesWithName(const ResourceID & resourceName) const
{
	std::vector<const ISimpleResourceLoader *> ret;

	for (auto & loader : *loaders)
		boost::range::copy(loader->getResourcesWithName(resourceName), std::back_inserter(ret));

	return ret;
}

void CFilesystemList::addLoader(ISimpleResourceLoader * loader, bool writeable)
{
	loaders->push_back(std::unique_ptr<ISimpleResourceLoader>(loader));
	if (writeable)
		writeableLoaders.insert(loader);
}

ISimpleResourceLoader * CResourceHandler::get()
{
	if(resourceLoader != nullptr)
	{
		return resourceLoader;
	}
	else
	{
		std::stringstream string;
		string << "Error: Resource loader wasn't initialized. "
			   << "Make sure that you set one via FilesystemFactory::initialize";
		throw std::runtime_error(string.str());
	}
}