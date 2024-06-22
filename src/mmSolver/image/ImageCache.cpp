/*
 * Copyright (C) 2024 David Cattermole.
 *
 * This file is part of mmSolver.
 *
 * mmSolver is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * mmSolver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with mmSolver.  If not, see <https://www.gnu.org/licenses/>.
 * ====================================================================
 *
 */

// Helpful code:
// https://github.com/david-cattermole/cpp-utilities/blob/master/include/fileSystemUtils.h
// https://github.com/david-cattermole/cpp-utilities/blob/master/include/hashUtils.h

#include "ImageCache.h"

// Get M_PI constant
#define _USE_MATH_DEFINES
#include <cmath>

// STL
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>

// Maya
#include <maya/MImage.h>
#include <maya/MString.h>

// Maya Viewport 2.0
#include <maya/MTextureManager.h>

// MM Solver
#include <mmcore/lib.h>

#include "ImagePixelData.h"
#include "PixelDataType.h"
#include "TextureData.h"
#include "image_io.h"
#include "mmSolver/mayahelper/maya_utils.h"
#include "mmSolver/render/shader/shader_utils.h"
#include "mmSolver/shape/constant_texture_data.h"
#include "mmSolver/utilities/hash_utils.h"
#include "mmSolver/utilities/number_utils.h"
#include "mmSolver/utilities/path_utils.h"
#include "mmSolver/utilities/string_utils.h"

namespace mmsolver {
namespace image {

MTexture *read_texture_image_file(MHWRender::MTextureManager *texture_manager,
                                  ImageCache &image_cache, MImage &temp_image,
                                  const MString &file_pattern,
                                  const MString &file_path,
                                  const bool do_texture_update) {
    assert(texture_manager != nullptr);

    const bool verbose = false;
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache: read_texture_image_file:"
                      << " file_path=" << file_path.asChar());

    MString resolved_file_path = file_path;
    MStatus status = mmpath::resolve_input_file_path(resolved_file_path);
    if (status != MS::kSuccess) {
        MMSOLVER_MAYA_VRB("mmsolver::ImageCache: read_texture_image_file:"
                          << " file does not exist \""
                          << resolved_file_path.asChar() << "\".");
        return nullptr;
    }

