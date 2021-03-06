#include <base/info/stream.h>
#include <base/publisher/stream.h>
#include "file_userdata.h"
#include "file_private.h"

FileUserdataSets::FileUserdataSets()
{

}

FileUserdataSets::~FileUserdataSets()
{

}

bool FileUserdataSets::Set(ov::String userdata_id, std::shared_ptr<FileUserdata> userdata)
{
	_userdata_sets[userdata_id] = userdata;

	return true;
}

std::shared_ptr<FileUserdata> FileUserdataSets::GetAt(uint32_t index)
{
	auto iter( _userdata_sets.begin() );
    std::advance( iter, index );

    if(iter == _userdata_sets.end())
    {
    	return nullptr;
    }

	return iter->second;
}

ov::String FileUserdataSets::GetKeyAt(uint32_t index)
{
	auto iter( _userdata_sets.begin() );
    std::advance( iter, index );

    if(iter == _userdata_sets.end())
    {
    	return nullptr;
    }

	return iter->first;	
}

std::shared_ptr<FileUserdata> FileUserdataSets::GetByKey(ov::String key)
{
	auto iter = _userdata_sets.find(key);
	if(iter == _userdata_sets.end())
	{
		return nullptr;
	}

	return iter->second;
}


std::shared_ptr<FileUserdata> FileUserdataSets::GetBySessionId(session_id_t session_id)
{
	for ( auto &item : _userdata_sets )
	{
		auto userdata = item.second;

		if(userdata->GetSessionId() == session_id)
			return userdata;
	}

	return nullptr;
}

uint32_t FileUserdataSets::GetCount()
{
	return _userdata_sets.size();	
}