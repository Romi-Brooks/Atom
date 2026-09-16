/**
 * @file           : IResourceLoader.cpp
 * @brief          : Shared AssetResult mapping helpers.
**/

#include "IResourceLoader.hpp"

namespace atom::asset {

auto MapFsResult(const fs::Result result) -> AssetResult {
    switch (result) {
    case fs::Result::Success:
        return AssetResult::Success;
    case fs::Result::InvalidPath:
        return AssetResult::InvalidId;
    case fs::Result::NotFound:
        return AssetResult::NotFound;
    case fs::Result::NotFile:
        return AssetResult::NotFile;
    case fs::Result::NotDirectory:
    case fs::Result::OutsideRoot:
    case fs::Result::OutOfRange:
    case fs::Result::IoError:
        return AssetResult::IoError;
    }
    return AssetResult::LoadFailed;
}

} // namespace atom::asset