    const std::string key = std::string(resolved_file_path.asChar());
    TextureData texture_data = image_cache.gpu_find_item(key);

    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache: read_texture_image_file: findTexture: "
        << texture_data.is_valid());
    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache: read_texture_image_file: do_texture_update="
        << do_texture_update);
    if (texture_data.is_valid() && (do_texture_update == false)) {
        MMSOLVER_MAYA_VRB("mmsolver::ImageCache: read_texture_image_file DONE1:"
                          << " texture=" << texture_data.texture());
        return texture_data.texture();
    }

    // TODO: We should test if the file exists, then cache
    // result. This avoids us having to check the disk each time we
    // hit a frame that is outside the frame range of the image
    // sequence. This would mean that we would then need to be able to
    // flush that cached result - for example if the image sequence
    // changes. Alternatively, we could could pre-cache the file path
    // existence as soon as the user changes the file path. Another
    // approach would be to expose a Maya command that would allow
    // Python user code to add the list of valid images into the
    // cache.

    ImagePixelData image_pixel_data = image_cache.cpu_find_item(key);

    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t num_channels = 4;
    uint8_t bytes_per_channel = 0;
    MHWRender::MRasterFormat texture_format;
    PixelDataType pixel_data_type = PixelDataType::kUnknown;
    void *maya_owned_pixel_data = nullptr;

    const bool image_pixel_data_valid = image_pixel_data.is_valid();
    if (image_pixel_data_valid) {
        maya_owned_pixel_data = image_pixel_data.pixel_data();
        width = image_pixel_data.width();
        height = image_pixel_data.height();
        num_channels = image_pixel_data.num_channels();
        pixel_data_type = image_pixel_data.pixel_data_type();
        bytes_per_channel =
            convert_pixel_data_type_to_bytes_per_channel(pixel_data_type);

        if (pixel_data_type == PixelDataType::kU8) {
            // Assumes the 8-bit data is "RGBA".
            texture_format = MHWRender::kR8G8B8A8_UNORM;
        } else if (pixel_data_type == PixelDataType::kF32) {
            texture_format = MHWRender::kR32G32B32A32_FLOAT;
        }

    } else {
        status =
            read_image_file(temp_image, resolved_file_path, width, height,
                            num_channels, bytes_per_channel, texture_format,
                            pixel_data_type, maya_owned_pixel_data);
        if (status != MS::kSuccess) {
            return nullptr;
        }

        // TODO: any image manipulations required can be done here.
        // TODO: Apply colour correction via OIIO.
        // // image.verticalFlip();
    }

    if (!maya_owned_pixel_data) {
        MMSOLVER_MAYA_ERR("mmsolver::ImageCache: read_texture_image_file: "
                          << "Invalid pixel data!");
        return nullptr;
    }

    ImagePixelData gpu_image_pixel_data =
        ImagePixelData(static_cast<void *>(maya_owned_pixel_data), width,
                       height, num_channels, pixel_data_type);

    const std::string group_name = std::string(file_pattern.asChar());
    const uint64_t group_hash = mmsolver::hash::make_hash(group_name);

    texture_data = image_cache.gpu_insert_item(texture_manager, group_name, key,
                                               gpu_image_pixel_data);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache: read_texture_image_file: "
                      << "gpu_inserted=" << texture_data.texture());

    // Duplicate the Maya-owned pixel data for our image cache.
    const size_t pixel_data_byte_count =
        width * height * num_channels * bytes_per_channel;
    image_pixel_data = ImagePixelData();
    const bool allocated_ok = image_pixel_data.allocate_pixels(
        width, height, num_channels, pixel_data_type);
    if (allocated_ok == false) {
        MMSOLVER_MAYA_ERR("mmsolver::ImageCache: read_texture_image_file: "
                          << "Could not allocate pixel data!");
        return nullptr;
    }
    assert(image_pixel_data.is_valid() == true);
    assert(image_pixel_data.byte_count() == pixel_data_byte_count);
    std::memcpy(image_pixel_data.pixel_data(), maya_owned_pixel_data,
                pixel_data_byte_count);

    const bool cpu_inserted =
        image_cache.cpu_insert_item(group_name, key, image_pixel_data);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache: read_texture_image_file: "
                      << "cpu_inserted=" << cpu_inserted);

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache: read_texture_image_file DONE2:"
                      << " texture=" << texture_data.texture());

    return texture_data.texture();
}

void ImageCache::set_gpu_capacity_bytes(
    MHWRender::MTextureManager *texture_manager, const size_t value) {
    const bool verbose = false;
    m_gpu_capacity_bytes = value;
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::set_gpu_capacity_bytes: "
                      << "m_gpu_capacity_bytes=" << m_gpu_capacity_bytes);

    // Because we must always ensure our used memory is less than
    // the given capacity.
    //
    // If we are at capacity remove the least recently used items
    // until our capacity is under 'new_used_bytes' or we reach the minimum
    // number of items
    while (!m_gpu_item_map.empty() &&
           (m_gpu_item_map.size() > m_gpu_item_count_minumum) &&
           (m_gpu_used_bytes > m_gpu_capacity_bytes)) {
        const CacheEvictionResult result =
            ImageCache::gpu_evict_one_item(texture_manager);
        if (result != CacheEvictionResult::kSuccess) {
            break;
        }
    }
}

void ImageCache::set_cpu_capacity_bytes(const size_t value) {
    const bool verbose = false;
    m_cpu_capacity_bytes = value;
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::set_cpu_capacity_bytes: "
                      << "m_cpu_capacity_bytes=" << m_cpu_capacity_bytes);

    // Because we must always ensure our used memory is less than
    // the given capacity.
    //
    // If we are at capacity remove the least recently used items
    // until our capacity is under 'new_used_bytes' or we reach the minimum
    // number of items
    while (!m_cpu_item_map.empty() &&
           (m_cpu_item_map.size() > m_cpu_item_count_minumum) &&
           (m_cpu_used_bytes > m_cpu_capacity_bytes)) {
        const CacheEvictionResult result = ImageCache::cpu_evict_one_item();
        if (result != CacheEvictionResult::kSuccess) {
            break;
        }
    }
}

