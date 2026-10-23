/**
 * @file           : HarfBuzzTextShaper.cpp
 * @brief          : HarfBuzzTextShaper implementation (W2c).
**/

#include "HarfBuzzTextShaper.hpp"

#include <vector>

#include <hb.h>

namespace atom::text {
namespace {

auto ToDirection(const std::string_view direction) -> hb_direction_t {
    if (direction == "rtl")
        return HB_DIRECTION_RTL;
    if (direction == "ttb")
        return HB_DIRECTION_TTB;
    if (direction == "btt")
        return HB_DIRECTION_BTT;
    return HB_DIRECTION_LTR;
}

auto ToScript(const std::string_view script) -> hb_script_t {
    if (script.size() < 4)
        return HB_SCRIPT_INVALID;
    return hb_script_from_iso15924_tag(HB_TAG(script[0], script[1], script[2], script[3]));
}

auto IsFeatureSeparator(const char c) -> bool {
    return c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Parses a comma/space separated OpenType feature list ("kern=1,liga=0").
auto ParseFeatures(const std::string_view features, std::vector<hb_feature_t>& out) -> void {
    out.clear();
    std::size_t start = 0;
    while (start < features.size()) {
        while (start < features.size() && IsFeatureSeparator(features[start]))
            ++start;
        if (start >= features.size())
            break;
        std::size_t end = start;
        while (end < features.size() && !IsFeatureSeparator(features[end]))
            ++end;
        hb_feature_t feature{};
        if (hb_feature_from_string(features.data() + start, static_cast<int>(end - start), &feature))
            out.push_back(feature);
        start = end;
    }
}

} // namespace

struct HarfBuzzTextShaper::Impl {
        const FreeTypeFontProvider& provider;
};

HarfBuzzTextShaper::HarfBuzzTextShaper(const FreeTypeFontProvider& provider) : impl_(new Impl{provider}) {}

HarfBuzzTextShaper::~HarfBuzzTextShaper() {
    delete impl_;
}

auto HarfBuzzTextShaper::Shape(const FontInstanceId instance, const std::string_view utf8,
                               const ShapeParams& params) -> ShapedRun {
    ShapedRun run{};
    run.font_instance = instance;

    if (!instance.IsValid() || utf8.empty())
        return run;

    const auto bytes = impl_->provider.BytesFor(instance);
    const float size_px = impl_->provider.SizePxFor(instance);
    if (bytes.empty() || size_px <= 0.0f)
        return run;

    // Hand HarfBuzz the real font bytes so cmap/GSUB/GPOS drive shaping; the
    // bytes outlive this call (owned by the provider).
    hb_blob_t* blob = hb_blob_create(reinterpret_cast<const char*>(bytes.data()),
                                     static_cast<unsigned int>(bytes.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    hb_face_t* face = hb_face_create(blob, 0);
    hb_font_t* font = hb_font_create(face);

    // Scale so advances/offsets come out in 26.6 fixed-point pixels, which the
    // extraction below converts to float px.
    const auto scale = static_cast<int>(size_px * 64.0f);
    hb_font_set_ppem(font, static_cast<unsigned int>(size_px), static_cast<unsigned int>(size_px));
    hb_font_set_scale(font, scale, scale);

    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, utf8.data(), static_cast<int>(utf8.size()), 0, static_cast<int>(utf8.size()));
    hb_buffer_set_direction(buffer, ToDirection(params.direction));
    if (const hb_script_t script = ToScript(params.script); script != HB_SCRIPT_INVALID)
        hb_buffer_set_script(buffer, script);
    if (!params.language.empty())
        hb_buffer_set_language(buffer,
                               hb_language_from_string(params.language.data(), static_cast<int>(params.language.size())));

    std::vector<hb_feature_t> features{};
    ParseFeatures(params.features, features);

    hb_shape(font, buffer, features.empty() ? nullptr : features.data(),
             static_cast<unsigned int>(features.size()));

    unsigned int count = 0;
    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &count);
    hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &count);

    run.glyphs.resize(count);
    for (unsigned int i = 0; i < count; ++i) {
        ShapedGlyph& glyph = run.glyphs[i];
        glyph.font_instance = instance;
        glyph.glyph_id = infos[i].codepoint;
        glyph.cluster_byte_offset = infos[i].cluster;
        glyph.x_advance = static_cast<float>(positions[i].x_advance) / 64.0f;
        glyph.y_advance = static_cast<float>(positions[i].y_advance) / 64.0f;
        glyph.x_offset = static_cast<float>(positions[i].x_offset) / 64.0f;
        glyph.y_offset = static_cast<float>(positions[i].y_offset) / 64.0f;
    }

    hb_buffer_destroy(buffer);
    hb_font_destroy(font);
    hb_face_destroy(face);
    hb_blob_destroy(blob);

    return run;
}

} // namespace atom::text
