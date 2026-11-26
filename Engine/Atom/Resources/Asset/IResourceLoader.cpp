/**
 * @file           : IResourceLoader.cpp
 * @brief          : Asset-layer error mapping helpers.
**/

#include "IResourceLoader.hpp"

namespace atom::asset {

namespace {

[[nodiscard]] auto AssetResultCode(const AssetResult result) -> ErrorCode {
    switch (result) {
    case AssetResult::Success:
        // Not a failure; see fs::ToError for the same reasoning.
        return ErrorCode::Internal;
    case AssetResult::InvalidId:
        return ErrorCode::AssetInvalidId;
    case AssetResult::NoLoader:
        return ErrorCode::AssetNoLoader;
    case AssetResult::NotFound:
        return ErrorCode::AssetNotFound;
    case AssetResult::NotFile:
        return ErrorCode::AssetNotFile;
    case AssetResult::IoError:
        return ErrorCode::AssetIoError;
    case AssetResult::TypeMismatch:
        return ErrorCode::AssetTypeMismatch;
    case AssetResult::LoadFailed:
        return ErrorCode::AssetLoadFailed;
    }
    return ErrorCode::Internal;
}

} // namespace

auto ToError(const AssetResult result, std::string_view operation) -> Error {
    return Error{AssetResultCode(result), Module::Asset, std::string{operation}};
}

auto ToError(const AssetResult result, const ResourceId& id, const std::string_view operation) -> Error {
    auto error = ToError(result, operation);
    error.WithResource(id.Path().String());
    if (const auto variant = id.Variant(); !variant.empty())
        error.WithDetail(std::string{variant});
    return error;
}

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