inline std::string generate_cache_brief(const char *prefix_str,
                                        const size_t item_count,
                                        const size_t item_min_count,
                                        const size_t capacity_bytes,
                                        const size_t used_bytes) {
    const size_t capacity_megabytes = capacity_bytes / BYTES_TO_MEGABYTES;
    const size_t used_megabytes = used_bytes / BYTES_TO_MEGABYTES;

    std::string capacity_megabytes_str =
        mmstring::numberToStringWithCommas(capacity_megabytes);

    std::string used_megabytes_str =
        mmstring::numberToStringWithCommas(used_megabytes);

    double used_percent = 0.0;
    if (capacity_bytes > 0) {
        used_percent = (static_cast<double>(used_bytes) /
                        static_cast<double>(capacity_bytes)) *
                       100.0;
    }

    std::stringstream ss;
    ss << prefix_str << "item_count=" << item_count << " items "
       << "| minimum=" << item_min_count << " items "
       << "| used=" << used_megabytes_str << "MB "
       << "| capacity=" << capacity_megabytes_str << "MB "
       << "| percent=" << used_percent << '%';
    return ss.str();
}

MString ImageCache::generate_cache_brief_text() const {
    std::string gpu_cache_text = generate_cache_brief(
        "GPU cache | ", m_gpu_item_map.size(), m_gpu_item_count_minumum,
        m_gpu_capacity_bytes, m_gpu_used_bytes);
    std::string cpu_cache_text = generate_cache_brief(
        "CPU cache | ", m_cpu_item_map.size(), m_cpu_item_count_minumum,
        m_cpu_capacity_bytes, m_cpu_used_bytes);

    std::stringstream ss;
    ss << gpu_cache_text << std::endl << cpu_cache_text << std::endl;

    std::string string = ss.str();
    MString mstring = MString(string.c_str());
    return mstring;
}

void ImageCache::print_cache_brief() const {
    std::string gpu_cache_text = generate_cache_brief(
        "GPU cache | ", m_gpu_item_map.size(), m_gpu_item_count_minumum,
        m_gpu_capacity_bytes, m_gpu_used_bytes);
    std::string cpu_cache_text = generate_cache_brief(
        "CPU cache | ", m_cpu_item_map.size(), m_gpu_item_count_minumum,
        m_cpu_capacity_bytes, m_cpu_used_bytes);

    MMSOLVER_MAYA_INFO(
        "mmsolver::ImageCache::print_cache_brief: " << gpu_cache_text);
    MMSOLVER_MAYA_INFO(
        "mmsolver::ImageCache::print_cache_brief: " << cpu_cache_text);
    return;
}

void ImageCache::gpu_group_names(GPUVectorString &out_group_names) const {
    const bool verbose = false;
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_group_names: ");

    out_group_names.clear();
    out_group_names.reserve(m_gpu_group_names_set.size());
    for (auto it = m_gpu_group_names_set.begin();
         it != m_gpu_group_names_set.end(); it++) {
        GPUCacheString value = *it;
        out_group_names.push_back(value);
    }
    // The set is unordered, so lets make the output of this function
    // consistent for end users.
    std::sort(out_group_names.begin(), out_group_names.end());

    return;
}

void ImageCache::cpu_group_names(CPUVectorString &out_group_names) const {
    const bool verbose = false;
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_group_names: ");

    out_group_names.clear();
    out_group_names.reserve(m_cpu_group_names_set.size());
    for (auto it = m_cpu_group_names_set.begin();
         it != m_cpu_group_names_set.end(); it++) {
        CPUCacheString value = *it;
        out_group_names.push_back(value);
    }
    // The set is unordered, so lets make the output of this function
    // consistent for end users.
    std::sort(out_group_names.begin(), out_group_names.end());

    return;
}

size_t ImageCache::gpu_group_item_count(
    const GPUCacheString &group_name) const {
    const GPUGroupKey group_key = mmsolver::hash::make_hash(group_name);
    return ImageCache::gpu_group_item_count(group_key);
}

size_t ImageCache::gpu_group_item_count(const GPUGroupKey group_key) const {
    const auto group_search = m_gpu_group_map.find(group_key);
    const bool group_found = group_search != m_gpu_group_map.end();
    if (!group_found) {
        return 0;
    }

    const GPUGroupSet &values_set = group_search->second;
    return values_set.size();
}

size_t ImageCache::cpu_group_item_count(
    const CPUCacheString &group_name) const {
    const GPUGroupKey group_key = mmsolver::hash::make_hash(group_name);
    return ImageCache::cpu_group_item_count(group_key);
}

size_t ImageCache::cpu_group_item_count(const CPUGroupKey group_key) const {
    const auto group_search = m_cpu_group_map.find(group_key);
    const bool group_found = group_search != m_cpu_group_map.end();
    if (!group_found) {
        return 0;
    }

    const CPUGroupSet &values_set = group_search->second;
    return values_set.size();
}

bool ImageCache::gpu_group_item_names(
    const GPUCacheString &group_name,
    GPUVectorString &out_group_item_names) const {
    const GPUGroupKey group_key = mmsolver::hash::make_hash(group_name);
    return ImageCache::gpu_group_item_names(group_key, out_group_item_names);
}

bool ImageCache::gpu_group_item_names(
    const GPUGroupKey group_key, GPUVectorString &out_group_item_names) const {
    out_group_item_names.clear();
    const auto group_search = m_gpu_group_map.find(group_key);
    const bool group_found = group_search != m_gpu_group_map.end();
    if (!group_found) {
        return false;
    }

    const GPUGroupSet &values_set = group_search->second;
    out_group_item_names.reserve(values_set.size());
    for (auto it = values_set.begin(); it != values_set.end(); it++) {
        GPUCacheString value = *it;
        out_group_item_names.push_back(value);
    }
    // The set is unordered, so lets make the output of this function
    // consistent for end users.
    std::sort(out_group_item_names.begin(), out_group_item_names.end());

    return true;
}

bool ImageCache::cpu_group_item_names(
    const CPUCacheString &group_name,
    CPUVectorString &out_group_item_names) const {
    const CPUGroupKey group_key = mmsolver::hash::make_hash(group_name);
    return ImageCache::cpu_group_item_names(group_key, out_group_item_names);
}

bool ImageCache::cpu_group_item_names(
    const CPUGroupKey group_key, CPUVectorString &out_group_item_names) const {
    out_group_item_names.clear();
    const auto group_search = m_cpu_group_map.find(group_key);
    const bool group_found = group_search != m_cpu_group_map.end();
    if (!group_found) {
        return false;
    }

    const CPUGroupSet &values_set = group_search->second;
    out_group_item_names.reserve(values_set.size());
    for (auto it = values_set.begin(); it != values_set.end(); it++) {
        CPUCacheString value = *it;
        out_group_item_names.push_back(value);
    }
    // The set is unordered, so lets make the output of this function
    // consistent for end users.
    std::sort(out_group_item_names.begin(), out_group_item_names.end());

    return true;
}

bool ImageCache::gpu_insert_group(const GPUGroupKey group_key,
                                  const GPUCacheString &group_name,
                                  const GPUCacheString &file_path) {
    const GPUGroupMapIt group_search = m_gpu_group_map.find(group_key);
    const bool group_found = group_search != m_gpu_group_map.end();
    if (!group_found) {
        GPUGroupSet values_set;
        values_set.insert(file_path);

        const auto group_map_pair =
            m_gpu_group_map.insert(std::make_pair(group_key, values_set));
        const bool group_map_ok = group_map_pair.second;
        assert(group_map_ok == true);

        // First time inserting into the set, it won't exist yet.
        const auto set_pair = m_gpu_group_names_set.insert(group_name);
        const bool set_ok = set_pair.second;
        assert(set_ok == true);
    } else {
        GPUGroupSet &values_set = group_search->second;
        const GPUGroupSetIt value_search = values_set.find(group_name);
        const bool value_found = value_search != values_set.end();
        if (!value_found) {
            const auto values_set_pair = values_set.insert(file_path);
            const bool values_set_ok = values_set_pair.second;
            assert(values_set_ok == true);
        }
    }
    return true;
}

bool ImageCache::cpu_insert_group(const CPUGroupKey group_key,
                                  const CPUCacheString &group_name,
                                  const CPUCacheString &file_path) {
    const CPUGroupMapIt group_search = m_cpu_group_map.find(group_key);
    const bool group_found = group_search != m_cpu_group_map.end();
    if (!group_found) {
        CPUGroupSet values_set;
        values_set.insert(file_path);

        const auto group_map_pair =
            m_cpu_group_map.insert(std::make_pair(group_key, values_set));
        const bool group_map_ok = group_map_pair.second;
        assert(group_map_ok == true);

        // First time inserting into the set, it won't exist yet.
        const auto set_pair = m_cpu_group_names_set.insert(group_name);
        const bool set_ok = set_pair.second;
        assert(set_ok == true);
    } else {
        CPUGroupSet &values_set = group_search->second;
        const CPUGroupSetIt value_search = values_set.find(group_name);
        const bool value_found = value_search != values_set.end();
        if (!value_found) {
            const auto values_set_pair = values_set.insert(file_path);
            const bool values_set_ok = values_set_pair.second;
            assert(values_set_ok == true);
        }
    }
    return true;
}

static void update_texture(MTexture *texture,
                           const ImageCache::CPUCacheValue &image_pixel_data) {
    const bool verbose = false;

    // No need for MIP-maps.
    const bool generate_mip_maps = false;

    // TODO: If the 'texture_desc' is different than the
    // current texture, it cannot be updated, and must be
    // released and a new texture created instead.

    // The default value of this argument is 0. This means to
    // use the texture's "width * number of bytes per pixel".
    uint32_t rowPitch = 0;

    MHWRender::MTextureUpdateRegion *region = nullptr;
    void *pixel_data = image_pixel_data.pixel_data();
    MStatus status =
        texture->update(pixel_data, generate_mip_maps, rowPitch, region);
    CHECK_MSTATUS(status);
}

ImageCache::GPUCacheValue ImageCache::gpu_insert_item(
    MHWRender::MTextureManager *texture_manager,
    const GPUCacheString &group_name, const GPUCacheString &file_path,
    const ImageCache::CPUCacheValue &image_pixel_data) {
    assert(texture_manager != nullptr);
    assert(image_pixel_data.is_valid());
    const bool verbose = false;

    const GPUGroupKey key = mmsolver::hash::make_hash(file_path);
    const GPUGroupKey group_key = mmsolver::hash::make_hash(group_name);

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_insert_item: "
                      << "key=" << key << " group_name=" << group_name.c_str()
                      << " file_path=" << file_path.c_str());

    GPUCacheValue texture_data = GPUCacheValue();
    const ImageCache::GPUMapIt search = m_gpu_item_map.find(key);
    const bool found = search != m_gpu_item_map.end();
    if (!found) {
        // If we are at capacity, make room for new entry.
        const size_t image_data_size = image_pixel_data.byte_count();

        const CacheEvictionResult evict_result =
            ImageCache::gpu_evict_enough_for_new_item(texture_manager,
                                                      image_data_size);
        if (evict_result == CacheEvictionResult::kFailed) {
            MMSOLVER_MAYA_WRN(
                "mmsolver::ImageCache::gpu_insert_item: evicting memory "
                "failed!");
            ImageCache::print_cache_brief();
        }

        const bool allocate_ok = texture_data.allocate_texture(
            texture_manager, image_pixel_data.pixel_data(),
            image_pixel_data.width(), image_pixel_data.height(),
            image_pixel_data.num_channels(),
            image_pixel_data.pixel_data_type());
        if (!allocate_ok) {
            MMSOLVER_MAYA_ERR(
                "mmsolver::ImageCache: gpu_insert_item: "
                "Could not allocate texture!");
        }

        if (!texture_data.is_valid()) {
            return GPUCacheValue();
        }

        m_gpu_used_bytes += texture_data.byte_count();
        assert(m_gpu_used_bytes <= m_gpu_capacity_bytes);

        // Make 'key' the most-recently-used key, because when we
        // insert an item into the cache, it's used most recently.
        ImageCache::GPUKeyListIt key_iterator =
            m_gpu_key_list.insert(m_gpu_key_list.end(), key);

        // Create the key-value entry, linked to the usage record.
        const auto item_map_pair = m_gpu_item_map.insert(
            std::make_pair(key, std::make_pair(key_iterator, texture_data)));

        const auto inserted_key_iterator = item_map_pair.first;
        const bool item_ok = item_map_pair.second;
        assert(ok == true);

        const bool group_ok =
            ImageCache::gpu_insert_group(group_key, group_name, file_path);
        assert(group_ok == true);

    } else {
        ImageCache::GPUKeyListIt iterator = search->second.first;
        texture_data = search->second.second;
        if (!texture_data.is_valid()) {
            MMSOLVER_MAYA_ERR(
                "mmsolver::ImageCache: gpu_insert_item: "
                "Found texture is invalid!");
            return ImageCache::GPUCacheValue();
        }

        move_iterator_to_back_of_key_list(m_gpu_key_list, iterator);

        update_texture(texture_data.texture(), image_pixel_data);
    }

    return texture_data;
}

bool ImageCache::cpu_insert_item(const CPUCacheString &group_name,
                                 const CPUCacheString &file_path,
                                 const CPUCacheValue &image_pixel_data) {
    const bool verbose = false;

    const CPUGroupKey key = mmsolver::hash::make_hash(file_path);
    const CPUGroupKey group_key = mmsolver::hash::make_hash(group_name);

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_insert_item: "
                      << "key=" << key << " group_name=" << group_name.c_str()
                      << " file_path=" << file_path.c_str());

    const CPUMapIt item_search = m_cpu_item_map.find(key);
    const bool item_found = item_search != m_cpu_item_map.end();
    if (item_found) {
        ImageCache::cpu_erase_item(key);
    }

    // If we are at capacity, make room for new entry.
    const size_t image_data_size = image_pixel_data.byte_count();
    const CacheEvictionResult evict_result =
        ImageCache::cpu_evict_enough_for_new_item(image_data_size);
    if (evict_result == CacheEvictionResult::kFailed) {
        MMSOLVER_MAYA_WRN(
            "mmsolver::ImageCache::cpu_insert_item: evicting memory failed!");
        ImageCache::print_cache_brief();
    }

    m_cpu_used_bytes += image_data_size;
    assert(m_cpu_used_bytes <= m_cpu_capacity_bytes);

    // Because we are inserting into the cache, the 'key' is the
    // most-recently-used item.
    CPUKeyListIt key_iterator =
        m_cpu_key_list.insert(m_cpu_key_list.end(), key);

    const auto item_map_pair = m_cpu_item_map.insert(
        std::make_pair(key, std::make_pair(key_iterator, image_pixel_data)));

    const auto inserted_key_iterator = item_map_pair.first;
    const bool item_map_ok = item_map_pair.second;
    assert(item_map_ok == true);

    const bool group_ok =
        ImageCache::cpu_insert_group(group_key, group_name, file_path);
    assert(group_ok == true);

    return true;
}

ImageCache::GPUCacheValue ImageCache::gpu_find_item(
    const GPUCacheString &file_path) {
    const bool verbose = false;

    const GPUGroupKey key = mmsolver::hash::make_hash(file_path);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_find_item: "
                      << "key=" << key << "file_path=\"" << file_path.c_str()
                      << "\"");
    return ImageCache::gpu_find_item(key);
}

ImageCache::GPUCacheValue ImageCache::gpu_find_item(const GPUCacheKey key) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_find_item: "
                      << "key=" << key);

    const GPUMapIt search = m_gpu_item_map.find(key);
    const bool found = search != m_gpu_item_map.end();
    if (found) {
        GPUKeyListIt iterator = search->second.first;
        GPUCacheValue value = search->second.second;
        move_iterator_to_back_of_key_list(m_gpu_key_list, iterator);
        return value;
    }
    return GPUCacheValue();
}

ImageCache::CPUCacheValue ImageCache::cpu_find_item(
    const CPUCacheString &file_path) {
    const bool verbose = false;

    const CPUGroupKey key = mmsolver::hash::make_hash(file_path);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_find_item: "
                      << "key=" << key << "file_path=\"" << file_path.c_str()
                      << "\"");
    return ImageCache::cpu_find_item(key);
}

ImageCache::CPUCacheValue ImageCache::cpu_find_item(const CPUCacheKey key) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_find_item: "
                      << "key=" << key);

    const CPUMapIt search = m_cpu_item_map.find(key);
    const bool found = search != m_cpu_item_map.end();
    if (found) {
        CPUKeyListIt iterator = search->second.first;
        CPUCacheValue value = search->second.second;
        move_iterator_to_back_of_key_list(m_cpu_key_list, iterator);
        return value;
    }
    return CPUCacheValue();
}

CacheEvictionResult ImageCache::gpu_evict_one_item(
    MHWRender::MTextureManager *texture_manager) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_evict_one_item: ");

    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache::gpu_evict_one_item: "
        "before m_gpu_used_bytes="
        << m_gpu_used_bytes);

    assert(texture_manager != nullptr);
    if (m_gpu_key_list.empty() ||
        (m_gpu_item_map.size() <= m_gpu_item_count_minumum)) {
        return CacheEvictionResult::kNotNeeded;
    }

    const GPUCacheKey lru_key = m_gpu_key_list.front();
    const GPUMapIt lru_key_iterator = m_gpu_item_map.find(lru_key);
    assert(lru_key_iterator != m_gpu_item_map.end());

    GPUCacheValue texture_data = lru_key_iterator->second.second;
    m_gpu_used_bytes -= texture_data.byte_count();
    texture_data.deallocate_texture(texture_manager);

    m_gpu_item_map.erase(lru_key_iterator);
    m_gpu_key_list.pop_front();

    // TODO: Remove from group maps.

    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache::gpu_evict_one_item: "
        "after m_gpu_used_bytes="
        << m_gpu_used_bytes);
    return CacheEvictionResult::kSuccess;
}

CacheEvictionResult ImageCache::cpu_evict_one_item() {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_evict_one_item: ");

    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache::cpu_evict_one_item: "
        "before m_cpu_used_bytes="
        << m_cpu_used_bytes);

    if (m_cpu_key_list.empty() ||
        (m_cpu_item_map.size() <= m_cpu_item_count_minumum)) {
        return CacheEvictionResult::kNotNeeded;
    }

    const CPUCacheKey lru_key = m_cpu_key_list.front();
    const CPUMapIt lru_key_iterator = m_cpu_item_map.find(lru_key);
    assert(lru_key_iterator != m_cpu_item_map.end());

    CPUCacheValue image_pixel_data = lru_key_iterator->second.second;
    m_cpu_used_bytes -= image_pixel_data.byte_count();
    image_pixel_data.deallocate_pixels();

    m_cpu_item_map.erase(lru_key_iterator);
    m_cpu_key_list.pop_front();

    // TODO: Remove from group maps.

    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache::cpu_evict_one_item: "
        "after m_cpu_used_bytes="
        << m_cpu_used_bytes);
    return CacheEvictionResult::kSuccess;
}

CacheEvictionResult ImageCache::gpu_evict_enough_for_new_item(
    MHWRender::MTextureManager *texture_manager,
    const size_t new_memory_chunk_size) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_evict_enough_for_new_item: ");

    if (m_gpu_key_list.empty() ||
        (m_gpu_item_map.size() <= m_gpu_item_count_minumum)) {
        return CacheEvictionResult::kNotNeeded;
    }

    CacheEvictionResult result = CacheEvictionResult::kSuccess;
    // If we are at capacity remove the least recently used items
    // until we have enough room to store 'new_memory_chunk_size'.
    size_t new_used_bytes = m_gpu_used_bytes + new_memory_chunk_size;
    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache::gpu_evict_enough_for_new_item: "
        "new_used_bytes="
        << new_used_bytes);
    while (!m_gpu_item_map.empty() &&
           (m_gpu_item_map.size() > m_gpu_item_count_minumum) &&
           (new_used_bytes > m_gpu_capacity_bytes)) {
        const CacheEvictionResult evict_result =
            ImageCache::gpu_evict_one_item(texture_manager);
        if (evict_result != CacheEvictionResult::kSuccess) {
            result = evict_result;
            break;
        }
        new_used_bytes = m_gpu_used_bytes + new_memory_chunk_size;
        MMSOLVER_MAYA_VRB(
            "mmsolver::ImageCache::gpu_evict_enough_for_new_item: "
            "new_used_bytes="
            << new_used_bytes);
    }

    return result;
}

CacheEvictionResult ImageCache::cpu_evict_enough_for_new_item(
    const size_t new_memory_chunk_size) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_evict_enough_for_new_item: ");

    if (m_cpu_key_list.empty() ||
        (m_cpu_item_map.size() <= m_cpu_item_count_minumum)) {
        return CacheEvictionResult::kNotNeeded;
    }

    CacheEvictionResult result = CacheEvictionResult::kSuccess;
    // If we are at capacity remove the least recently used items
    // until we have enough room to store 'new_memory_chunk_size'.
    size_t new_used_bytes = m_cpu_used_bytes + new_memory_chunk_size;
    MMSOLVER_MAYA_VRB(
        "mmsolver::ImageCache::cpu_evict_enough_for_new_item: "
        "new_used_bytes="
        << new_used_bytes);
    while (!m_cpu_item_map.empty() &&
           (m_cpu_item_map.size() > m_cpu_item_count_minumum) &&
           (new_used_bytes > m_cpu_capacity_bytes)) {
        const CacheEvictionResult evict_result =
            ImageCache::cpu_evict_one_item();
        if (evict_result != CacheEvictionResult::kSuccess) {
            result = evict_result;
            break;
        }
        new_used_bytes = m_cpu_used_bytes + new_memory_chunk_size;
        MMSOLVER_MAYA_VRB(
            "mmsolver::ImageCache::cpu_evict_enough_for_new_item: "
            "new_used_bytes="
            << new_used_bytes);
    }

    return result;
}

bool ImageCache::gpu_erase_item(MHWRender::MTextureManager *texture_manager,
                                const GPUCacheString &file_path) {
    const bool verbose = false;
    const GPUGroupKey key = mmsolver::hash::make_hash(file_path);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_erase_item: "
                      << "key=" << key << "file_path=\"" << file_path.c_str()
                      << "\"");
    return ImageCache::gpu_erase_item(texture_manager, key);
}

bool ImageCache::gpu_erase_item(MHWRender::MTextureManager *texture_manager,
                                const GPUCacheKey key) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_erase_item: "
                      << "key=" << key);

    const GPUMapIt search = m_gpu_item_map.find(key);
    const bool found = search != m_gpu_item_map.end();
    if (found) {
        GPUCacheValue texture_data = search->second.second;
        m_gpu_used_bytes -= texture_data.byte_count();
        texture_data.deallocate_texture(texture_manager);

        m_gpu_item_map.erase(search);

        // NOTE: This is a O(n) linear operation, and can be very
        // slow since the list items is spread out in memory.
        m_gpu_key_list.remove(key);

        // TODO: Remove from group maps.
    }
    return found;
}

bool ImageCache::cpu_erase_item(const CPUCacheString &file_path) {
    const bool verbose = false;
    const CPUGroupKey key = mmsolver::hash::make_hash(file_path);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_erase_item: "
                      << "key=" << key << "file_path=\"" << file_path.c_str()
                      << "\"");
    return ImageCache::cpu_erase_item(key);
}

bool ImageCache::cpu_erase_item(const CPUCacheKey key) {
    const bool verbose = false;

    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_erase_item: "
                      << "key=" << key);

    const CPUMapIt search = m_cpu_item_map.find(key);
    const bool found = search != m_cpu_item_map.end();
    if (found) {
        CPUCacheValue image_pixel_data = search->second.second;
        m_cpu_used_bytes -= image_pixel_data.byte_count();
        image_pixel_data.deallocate_pixels();

        m_cpu_item_map.erase(search);

        // NOTE: This is a O(n) linear operation, and can be very
        // slow since the list items is spread out in memory.
        m_cpu_key_list.remove(key);

        // TODO: Remove from group maps.
    }
    return found;
}

size_t ImageCache::gpu_erase_group(MHWRender::MTextureManager *texture_manager,
                                   const GPUCacheString &group_name) {
    const bool verbose = false;

    const GPUGroupKey key = mmsolver::hash::make_hash(group_name);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::gpu_erase_group: "
                      << "key=" << key << "group_name=\"" << group_name.c_str()
                      << "\"");

    const GPUGroupMapIt group_search = m_gpu_group_map.find(key);
    const bool group_found = group_search != m_gpu_group_map.end();
    if (!group_found) {
        MMSOLVER_MAYA_WRN(
            "mmsolver::ImageCache: gpu_erase_group: "
            "Group name \""
            << group_name << "\" not found!");
        return 0;
    }

    size_t count = 0;
    GPUGroupSet &values_set = group_search->second;
    for (auto it = values_set.begin(); it != values_set.end(); it++) {
        GPUCacheString value = *it;
        const bool ok = ImageCache::gpu_erase_item(texture_manager, value);
        count += static_cast<size_t>(ok);
    }

    return count;
}

size_t ImageCache::cpu_erase_group(const CPUCacheString &group_name) {
    const bool verbose = false;

    const CPUGroupKey key = mmsolver::hash::make_hash(group_name);
    MMSOLVER_MAYA_VRB("mmsolver::ImageCache::cpu_erase_group: "
                      << "key=" << key << "group_name=\"" << group_name.c_str()
                      << "\"");

    const CPUGroupMapIt group_search = m_cpu_group_map.find(key);
    const bool group_found = group_search != m_cpu_group_map.end();
    if (!group_found) {
        MMSOLVER_MAYA_WRN(
            "mmsolver::ImageCache: cpu_erase_group: "
            "Group name \""
            << group_name << "\" not found!");
        return 0;
    }

    size_t count = 0;
    CPUGroupSet &values_set = group_search->second;
    for (auto it = values_set.begin(); it != values_set.end(); it++) {
        CPUCacheString value = *it;
        const bool ok = ImageCache::cpu_erase_item(value);
        count += static_cast<size_t>(ok);
    }

    return count;
}

}  // namespace image
}  // namespace mmsolver
